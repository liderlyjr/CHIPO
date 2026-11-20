#include "chipo_settings_logic.h"

#include <algorithm>
#include <cstdio>

int SettingsStep(int value, int delta_steps, int min_percent, int max_percent) {
    int v = value + delta_steps * 10;
    v = std::max(min_percent, std::min(max_percent, v));
    return v;
}

BatteryView ClassifyBattery(bool valid, int level, bool charging, bool discharging) {
    BatteryView v;
    if (!valid || level < 0 || level > 100) {
        v.text = "Sin lectura";
        return v;
    }
    v.percent = level;
    char buf[48];
    if (charging && level >= 100) {
        v.state = BatteryState::Full;
        std::snprintf(buf, sizeof(buf), "%d %%  completa", level);
    } else if (charging) {
        v.state = BatteryState::Charging;
        std::snprintf(buf, sizeof(buf), "%d %%  cargando", level);
    } else if (level <= 5) {
        v.state = BatteryState::Critical;
        std::snprintf(buf, sizeof(buf), "%d %%  casi sin bateria", level);
    } else if (level <= 15) {
        v.state = BatteryState::Low;
        std::snprintf(buf, sizeof(buf), "%d %%  bateria baja", level);
    } else {
        v.state = BatteryState::Normal;
        std::snprintf(buf, sizeof(buf), discharging ? "%d %%" : "%d %%  con corriente", level);
    }
    v.text = buf;
    return v;
}

bool BatteryCrossedWarning(int previous_percent, int percent, int* threshold) {
    if (previous_percent < 0 || percent < 0) return false;
    for (int t : {5, 15}) {
        if (previous_percent > t && percent <= t) {
            if (threshold) *threshold = t;
            return true;
        }
    }
    return false;
}

uint32_t TiredFaceIntervalMs(BatteryState state) {
    switch (state) {
    case BatteryState::Low: return 120000;
    case BatteryState::Critical: return 45000;
    default: return 0;
    }
}

bool BatteryIndicatorVisible(const BatteryView& v) {
    return v.state == BatteryState::Charging || v.state == BatteryState::Low || v.state == BatteryState::Critical;
}

int DimPercent(uint32_t idle_ms) {
    if (idle_ms < 90000u) return 100;
    if (idle_ms < 300000u) return 40;
    return 15;
}

void PixelShift(uint32_t uptime_ms, int* dx, int* dy) {
    static const int kPattern[9][2] = {{0, 0}, {3, 0}, {3, 3}, {0, 3}, {-3, 3}, {-3, 0}, {-3, -3}, {0, -3}, {3, -3}};
    const uint32_t i = (uptime_ms / 45000u) % 9u;
    if (dx) *dx = kPattern[i][0];
    if (dy) *dy = kPattern[i][1];
}

std::string WifiText(const WifiView& w) {
    if (!w.connected) return "Sin Wi-Fi";
    const char* quality = w.rssi_dbm >= -60 ? "buena" : (w.rssi_dbm >= -75 ? "normal" : "debil");
    std::string out = w.ssid.empty() ? "Conectado" : w.ssid;  // the network's name when known (it fits better)
    out += " - ";
    out += quality;
    return out;
}

bool InFaceBand(int y) { return y >= 110 && y <= 310; }

TapKind TapTracker::OnTap(uint32_t now_ms) {
    // keep the last three taps
    if (count_ == 3) { times_[0] = times_[1]; times_[1] = times_[2]; count_ = 2; }
    times_[count_++] = now_ms;
    if (count_ == 3 && times_[2] - times_[0] <= 1500u) {
        count_ = 0;
        return TapKind::Tickle;
    }
    return TapKind::Poke;
}
