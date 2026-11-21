#include "chipo_weather_client.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "chipo_weather_config.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char* TAG = "chipo_weather";

namespace {

constexpr size_t kBodyCap = 16 * 1024;  // an Open-Meteo answer for 4 days is about 4 KB

// GET `url` over HTTPS (server certificate checked against the built-in bundle) into buf.
bool HttpGet(const char* url, char* buf, size_t cap) {
    esp_http_client_config_t cfg = {};
    cfg.url = url;
    cfg.timeout_ms = CHIPO_WEATHER_FETCH_TIMEOUT_MS;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.user_agent = "CHIPO-OS (ESP32-S3)";
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;

    bool ok = false;
    if (esp_http_client_open(client, 0) == ESP_OK) {
        esp_http_client_fetch_headers(client);  // its return value is ambiguous for chunked answers
        const int status = esp_http_client_get_status_code(client);
        if (status == 200) {
            size_t total = 0;
            while (total + 1 < cap) {
                const int n = esp_http_client_read(client, buf + total, static_cast<int>(cap - 1 - total));
                if (n <= 0) break;
                total += static_cast<size_t>(n);
            }
            buf[total] = '\0';
            ok = total > 0;
        } else {
            ESP_LOGW(TAG, "HTTP %d from %.40s", status, url);
        }
    } else {
        ESP_LOGW(TAG, "cannot connect to %.40s", url);
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ok;
}

bool ResolveLocation(char* buf, size_t cap, GeoLocation* out) {
    if (CHIPO_WEATHER_CITY[0]) {
        out->lat = CHIPO_WEATHER_LAT;
        out->lon = CHIPO_WEATHER_LON;
        AsciiFold(CHIPO_WEATHER_CITY, out->city, sizeof out->city);
        return true;
    }
    // One fetch at a time (the controller guarantees it), so a plain static cache is safe.
    static GeoLocation cached;
    static bool have = false;
    if (have) {
        *out = cached;
        return true;
    }
    static const char* kProviders[] = {"https://ipapi.co/json/", "https://ipwho.is/"};
    for (const char* url : kProviders) {
        if (HttpGet(url, buf, cap) && ParseGeoIp(buf, &cached)) {
            have = true;
            *out = cached;
            ESP_LOGI(TAG, "location by IP: %s", cached.city);
            return true;
        }
    }
    return false;
}

}  // namespace

bool ChipoWeatherFetch(WeatherData* out) {
    char* buf = static_cast<char*>(heap_caps_malloc(kBodyCap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buf) buf = static_cast<char*>(malloc(kBodyCap));
    if (!buf) {
        ESP_LOGE(TAG, "out of memory");
        return false;
    }
    bool ok = false;
    GeoLocation geo;
    if (ResolveLocation(buf, kBodyCap, &geo)) {
        char url[384];
        BuildForecastUrl(geo.lat, geo.lon, url, sizeof url);
        if (HttpGet(url, buf, kBodyCap)) {
            ok = ParseOpenMeteo(buf, geo.city, CHIPO_WEATHER_HOUR_STEP, out);
            if (!ok) ESP_LOGW(TAG, "unexpected forecast answer");
        }
    }
    free(buf);
    return ok;
}
