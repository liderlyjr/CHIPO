#include "chipo_weather.h"

#include <cstdio>
#include <mutex>
#include <string>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr char TAG[] = "ChipoWeather";
// Default forecast location: Sabadell, Barcelona. Open-Meteo requires no API key.
constexpr char kWeatherUrl[] =
    "https://api.open-meteo.com/v1/forecast?latitude=41.5486&longitude=2.1074"
    "&current=temperature_2m,weather_code&timezone=Europe%2FMadrid";
constexpr uint32_t kRefreshMs = 15 * 60 * 1000;
constexpr uint32_t kRetryMs = 2 * 60 * 1000;

esp_err_t HttpEventHandler(esp_http_client_event_t* event) {
    if (event == nullptr || event->user_data == nullptr) return ESP_OK;
    if (event->event_id == HTTP_EVENT_ON_DATA && event->data != nullptr && event->data_len > 0) {
        auto* response = static_cast<std::string*>(event->user_data);
        response->append(static_cast<const char*>(event->data),
                         static_cast<size_t>(event->data_len));
    }
    return ESP_OK;
}

const char* ConditionName(int code) {
    if (code == 0) return "SUN";
    if (code == 1 || code == 2) return "PARTLY";
    if (code == 3) return "CLOUD";
    if (code == 45 || code == 48) return "FOG";
    if (code >= 51 && code <= 57) return "DRIZZLE";
    if (code >= 61 && code <= 67) return "RAIN";
    if (code >= 71 && code <= 77) return "SNOW";
    if (code >= 80 && code <= 82) return "SHOWERS";
    if (code == 85 || code == 86) return "SNOW";
    if (code == 95 || code == 96 || code == 99) return "STORM";
    return "WEATHER";
}
}

ChipoWeather& ChipoWeather::GetInstance() {
    static ChipoWeather instance;
    return instance;
}

bool ChipoWeather::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_) return true;
    started_ = true;
    if (xTaskCreate(WorkerTask, "chipo_weather", 6144, this, 3, nullptr) != pdPASS) {
        started_ = false;
        ESP_LOGE(TAG, "Could not create weather worker task");
        return false;
    }
    ESP_LOGI(TAG, "Weather worker started (Sabadell)");
    return true;
}

std::string ChipoWeather::GetDisplayText() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return display_text_;
}

void ChipoWeather::WorkerTask(void* arg) {
    auto* self = static_cast<ChipoWeather*>(arg);
    vTaskDelay(pdMS_TO_TICKS(5000)); // Let Wi-Fi finish connecting after boot.
    while (true) {
        const bool success = self->FetchCurrentWeather();
        vTaskDelay(pdMS_TO_TICKS(success ? kRefreshMs : kRetryMs));
    }
}

bool ChipoWeather::FetchCurrentWeather() {
    std::string response;
    esp_http_client_config_t config = {};
    config.url = kWeatherUrl;
    config.timeout_ms = 12000;
    config.event_handler = HttpEventHandler;
    config.user_data = &response;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGW(TAG, "Could not initialize HTTPS client");
        return false;
    }

    const esp_err_t result = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (result != ESP_OK || status != 200) {
        ESP_LOGW(TAG, "Weather request failed: %s (HTTP %d)",
                 esp_err_to_name(result), status);
        return false;
    }

    cJSON* root = cJSON_Parse(response.c_str());
    if (root == nullptr) {
        ESP_LOGW(TAG, "Weather response was not valid JSON");
        return false;
    }
    cJSON* current = cJSON_GetObjectItemCaseSensitive(root, "current");
    cJSON* temperature = current ? cJSON_GetObjectItemCaseSensitive(current, "temperature_2m") : nullptr;
    cJSON* weather_code = current ? cJSON_GetObjectItemCaseSensitive(current, "weather_code") : nullptr;
    if (!cJSON_IsNumber(temperature) || !cJSON_IsNumber(weather_code)) {
        cJSON_Delete(root);
        ESP_LOGW(TAG, "Weather response did not contain current conditions");
        return false;
    }

    char display[48];
    std::snprintf(display, sizeof(display), "SBD %.0fC %s",
                  temperature->valuedouble, ConditionName(weather_code->valueint));
    cJSON_Delete(root);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        display_text_ = display;
    }
    ESP_LOGI(TAG, "Updated weather: %s", display);
    return true;
}
