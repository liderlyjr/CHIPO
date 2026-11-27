#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "chipo_tilt.h"  // ImuSample
#include "chipo_tools_logic.h"
#include "lvgl.h"

/**
 * CHIPO TOOLS: small things that make use of the square AMOLED screen.
 *   - info strip around the face: the time on top and the temperature below (only when they are known)
 *   - pomodoro ring: a ring along the edge of the screen that drains while a pomodoro period runs
 *   - spirit level (bubble) with the accelerometer, magic 8-ball (shake or tap), drawing canvas
 *   - subtitles: the sentence CHIPO is saying, in big letters (off until asked)
 *   - shake: throws the dice / flips the coin again (via the FUN component) or asks the 8-ball
 *
 * Nothing here invents data: no clock -> no time; no weather downloaded -> no temperature.
 * All methods run on the LVGL task with the display lock held, except OnSample which only needs the same task as the
 * IMU observer (the face task = LVGL task).
 */
class ChipoTools {
public:
    struct Hooks {
        std::function<bool(int* hour, int* minute)> wall_time;       // real local time, false if not set yet
        std::function<bool(int* temp_c)> temperature;                // newest downloaded weather, false if none
        std::function<void()> refresh_weather;                       // silent background download
        std::function<bool(float* remaining, bool* work)> pomodoro;  // a pomodoro runs: fraction left, work period?
        std::function<bool()> face_calm;                             // CHIPO is idle and nothing else is going on
        std::function<bool()> other_screen;                          // weather, clock, lamp, car mode... cover the face
        std::function<void(const char* emotion, uint32_t ms)> emotion;
        std::function<bool(const char* clip_id)> play_clip;          // false if the clip does not exist
        std::function<void(const char* text)> notify;
        std::function<bool(uint32_t now_ms)> shake_other;            // dice / coin: throw again; true if it did
        std::function<const lv_font_t*()> text_font;                 // font with Spanish accents; nullptr: fold accents
        std::function<bool(const char* key, bool def)> load_flag;    // saved settings
        std::function<void(const char* key, bool value)> save_flag;
        std::function<uint32_t()> clock;                             // ms, defaults to lv_tick_get
    };

    ChipoTools();
    ~ChipoTools();
    ChipoTools(const ChipoTools&) = delete;
    ChipoTools& operator=(const ChipoTools&) = delete;

    bool Start(lv_obj_t* parent, Hooks hooks);
    void Stop();

    enum class Tool : uint8_t { None, Level, GravityBall, Horizon, Canvas };
    void Open(Tool tool);
    void Close();
    Tool tool() const { return tool_; }
    bool screen_shown() const { return tool_ != Tool::None; }
    // GPIO18: none -> level -> Gravity Ball -> artificial horizon -> DrawPanel -> none.
    void OnKey();

    // Every recognised sentence. True when CHIPO answered with a sound, so the server's spoken reply must be dropped.
    bool OnVoiceText(const char* text);
    // Every accelerometer sample (with the sensor clock). Detects shakes and feeds the level.
    void OnSample(const ImuSample& sample, uint32_t now_ms);
    // The sentence CHIPO is saying (only drawn when subtitles are on).
    void ShowSubtitle(const char* text);
    void ClearCanvas();

    // Touch on the canvas (also reachable from tests). Coordinates in screen pixels.
    void PenDown(int x, int y);
    void PenMove(int x, int y);
    void PenUp();

    void SetInfoEnabled(bool on);
    void SetSubtitlesEnabled(bool on);
    bool info_enabled() const { return info_on_; }
    bool subtitles_enabled() const { return subs_on_; }

    void Step(uint32_t now_ms);  // public so tests can drive time

    struct DebugInfo {
        Tool tool;
        bool strip_visible;
        char time_text[8];
        bool temp_visible;
        int temp_c;
        bool ring_visible;
        int ring_value;        // 0..1000
        bool ring_work;
        bool level_flat, level_ok;
        float bubble_x, bubble_y, angle_deg;
        float gravity_x, gravity_y;
        float horizon_roll, horizon_pitch;
        bool subtitle_visible;
        char subtitle_text[200];
        long canvas_dots;      // pixels drawn since the canvas was cleared
        int shakes;
    };
    DebugInfo Debug() const;

private:
    static void TimerCallback(lv_timer_t* timer);
    static void RootEvent(lv_event_t* e);
    static void CanvasEvent(lv_event_t* e);
    static void ButtonEvent(lv_event_t* e);
    uint32_t Now() const;
    bool Play(const char* clip) { return hooks_.play_clip && hooks_.play_clip(clip); }
    void Build(int w, int h);
    void UpdateStrip(uint32_t now_ms);
    void UpdateRing(uint32_t now_ms);
    void UpdateLevel(uint32_t now_ms);
    void UpdateGravityBall(uint32_t now_ms);
    void UpdateHorizon(uint32_t now_ms);
    void UpdateSubtitle(uint32_t now_ms);
    void SetOpa(lv_obj_t* obj, int opa);
    void DrawDot(int x, int y, int r);
    void DrawSegment(int x0, int y0, int x1, int y1);
    void LayoutTempRow();

