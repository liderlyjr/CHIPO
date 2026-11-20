#pragma once

#include <cstdint>
#include <atomic>
#include <functional>
#include <string>

#include "chipo_fun_voice.h"
#include "lvgl.h"

/**
 * CHIPO FUN: things that make the most of the AMOLED panel (pure black = pixels off, saturated neon colours).
 *   - classic-Mac style start-up screen with a glowing "happy" computer icon
 *   - colour lamp (warm, cool, green, pink, rainbow), by voice, with an automatic switch-off
 *   - dice (one or two) and coin flip with a short animation
 *
 * Offline murmur: with no internet CHIPO babbles now and then, laughing.
 *
 * Own sounds only (embedded clips). All methods run on the LVGL task with the display lock held.
 */
class ChipoFun {
public:
    struct Hooks {
        std::function<bool(const char* clip_id)> play_clip;  // false if the clip does not exist
        std::function<void(const char* text)> notify;
        std::function<void()> boot_done;                      // the start-up screen ended: the face can wake up happy
        std::function<void()> oops;                           // face reacts to a fall (server error)
        std::function<void(uint32_t ms)> babble;             // face laughs while the murmur plays
        std::function<void(int64_t rgb)> set_eye_color;      // owner chose the eye colour (rgb >= 0) or reset it (-1)
        std::function<uint32_t()> clock;                      // ms, defaults to lv_tick_get
    };

    ChipoFun();
    ~ChipoFun();
    ChipoFun(const ChipoFun&) = delete;
    ChipoFun& operator=(const ChipoFun&) = delete;

    bool Start(lv_obj_t* parent, Hooks hooks, const char* version = "", const lv_font_t* font = nullptr);
    void Stop();

    void ShowBoot();  // the start-up screen (about 2.6 s); a touch skips it

    // True when CHIPO answered with a sound, so the server's spoken reply must be dropped.
    bool OnVoiceText(const char* text);
    void OnTouch();  // the overlay was touched (also reachable from tests)

    enum class Mode : uint8_t { Off, Boot, Lamp, Dice, Coin };
    Mode mode() const { return mode_; }
    int eye_changes() const { return eye_changes_; }

    struct DebugInfo {
        Mode mode;
        bool rolling;
        int dice_values[2];
        int dice_count;
        bool coin_heads;
        LampColor lamp;
        uint32_t lamp_rgb;  // colour of the lamp right now
        float boot_alpha;
    };
    DebugInfo Debug() const;

    // Offline murmur: with no internet CHIPO babbles now and then (cheerful made-up sounds, no words).
    // SetOffline may be called from any task; MurmurTick runs on the LVGL task (a 1 s timer, also public for tests).
    void SetOffline(bool offline);
    void MurmurTick(uint32_t now_ms);
    int murmur_count() const { return murmur_count_; }
    // The server could not be reached or failed: "Me caí... uuuy, me caí" (text, falling sound, sad face).
    // At most once every 20 s; returns true if it reacted.
    bool OnServerError(uint32_t now_ms);
    int oops_count() const { return oops_count_; }

    void Step(uint32_t now_ms);  // public so tests can drive time

private:
    static void TimerCallback(lv_timer_t* timer);
    static void MurmurCallback(lv_timer_t* timer);
    static void EventCallback(lv_event_t* e);
    uint32_t Now() const;
    bool Play(const char* clip);
    void Note(const char* text);
    void Build(const lv_font_t* font, const char* version, int w, int h);
    void SetMode(Mode m, uint32_t now_ms);
    void StartLamp(LampColor c, uint32_t now_ms);
    void StartDice(int count, uint32_t now_ms);
    void StartCoin(uint32_t now_ms);
    void UpdateBoot(uint32_t now_ms);
    void UpdateLamp(uint32_t now_ms);
    void UpdateDice(uint32_t now_ms);
    void UpdateCoin(uint32_t now_ms);
    void ShowPips(int die, int value);
    void Hide();

    Hooks hooks_;
    lv_timer_t* murmur_timer_ = nullptr;
    std::atomic<bool> offline_{false};
    bool offline_seen_ = false;
    uint32_t next_murmur_ms_ = 0;
    int murmur_count_ = 0;
    int murmur_last_ = 0;
    int oops_count_ = 0;
    int eye_changes_ = 0;
    uint32_t last_oops_ms_ = 0;
    bool had_oops_ = false;
    int w_ = 368, h_ = 448;  // screen size, kept from Start (object sizes read 0 before the first layout)
    lv_timer_t* timer_ = nullptr;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* boot_ = nullptr;
    lv_obj_t* boot_parts_[8] = {};
    lv_obj_t* lamp_ = nullptr;
    lv_obj_t* die_[2] = {};
    lv_obj_t* pip_[2][9] = {};
    lv_obj_t* coin_ = nullptr;
    lv_obj_t* coin_halo_ = nullptr;  // fixed-size glow behind the coin (a glow on the squashed coin itself hangs LVGL's shadow code)
    lv_obj_t* coin_face_[5] = {};  // eyes, smile, cross bars
    lv_obj_t* caption_ = nullptr;
    lv_obj_t* boot_title_ = nullptr;
    lv_obj_t* boot_version_ = nullptr;

    bool running_ = false;
    Mode mode_ = Mode::Off;
    uint32_t mode_since_ = 0;

    LampColor lamp_color_ = LampColor::Warm;
    uint32_t lamp_rgb_ = 0;
    uint32_t lamp_applied_rgb_ = 0xFFFFFFFF;

    int dice_count_ = 1;
    int dice_final_[2] = {1, 1};
    int dice_shown_[2] = {0, 0};
    bool rolling_ = false;
    uint32_t next_shuffle_ = 0;

    bool coin_heads_ = true;
    int coin_w_applied_ = -1;
    bool coin_content_shown_ = false;
    float boot_alpha_ = 0;
    int boot_alpha_applied_ = -1;
};
