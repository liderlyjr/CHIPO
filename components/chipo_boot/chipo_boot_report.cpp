#include "chipo_boot_report.h"

#include <cstdio>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_system.h"
#endif

std::vector<std::string> ChipoFormatBootReport(const ChipoBootInfo& info, const std::vector<ChipoModuleStatus>& modules) {
    std::vector<std::string> lines;
    char buf[160];

    std::snprintf(buf, sizeof(buf), "CHIPO OS %s | ESP-IDF %s | reset: %s", info.version, info.idf_version, info.reset_reason);
    lines.emplace_back(buf);

    if (info.has_psram) {
        std::snprintf(buf, sizeof(buf), "Memory free: internal %u KB (lowest since boot %u KB), PSRAM %u KB",
                      static_cast<unsigned>(info.free_internal_kb), static_cast<unsigned>(info.min_internal_kb),
                      static_cast<unsigned>(info.free_psram_kb));
    } else {
        std::snprintf(buf, sizeof(buf), "Memory free: internal %u KB (lowest since boot %u KB), NO PSRAM DETECTED",
                      static_cast<unsigned>(info.free_internal_kb), static_cast<unsigned>(info.min_internal_kb));
    }
    lines.emplace_back(buf);

    std::string started, failed;
    int n_started = 0, n_failed = 0;
    for (const auto& m : modules) {
        std::string& list = m.started ? started : failed;
        (m.started ? n_started : n_failed)++;
        if (!list.empty()) list += ' ';
        list += m.name;
    }
    std::snprintf(buf, sizeof(buf), "Modules started (%d/%d):", n_started, n_started + n_failed);
    lines.emplace_back(std::string(buf) + (started.empty() ? " none" : " " + started));
    if (n_failed > 0) {
        std::snprintf(buf, sizeof(buf), "Modules FAILED (%d):", n_failed);
        lines.emplace_back(std::string(buf) + " " + failed);
    }
    return lines;
}

#ifdef ESP_PLATFORM
namespace {
constexpr char TAG[] = "CHIPO";

const char* ResetReasonName(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON: return "power-on";
        case ESP_RST_EXT: return "external pin";
        case ESP_RST_SW: return "software restart";
        case ESP_RST_PANIC: return "PANIC (crash)";
        case ESP_RST_INT_WDT: return "interrupt watchdog";
        case ESP_RST_TASK_WDT: return "task watchdog";
        case ESP_RST_WDT: return "other watchdog";
        case ESP_RST_DEEPSLEEP: return "deep sleep wake-up";
        case ESP_RST_BROWNOUT: return "BROWNOUT (low voltage)";
        case ESP_RST_SDIO: return "SDIO";
        default: return "unknown";
    }
}
}  // namespace

void ChipoLogBootReport(const char* chipo_version, const std::vector<ChipoModuleStatus>& modules) {
    ChipoBootInfo info;
    info.version = chipo_version;
    info.idf_version = esp_get_idf_version();
    info.reset_reason = ResetReasonName(esp_reset_reason());
    info.free_internal_kb = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
    info.min_internal_kb = static_cast<uint32_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024);
    info.free_psram_kb = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
    info.has_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0;

    ESP_LOGI(TAG, "================ boot summary ================");
    for (const auto& line : ChipoFormatBootReport(info, modules)) {
        const bool bad = line.find("FAILED") != std::string::npos || line.find("NO PSRAM") != std::string::npos ||
                         line.find("PANIC") != std::string::npos || line.find("BROWNOUT") != std::string::npos;
        if (bad) {
            ESP_LOGW(TAG, "%s", line.c_str());
        } else {
            ESP_LOGI(TAG, "%s", line.c_str());
        }
    }
    ESP_LOGI(TAG, "==============================================");
}
#endif
