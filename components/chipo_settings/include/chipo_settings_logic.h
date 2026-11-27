#pragma once
// CHIPO SETTINGS: pure logic (no LVGL, no ESP-IDF), so it is tested on the PC.
#include <cstdint>
#include <string>

// Volume and brightness move in steps of 10 %. Brightness never goes below 5 % so the screen cannot be lost.
int SettingsStep(int value, int delta_steps, int min_percent, int max_percent);

enum class BatteryState : uint8_t { Unknown, Critical, Low, Normal, Charging, Full };
struct BatteryView {
    BatteryState state = BatteryState::Unknown;
    int percent = -1;      // -1 when there is no valid reading: never invented
    std::string text;      // "82 %  cargando", "Sin lectura"...
};
// valid=false when the fuel gauge gave no reading (no battery connected, driver error).
BatteryView ClassifyBattery(bool valid, int level, bool charging, bool discharging);
// How often the face shows a tired look while the battery is low: 0 = never (normal, charging, full, unknown).
uint32_t TiredFaceIntervalMs(BatteryState state);
// True when the level just crossed 15 % or 5 % going down (one warning per threshold).
bool BatteryCrossedWarning(int previous_percent, int percent, int* threshold);
bool BatteryIndicatorVisible(const BatteryView& v);  // only charging or low: otherwise stay out of the way

// Screen saver: dim after a while without activity (AMOLED burn-in and battery).
int DimPercent(uint32_t idle_ms);
// Burn-in protection: the whole face drifts a few pixels every 45 s (9 positions).
void PixelShift(uint32_t uptime_ms, int* dx, int* dy);

struct WifiView {
    bool connected = false;
    int rssi_dbm = 0;
    std::string ssid;
};
std::string WifiText(const WifiView& w);  // "Casa - buena" / "Sin Wi-Fi"

// Taps on the face: one pokes it (wink), three within 1.5 s tickle it (laugh).
enum class TapKind : uint8_t { None, Poke, Tickle };
bool InFaceBand(int y);  // taps on the eyes/mouth area belong to the face, not to the weather screen
class TapTracker {
public:
    TapKind OnTap(uint32_t now_ms);
private:
    uint32_t times_[3] = {0, 0, 0};
    int count_ = 0;
};
bool ChipoSettingsWifi(WifiView* out);  // ESP only
void ChipoSettingsEnterWifiConfigMode();  // ESP only: starts the board's native Wi-Fi provisioning flow
