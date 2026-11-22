// The boot summary is plain text: check what it says in each situation (no ESP-IDF needed).
#include <cstdio>
#include <string>

#include "chipo_boot_report.h"

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); ++g_fail; } } while (0)

static ChipoBootInfo Info() {
    ChipoBootInfo i;
    i.version = "0.6.0";
    i.idf_version = "v6.1";
    i.reset_reason = "power-on";
    i.free_internal_kb = 210;
    i.min_internal_kb = 190;
    i.free_psram_kb = 7650;
    return i;
}

int main() {
    std::printf("== everything started\n");
    {
        auto lines = ChipoFormatBootReport(Info(), {{"face", true}, {"weather", true}, {"tools", true}});
        CHECK(lines.size() == 3, "3 lines when nothing failed (%zu)", lines.size());
        CHECK(lines[0] == "CHIPO OS 0.6.0 | ESP-IDF v6.1 | reset: power-on", "header: %s", lines[0].c_str());
        CHECK(lines[1] == "Memory free: internal 210 KB (lowest since boot 190 KB), PSRAM 7650 KB", "memory: %s", lines[1].c_str());
        CHECK(lines[2] == "Modules started (3/3): face weather tools", "modules: %s", lines[2].c_str());
        for (auto& l : lines) CHECK(l.find("FAILED") == std::string::npos, "no FAILED line: %s", l.c_str());
    }

    std::printf("== some failed\n");
    {
        auto lines = ChipoFormatBootReport(Info(), {{"face", true}, {"weather", false}, {"tools", true}, {"pet", false}});
        CHECK(lines.size() == 4, "4 lines (%zu)", lines.size());
        CHECK(lines[2] == "Modules started (2/4): face tools", "started: %s", lines[2].c_str());
        CHECK(lines[3] == "Modules FAILED (2): weather pet", "failed: %s", lines[3].c_str());
    }

    std::printf("== nothing started, no module list, no PSRAM\n");
    {
        auto info = Info();
        info.has_psram = false;
        auto lines = ChipoFormatBootReport(info, {{"face", false}});
        CHECK(lines[1].find("NO PSRAM DETECTED") != std::string::npos, "PSRAM warning: %s", lines[1].c_str());
        CHECK(lines[2] == "Modules started (0/1): none", "none started: %s", lines[2].c_str());
        CHECK(lines[3] == "Modules FAILED (1): face", "failed: %s", lines[3].c_str());
        auto empty = ChipoFormatBootReport(Info(), {});
        CHECK(empty.size() == 3 && empty[2] == "Modules started (0/0): none", "empty list: %s", empty.back().c_str());
    }

    std::printf("== long text never overflows\n");
    {
        auto info = Info();
        info.version = "1.2.3-a-very-long-version-name-that-goes-on-and-on-and-on-and-on-and-on-and-on-and-on-and-on-and-on";
        auto lines = ChipoFormatBootReport(info, {{"face", true}});
        CHECK(!lines[0].empty() && lines[0].rfind("CHIPO OS 1.2.3", 0) == 0, "header survives a long version");
    }

    if (g_fail == 0) std::printf("ALL BOOT REPORT TESTS PASSED\n");
    return g_fail == 0 ? 0 : 1;
}
