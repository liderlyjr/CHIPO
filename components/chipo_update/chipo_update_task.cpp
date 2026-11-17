// ESP-only part: asks GitHub for the latest Release and hands the flash to the application.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

#include "chipo_update.h"
#include "chipo_update_config.h"
#include "chipo_version.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "chipo_update";

namespace {

constexpr size_t kApiBytes = 4096;  // "tag_name" sits near the top of the answer
constexpr size_t kLocationBytes = 2048;

struct HeaderCapture {
    char location[kLocationBytes];
};

esp_err_t OnHttpEvent(esp_http_client_event_t* evt) {
    if (evt->event_id == HTTP_EVENT_ON_HEADER && evt->user_data && evt->header_key && evt->header_value &&
        strcasecmp(evt->header_key, "Location") == 0) {
        auto* cap = static_cast<HeaderCapture*>(evt->user_data);
        snprintf(cap->location, sizeof cap->location, "%s", evt->header_value);
    }
    return ESP_OK;
}

void FillCommon(esp_http_client_config_t* cfg) {
    cfg->timeout_ms = CHIPO_UPDATE_HTTP_TIMEOUT_MS;
    cfg->crt_bundle_attach = esp_crt_bundle_attach;  // server certificate checked against the bundle
    cfg->user_agent = "CHIPO-OS/" CHIPO_OS_VERSION;
    cfg->buffer_size = 4096;  // GitHub redirect headers are long
    cfg->buffer_size_tx = 1024;
}

bool FetchLatestTag(char* tag, size_t cap) {
    char url[160];
    snprintf(url, sizeof url, "https://api.github.com/repos/%s/releases/latest", CHIPO_UPDATE_REPO);
    esp_http_client_config_t cfg = {};
    cfg.url = url;
    FillCommon(&cfg);
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;
    esp_http_client_set_header(client, "Accept", "application/vnd.github+json");

    char* body = static_cast<char*>(calloc(1, kApiBytes));
    bool ok = false;
    if (body && esp_http_client_open(client, 0) == ESP_OK) {
        esp_http_client_fetch_headers(client);
        const int status = esp_http_client_get_status_code(client);
        if (status == 200) {
            size_t total = 0;
            while (total + 1 < kApiBytes) {  // the first 4 KB are enough, the rest is dropped on close
                const int n = esp_http_client_read(client, body + total, static_cast<int>(kApiBytes - 1 - total));
                if (n <= 0) break;
                total += static_cast<size_t>(n);
            }
            body[total] = '\0';
            ok = ChipoParseTagName(body, tag, cap);
        } else {
            ESP_LOGW(TAG, "GitHub answered HTTP %d (404 = no Release published yet)", status);
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(body);
    return ok;
}

// github.com/.../releases/download/... answers 302 to a signed CDN address; we read it ourselves
// so the address that gets flashed is one we have validated.
bool ResolveDownload(const char* release_url, char* out, size_t cap) {
    if (!ChipoIsTrustedReleaseUrl(release_url)) return false;
    auto* capture = static_cast<HeaderCapture*>(calloc(1, sizeof(HeaderCapture)));
    if (!capture) return false;
    esp_http_client_config_t cfg = {};
    cfg.url = release_url;
    cfg.disable_auto_redirect = true;
    cfg.event_handler = OnHttpEvent;
    cfg.user_data = capture;
    FillCommon(&cfg);
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    bool ok = false;
    if (client) {
        if (esp_http_client_open(client, 0) == ESP_OK) {
            esp_http_client_fetch_headers(client);
            const int status = esp_http_client_get_status_code(client);
            if ((status == 301 || status == 302 || status == 303 || status == 307 || status == 308) &&
                ChipoIsTrustedRedirectUrl(capture->location) && strlen(capture->location) < cap) {
                snprintf(out, cap, "%s", capture->location);
                ok = true;
            } else {
                ESP_LOGW(TAG, "release file not available (HTTP %d)", status);
            }
        }
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    free(capture);
    return ok;
}

void UpdaterTask(void* arg) {
    auto* hooks = static_cast<ChipoUpdaterHooks*>(arg);
    char failed_tag[32] = "";
    int tries = 0;
    vTaskDelay(pdMS_TO_TICKS(CHIPO_UPDATE_FIRST_CHECK_MS));
    for (;;) {
        char tag[32];
        if (FetchLatestTag(tag, sizeof tag)) {
            if (!ChipoIsNewer(CHIPO_OS_VERSION, tag)) {
                ESP_LOGI(TAG, "up to date (%s, latest release %s)", CHIPO_OS_VERSION, tag);
            } else if (strcmp(failed_tag, tag) == 0 && tries >= CHIPO_UPDATE_MAX_TRIES) {
                ESP_LOGW(TAG, "giving up on %s until the next boot", tag);
            } else {
                ESP_LOGI(TAG, "new release %s available (running %s)", tag, CHIPO_OS_VERSION);
#if CHIPO_UPDATE_AUTO_INSTALL
                // Wait until nobody is talking to CHIPO: the flash ends with a reboot.
                uint32_t waited = 0;
                while (!hooks->is_idle() && waited < CHIPO_UPDATE_IDLE_WAIT_MS) {
                    vTaskDelay(pdMS_TO_TICKS(10000));
                    waited += 10000;
                }
                if (hooks->is_idle()) {
                    char release_url[200];
                    char* cdn_url = static_cast<char*>(calloc(1, kLocationBytes));
                    if (cdn_url && ChipoBuildDownloadUrl(tag, release_url, sizeof release_url) &&
                        ResolveDownload(release_url, cdn_url, kLocationBytes)) {
                        if (strcmp(failed_tag, tag) != 0) {
                            snprintf(failed_tag, sizeof failed_tag, "%s", tag);
                            tries = 0;
                        }
                        ++tries;  // counted before the flash: a successful flash reboots
                        ChipoUpdateAuthorize(cdn_url);
                        hooks->install(cdn_url, tag);
                    }
                    free(cdn_url);
                }
#endif
            }
        }
        vTaskDelay(pdMS_TO_TICKS(CHIPO_UPDATE_INTERVAL_MS));
    }
}

}  // namespace

bool ChipoUpdaterStart(ChipoUpdaterHooks hooks) {
#if !CHIPO_UPDATE_ENABLED
    (void)hooks;
    return false;
#else
    if (!hooks.is_idle || !hooks.install) return false;
    auto* heap_hooks = new ChipoUpdaterHooks(std::move(hooks));  // lives as long as the task
    if (xTaskCreate(UpdaterTask, "chipo_update", 8192, heap_hooks, 2, nullptr) != pdPASS) {
        delete heap_hooks;
        return false;
    }
    return true;
#endif
}
