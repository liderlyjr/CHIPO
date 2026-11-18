#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "chipo_settings_logic.h"
#include "lvgl.h"

/**
 * CHIPO SETTINGS (neon AMOLED):
 *   - touch menu, opened with a long press anywhere: volume, brightness, battery, Wi-Fi, info, restart
 *   - battery indicator (only while charging or low) and one warning at 15 % and 5 %
 *   - screen saver: dims the screen after a while without activity (AMOLED burn-in, battery)
 *   - burn-in protection: the face drifts a few pixels every 45 s
 *   - face touch reactions: a tap on the face winks, three quick taps tickle it
 * Everything the board knows (volume, backlight, fuel gauge, Wi-Fi) comes in through hooks.
 * All methods run on the LVGL task with the display lock held.
 */
class ChipoSettings {
public:
    struct Hooks {
        std::function<int()> get_volume;               // 0..100
        std::function<void(int)> set_volume;
        std::function<int()> get_brightness;           // 0..100, the user's level
        std::function<void(int percent, bool permanent)> set_brightness;  // permanent: the user's own choice; not the saver
        std::function<bool(int* level, bool* charging, bool* discharging)> battery;  // false: no reading
        std::function<bool(WifiView*)> wifi;
        std::function<void()> restart;
        std::function<void(const char*)> notify;
        std::function<bool(const char*)> play_clip;
        std::function<void(TapKind)> react;            // the face reacts to a poke or a tickle
        std::function<void(int dx, int dy)> shift;     // burn-in drift of the face
        std::function<bool()> keep_awake;              // something is on screen or being said: do not dim
        std::function<bool()> overlay_active;          // weather, clock, dice... on screen
        std::function<uint32_t()> clock;               // ms, defaults to lv_tick_get
    };

    enum Action : int { kVolDown = 0, kVolUp, kBriDown, kBriUp, kRestart, kClose, kActionCount };

    ChipoSettings();
    ~ChipoSettings();
    ChipoSettings(const ChipoSettings&) = delete;
    ChipoSettings& operator=(const ChipoSettings&) = delete;

    bool Start(lv_obj_t* parent, Hooks hooks, const char* version = "", const lv_font_t* font = nullptr);
    void Stop();

    void Open();
    void Close();
    bool open() const { return open_; }

    // Touch input (the indev callbacks call these; tests call them directly).
    void OnPressed();                   // finger down: counts as activity
    void OnLongPress();                 // opens the menu
    void OnTap(int x, int y);           // a tap that was not a long press
    bool BlocksWeatherTap(int y) const; // the weather screen must not open for this tap
    void Press(int action);             // a menu button

    void Step(uint32_t now_ms);         // 1 s timer, public so tests can drive time
    void NotifyActivity();              // wakes the screen (touch, voice, state changes)

    struct DebugInfo {
        bool open;
        int volume, brightness;
        bool dimmed;
        int dim_percent;
        bool indicator_visible;
        BatteryState battery_state;
        int shift_x, shift_y;
        bool restart_armed;
    };
    DebugInfo Debug() const;

private:
    static void TimerCallback(lv_timer_t* timer);
    static void IndevCallback(lv_event_t* e);
    static void ButtonCallback(lv_event_t* e);
    uint32_t Now() const;
    void Build(const lv_font_t* font, const char* version);
    void Refresh();
    void UpdateBattery(uint32_t now_ms);
    void UpdateSaver(uint32_t now_ms);
    void ApplyBrightness(int user_percent, int dim_percent);

    struct Btn { ChipoSettings* self; int action; };

    Hooks hooks_;
    lv_timer_t* timer_ = nullptr;
    lv_obj_t* menu_ = nullptr;
    lv_obj_t* bar_vol_ = nullptr;
    lv_obj_t* bar_bri_ = nullptr;
    lv_obj_t* val_vol_ = nullptr;
    lv_obj_t* val_bri_ = nullptr;
    lv_obj_t* info_bat_ = nullptr;
    lv_obj_t* info_wifi_ = nullptr;
    lv_obj_t* info_ver_ = nullptr;
    lv_obj_t* restart_label_ = nullptr;
    lv_obj_t* indicator_ = nullptr;
    lv_obj_t* indicator_fill_ = nullptr;
    lv_obj_t* indicator_text_ = nullptr;
    Btn btn_[kActionCount] = {};
    bool running_ = false;
    bool open_ = false;
    int volume_ = 70, brightness_ = 80;
    uint32_t last_activity_ms_ = 0, opened_ms_ = 0, restart_armed_until_ms_ = 0;
    uint32_t last_battery_ms_ = 0;
    bool dimmed_ = false;
    int dim_percent_ = 100, user_brightness_ = 80;
    int shift_x_ = 0, shift_y_ = 0;
    BatteryView battery_;
    int last_level_ = -1;
    bool indicator_visible_ = false;
    TapTracker taps_;
    uint32_t start_ms_ = 0;
};
