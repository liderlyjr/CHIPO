#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "chipo_remind_logic.h"
#include "lvgl.h"

/**
 * CHIPO REMIND controller: stores reminders (kept across reboots through the load/save hooks), rings them at the
 * wall-clock time with a sound and a text, and answers the voice commands. Call everything on the LVGL task.
 */
class ChipoRemind {
public:
    struct Wall {
        bool valid = false;  // false until the real time is known (not 1970)
        int hour = 0, minute = 0;
    };
    struct Hooks {
        std::function<bool(const char* clip_id)> play_clip;  // false if the clip does not exist
        std::function<void(const char* text)> notify;
        std::function<void(const char* emotion, uint32_t ms)> emotion;
        std::function<bool()> assistant_busy;                // true while listening / speaking: wait before ringing
        std::function<bool(Wall* out)> wall_time;
        std::function<std::string()> load;                   // saved reminders ("" if none)
        std::function<void(const std::string&)> save;        // called after every change
        std::function<uint32_t()> clock;                     // ms, defaults to lv_tick_get
    };

    ChipoRemind();
    ~ChipoRemind();
    ChipoRemind(const ChipoRemind&) = delete;
    ChipoRemind& operator=(const ChipoRemind&) = delete;

    bool Start(Hooks hooks);
    void Stop();

    // True when CHIPO answered with a sound (the server's spoken reply must be dropped).
    bool OnVoiceText(const char* text);

    const RemindStore& store() const { return store_; }
    bool ringing() const { return ring_left_ > 0; }
    int rung() const { return rung_; }

    void Step(uint32_t now_ms);  // public so tests can drive time

private:
    static void TimerCallback(lv_timer_t* timer);
    uint32_t Now() const;
    bool Say(const std::string& text, const char* emotion);
    void Persist();
    void Ring(uint32_t now_ms);

    Hooks hooks_;
    RemindStore store_;
    lv_timer_t* timer_ = nullptr;
    bool running_ = false;
    std::vector<std::string> ring_queue_;  // texts waiting to ring (oldest first)
    int ring_left_ = 0;                    // rings left for the first text in the queue
    uint32_t next_ring_ms_ = 0;
    int rung_ = 0;
};
