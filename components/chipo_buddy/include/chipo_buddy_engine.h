#pragma once

#include <cstdint>

/**
 * Time keeping of CHIPO BUDDY: one countdown timer, a pomodoro, a stopwatch and an alarm clock.
 * Pure logic with injected time (no ESP-IDF, no LVGL): unit tested on a PC.
 *
 * Countdowns use the monotonic millisecond clock, so they survive changes of the wall clock
 * (Wi-Fi time sync). Only the alarm needs the wall clock, and only when it is valid.
 */
struct BuddyWallTime {
    bool valid = false;  // false until the real time is known (not 1970)
    int hour = 0, minute = 0, second = 0;
};

enum class BuddyEventKind : uint8_t {
    TimerDone,
    PomodoroBreak,      // work period finished: take a break
    PomodoroLongBreak,  // fourth work period finished
    PomodoroWork,       // break finished: back to work
    AlarmRing
};

struct BuddyEvents {
    BuddyEventKind kind[4];
    int count = 0;
    void Add(BuddyEventKind k) {
        if (count < 4) kind[count++] = k;
    }
};

enum class PomodoroPhase : uint8_t { Off, Work, Break, LongBreak };

// What to show when the clock overlay is asked for a countdown.
enum class BuddyShow : uint8_t { Nothing, Pomodoro, Timer, Stopwatch };

class BuddyEngine {
public:
    static constexpr uint32_t kWorkMs = 25u * 60u * 1000u;
    static constexpr uint32_t kBreakMs = 5u * 60u * 1000u;
    static constexpr uint32_t kLongBreakMs = 15u * 60u * 1000u;
    static constexpr int kWorksPerLongBreak = 4;

    void StartTimer(uint32_t seconds, uint32_t now_ms);
    bool timer_active() const { return timer_on_; }
    uint32_t TimerRemainingMs(uint32_t now_ms) const;

    void StartPomodoro(uint32_t now_ms);
    void StopPomodoro() { pomo_ = PomodoroPhase::Off; }
    PomodoroPhase pomodoro_phase() const { return pomo_; }
    int pomodoro_cycle() const { return pomo_cycle_; }  // 1-based work period in this round
    uint32_t PomodoroRemainingMs(uint32_t now_ms) const;

    void StartStopwatch(uint32_t now_ms);
    void StopStopwatch(uint32_t now_ms);  // keeps the elapsed time readable
    bool stopwatch_running() const { return sw_on_; }
    uint32_t StopwatchMs(uint32_t now_ms) const;

    void CancelCountdowns();  // timer + pomodoro (not the stopwatch, not the alarm)

    void SetAlarm(int hour, int minute);
    void CancelAlarm() { alarm_on_ = false; }
    bool alarm_set() const { return alarm_on_; }
    int alarm_hour() const { return alarm_h_; }
    int alarm_minute() const { return alarm_m_; }

    // Call about once a second (or faster). Returns the events that happened since the last call.
    BuddyEvents Update(uint32_t now_ms, const BuddyWallTime& wall);

    // Which countdown deserves the screen right now, and its value (ms; stopwatch counts up).
    BuddyShow Best(uint32_t now_ms, uint32_t* value_ms) const;

private:
    bool timer_on_ = false;
    uint32_t timer_end_ = 0;

    PomodoroPhase pomo_ = PomodoroPhase::Off;
    uint32_t pomo_end_ = 0;
    int pomo_cycle_ = 0;

    bool sw_on_ = false;
    uint32_t sw_start_ = 0, sw_elapsed_ = 0;

    bool alarm_on_ = false;
    int alarm_h_ = 0, alarm_m_ = 0;
    int alarm_last_day_minute_ = -1;  // minute of the day in which it last rang
};

// Wrap-safe "a has been reached" for 32-bit millisecond clocks.
inline bool BuddyReached(uint32_t now, uint32_t target) { return static_cast<int32_t>(now - target) >= 0; }
