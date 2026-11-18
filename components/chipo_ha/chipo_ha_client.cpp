// ESP32 only: HTTP POST to Home Assistant and reading its settings from NVS.
#include "chipo_ha_client.h"

#include <cstring>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

static const char* TAG = "chipo_ha";

bool ChipoHaPost(const HaRequest& req, const std::string& token) {
    esp_http_client_config_t cfg = {};
    cfg.url = req.url.c_str();
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = 6000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;  // https://: certificate checked against the bundle
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;
    const std::string auth = "Bearer " + token;
    esp_http_client_set_header(client, "Authorization", auth.c_str());
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, req.body.c_str(), static_cast<int>(req.body.size()));
    bool ok = false;
    const esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        const int status = esp_http_client_get_status_code(client);
        ok = status >= 200 && status < 300;
        if (!ok) ESP_LOGW(TAG, "Home Assistant answered HTTP %d", status);  // never log the token
    } else {
        ESP_LOGW(TAG, "Home Assistant request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    return ok;
}

namespace {
struct Job {
    std::function<void()> fn;
};
void Trampoline(void* arg) {
    auto* job = static_cast<Job*>(arg);
    job->fn();
    delete job;
    vTaskDelete(nullptr);
}
}  // namespace

void ChipoHaRunAsync(std::function<void()> job) {
    auto* j = new Job{std::move(job)};
    if (xTaskCreate(Trampoline, "chipo_ha", 8192, j, 3, nullptr) != pdPASS) {
        ESP_LOGW(TAG, "could not start the Home Assistant task");
        j->fn();  // runs inline so the pending flag is still resolved (short blocking, last resort)
        delete j;
    }
}

bool ChipoHaLoadConfig(HaConfig* out) {
    nvs_handle_t h;
    if (nvs_open("chipo_ha", NVS_READONLY, &h) != ESP_OK) return false;
    auto read = [&](const char* key, std::string* dst) {
        size_t len = 0;
        if (nvs_get_str(h, key, nullptr, &len) != ESP_OK || len == 0 || len > 512) return;
        std::string tmp(len, '\0');
        if (nvs_get_str(h, key, tmp.data(), &len) == ESP_OK) {
            tmp.resize(std::strlen(tmp.c_str()));
            *dst = tmp;
        }
    };
    read("url", &out->base_url);
    read("token", &out->token);
    read("scene_night", &out->scene_night);
    read("scene_morning", &out->scene_morning);
    nvs_close(h);
    return out->usable();
}
