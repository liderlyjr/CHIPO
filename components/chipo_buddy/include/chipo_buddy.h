#pragma once

#include <cstdint>
#include <functional>

#include "chipo_buddy_engine.h"
#include "chipo_buddy_view.h"
#include "chipo_buddy_voice.h"
#include "lvgl.h"

/**
 * CHIPO BUDDY: timer, pomodoro, stopwatch, alarm clock and bedside mode, all by voice.
 *
 *   voice text (XiaoZhi "stt")  ->  OnVoiceText()  ->  BuddyEngine  ->  clock screen + sounds
 *
 * Own sounds only: short chimes/alarms embedded in the firmware, played through the assistant's
 * audio path (`play_clip`). All methods run on the LVGL task with the display lock held.
 */
class ChipoBuddy {
public:
    struct Hooks {
        std::function<bool(const char* clip_id)> play_clip;  // false if the clip does not exist
        std::function<void(const char* text)> notify;
        std::function<bool()> assistant_busy;                // true while listening / speaking
        std::function<bool()> in_drive;                      // car mode on: no bedtime there
        std::function<uint32_t()> clock;                     // ms, defaults to lv_tick_get
        std::function<bool(BuddyWallTime* out)> wall_time;   // real local time if known
        std::function<void(const char* which)> scene;        // "night" / "morning": Home Assistant
    };

    ChipoBuddy();
    ~ChipoBuddy();
    ChipoBuddy(const ChipoBuddy&) = delete;
    ChipoBuddy& operator=(const ChipoBuddy&) = delete;

    bool Start(lv_obj_t* parent, Hooks hooks, const lv_font_t* font = nullptr);
    void Stop();

    // Every recognised sentence. True when CHIPO answered by itself (sound played), so the server's
    // spoken reply must be dropped. "Qué hora es" is left to the assistant (returns false).
    bool OnVoiceText(const char* text);

    // CHIPO lies on its side like a bedside clock (reported by the face after a while): show the big clock
    // turned to be read from the front. `side` is -1 for the left side down (right edge up), +1 for the right.
    void SetDeskClock(bool on, int side = -1);
    bool desk_clock() const { return desk_clock_; }

    // A pomodoro is running: fraction of the current period still to go (0..1) and whether it is a work period.
    bool PomodoroProgress(float* remaining_fraction, bool* work) const;

    bool bedtime() const { return bedtime_; }
    bool ringing() const { return ring_ != Ring::None; }
    bool screen_shown() const { return view_.IsShown(); }
    const BuddyEngine& engine() const { return engine_; }
    ChipoBuddyView::DebugInfo Debug() const { return view_.Debug(); }

    void Step(uint32_t now_ms);  // public so tests can drive time

    // The screen was touched (also reachable from tests).
    void OnTouch(bool long_press);

private:
    enum class Ring : uint8_t { None, Timer, Alarm, Pomodoro };

    static void TimerCallback(lv_timer_t* timer);
    uint32_t Now() const;
    bool Handle(const BuddyCommand& cmd);
    bool Chime(const char* clip = "buddy_chime");
    void Note(const char* text);
    void Peek(uint32_t ms);
    void StartRinging(Ring kind, const char* text);
    void StopRinging(bool snooze);
    void EnterBedtime();
    void LeaveBedtime();
    void Render(uint32_t now_ms);
    bool WallNow(BuddyWallTime* out) const;
    void Scene(const char* which) { if (hooks_.scene) hooks_.scene(which); }

    Hooks hooks_;
    lv_timer_t* timer_ = nullptr;
    ChipoBuddyView view_;
    BuddyEngine engine_;
    bool running_ = false;
    bool bedtime_ = false;
    bool desk_clock_ = false;

    uint32_t peek_until_ = 0;
    bool peeking_ = false;
    bool pinned_ = false;
    Ring ring_ = Ring::None;
    uint32_t ring_started_ = 0, next_sound_ = 0;
    int ring_plays_ = 0;
    int alarm_snoozes_ = 0;
    char ring_text_[40] = {};
    uint32_t last_engine_ms_ = 0;
    char caption_[48] = {};
    char footer_[48] = {};
};
