#pragma once
// Boot summary for the serial log. Reading hundreds of lines to find out which CHIPO module did not start is slow:
// right after the last module has started, CHIPO prints this short block.
//
//   CHIPO OS 0.6.0 | ESP-IDF v6.1 | reset: power-on
//   Memory free: internal 210 KB (lowest since boot 190 KB), PSRAM 7650 KB
//   Modules started (11/12): face weather drive ha fun pet remind ambient buddy scenes settings
//   Modules FAILED (1): tools
#include <cstdint>
#include <string>
#include <vector>

struct ChipoModuleStatus {
    const char* name;
    bool started;
};

struct ChipoBootInfo {
    const char* version = "";      // CHIPO OS version
    const char* idf_version = "";  // ESP-IDF version
    const char* reset_reason = "";
    uint32_t free_internal_kb = 0;
    uint32_t min_internal_kb = 0;  // lowest free internal memory since boot
    uint32_t free_psram_kb = 0;
    bool has_psram = true;
};

// The lines of the summary (no log prefix). Pure: tested on a PC.
std::vector<std::string> ChipoFormatBootReport(const ChipoBootInfo& info, const std::vector<ChipoModuleStatus>& modules);

#ifdef ESP_PLATFORM
// Reads the version of ESP-IDF, the reset reason and the free memory, and prints the summary at INFO level (WARN
// for the failed modules, so they stand out).
void ChipoLogBootReport(const char* chipo_version, const std::vector<ChipoModuleStatus>& modules);
#endif
