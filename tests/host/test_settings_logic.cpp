#include <cstdio>

#include "chipo_settings_logic.h"

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

int main() {
    CHECK(SettingsStep(70, 1, 0, 100) == 80 && SettingsStep(100, 1, 0, 100) == 100, "volume up, capped");
    CHECK(SettingsStep(0, -1, 0, 100) == 0 && SettingsStep(10, -1, 5, 100) == 5, "never below the minimum (brightness keeps 5)");

    BatteryView b = ClassifyBattery(false, 50, false, true);
    CHECK(b.state == BatteryState::Unknown && b.percent == -1 && b.text == "Sin lectura", "no reading is not invented");
    CHECK(ClassifyBattery(true, 150, false, true).percent == -1, "out of range level is not a reading");
    CHECK(ClassifyBattery(true, 82, true, false).state == BatteryState::Charging, "charging");
    CHECK(ClassifyBattery(true, 100, true, false).state == BatteryState::Full, "full");
    CHECK(ClassifyBattery(true, 12, false, true).state == BatteryState::Low, "low");
    CHECK(ClassifyBattery(true, 4, false, true).state == BatteryState::Critical, "critical");
    CHECK(ClassifyBattery(true, 4, true, false).state == BatteryState::Charging, "charging wins over low");
    CHECK(ClassifyBattery(true, 60, false, true).state == BatteryState::Normal, "normal");
    CHECK(!BatteryIndicatorVisible(ClassifyBattery(true, 60, false, true)) && BatteryIndicatorVisible(ClassifyBattery(true, 60, true, false)), "indicator only when charging or low");
    CHECK(!BatteryIndicatorVisible(ClassifyBattery(false, 0, false, false)), "no indicator without a reading");

    int t = 0;
    CHECK(BatteryCrossedWarning(16, 15, &t) && t == 15, "15 %% warning");
    CHECK(BatteryCrossedWarning(6, 5, &t) && t == 5, "5 %% warning");
    CHECK(!BatteryCrossedWarning(15, 14, &t), "only once per threshold");
    CHECK(!BatteryCrossedWarning(-1, 10, &t) && !BatteryCrossedWarning(20, -1, &t), "no warning without readings");
    CHECK(!BatteryCrossedWarning(10, 12, &t), "going up never warns");

    CHECK(DimPercent(0) == 100 && DimPercent(89999) == 100, "awake");
    CHECK(DimPercent(90000) == 40 && DimPercent(299999) == 40, "dim after 90 s");
    CHECK(DimPercent(300000) == 15, "darker after 5 min");

    int dx, dy, seen = 0;
    bool visited[9] = {};
    for (uint32_t k = 0; k < 9; ++k) {
        PixelShift(k * 45000u, &dx, &dy);
        CHECK(dx >= -3 && dx <= 3 && dy >= -3 && dy <= 3, "shift within 3 px");
        for (int i = 0; i < 9; ++i) {
            static const int pat[9][2] = {{0, 0}, {3, 0}, {3, 3}, {0, 3}, {-3, 3}, {-3, 0}, {-3, -3}, {0, -3}, {3, -3}};
            if (pat[i][0] == dx && pat[i][1] == dy) visited[i] = true;
        }
    }
    for (bool v : visited) seen += v;
    CHECK(seen == 9, "all nine positions are used (%d)", seen);
    int ax, ay, bx, by;
    PixelShift(10000, &ax, &ay);
    PixelShift(10001, &bx, &by);
    CHECK(ax == bx && ay == by, "stays put between changes");

    WifiView w;
    CHECK(WifiText(w) == "Sin Wi-Fi", "offline");
    w.connected = true; w.ssid = "Casa"; w.rssi_dbm = -50;
    CHECK(WifiText(w) == "Casa - buena", "good: %s", WifiText(w).c_str());
    w.rssi_dbm = -70;
    CHECK(WifiText(w).find("normal") != std::string::npos, "normal");
    w.rssi_dbm = -85; w.ssid.clear();
    CHECK(WifiText(w) == "Conectado - debil", "weak, no name: %s", WifiText(w).c_str());

    CHECK(InFaceBand(200) && !InFaceBand(60) && !InFaceBand(400), "face band");
    TapTracker tr;
    CHECK(tr.OnTap(1000) == TapKind::Poke, "one tap pokes");
    CHECK(tr.OnTap(1500) == TapKind::Poke, "two taps still poke");
    CHECK(tr.OnTap(2000) == TapKind::Tickle, "three within 1.5 s tickle");
    CHECK(tr.OnTap(2100) == TapKind::Poke, "counter restarts after a tickle");
    TapTracker slow;
    slow.OnTap(0); slow.OnTap(1000);
    CHECK(slow.OnTap(2600) == TapKind::Poke, "three slow taps do not tickle");

    if (g_fail == 0) std::printf("ALL SETTINGS LOGIC TESTS PASSED\n");
    return g_fail == 0 ? 0 : 1;
}