    Hooks hooks_;
    lv_timer_t* timer_ = nullptr;
    lv_obj_t* parent_ = nullptr;
    bool running_ = false;
    int w_ = 480, h_ = 480;
    Tool tool_ = Tool::None;

    const lv_font_t* font_big_ = nullptr;
    const lv_font_t* font_mid_ = nullptr;
    const lv_font_t* font_text_ = nullptr;  // subtitles
    bool text_has_accents_ = false;

    // info strip
    bool info_on_ = true;
    lv_obj_t* time_label_ = nullptr;
    lv_obj_t* temp_row_ = nullptr;
    lv_obj_t* temp_num_ = nullptr;
    lv_obj_t* temp_deg_ = nullptr;
    lv_obj_t* temp_unit_ = nullptr;
    float strip_opa_ = 0;
    char time_text_[8] = "";
    int temp_c_ = 0;
    bool temp_shown_ = false;
    uint32_t last_strip_ms_ = 0;
    uint32_t next_prefetch_ms_ = 0;
    bool strip_target_ = false;

    // pomodoro ring
    lv_obj_t* ring_ = nullptr;
    float ring_opa_ = 0;
    int ring_value_ = 0;
    bool ring_work_ = true;
    bool ring_target_ = false;
    uint32_t last_ring_ms_ = 0;

    // overlay with the three tools
    lv_obj_t* root_ = nullptr;
    lv_obj_t* caption_ = nullptr;
    lv_obj_t* hint_ = nullptr;
    // level
    lv_obj_t* lvl_ = nullptr;
    lv_obj_t* lvl_ring_ = nullptr;
    lv_obj_t* lvl_bubble_ = nullptr;
    lv_obj_t* lvl_cross_[2] = {};
    lv_obj_t* lvl_target_ = nullptr;
    lv_obj_t* lvl_angle_ = nullptr;
    float ax_ = 0, ay_ = 0, az_ = 1;
    bool have_accel_ = false;
    LevelReading level_{};
    // Manual spirit-level zero, captured by tapping the screen while it rests on a flat surface.
    float level_zero_x_ = 0.0f, level_zero_y_ = 0.0f;
    bool level_calibrated_ = false;
    float bubble_x_ = 0, bubble_y_ = 0;
    // Gravity Ball and artificial horizon (QMI8658 accelerometer)
    lv_obj_t* gravity_ball_ = nullptr;
    lv_obj_t* gravity_ball_track_ = nullptr;
    lv_obj_t* horizon_canvas_ = nullptr;
    uint16_t* horizon_px_ = nullptr;
    float gravity_x_ = 0.0f, gravity_y_ = 0.0f;
    float gravity_vx_ = 0.0f, gravity_vy_ = 0.0f;
    float horizon_roll_ = 0.0f, horizon_pitch_ = 0.0f;
    float horizon_zero_roll_ = 0.0f, horizon_zero_pitch_ = 0.0f;
    // canvas
    lv_obj_t* canvas_ = nullptr;
    lv_obj_t* canvas_close_ = nullptr;
    lv_obj_t* canvas_clear_ = nullptr;
    uint16_t* canvas_px_ = nullptr;  // w x h RGB565, owned here, handed to the LVGL canvas
    bool pen_down_ = false;
    int pen_x_ = 0, pen_y_ = 0;
    int pen_color_ = 0;
    long dots_ = 0;

    // subtitles
    lv_obj_t* sub_ = nullptr;
    bool subs_on_ = false;
    uint32_t sub_hide_at_ = 0;
    float sub_opa_ = 0;
    bool sub_visible_ = false;
    char sub_text_[200] = "";

    ShakeDetector shake_;
    int shakes_ = 0;
    uint32_t last_long_ms_ = 0;
};
