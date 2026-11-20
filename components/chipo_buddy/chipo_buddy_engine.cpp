#include "chipo_buddy_engine.h"

void BuddyEngine::StartTimer(uint32_t seconds, uint32_t now_ms) {
    timer_on_ = seconds > 0;
    timer_end_ = now_ms + seconds * 1000u;
}

uint32_t BuddyEngine::TimerRemainingMs(uint32_t now_ms) const {
    if (!timer_on_ || BuddyReached(now_ms, timer_end_)) return 0;
    return timer_end_ - now_ms;
}

void BuddyEngine::StartPomodoro(uint32_t now_ms) {
    pomo_ = PomodoroPhase::Work;
    pomo_cycle_ = 1;
    pomo_end_ = now_ms + kWorkMs;
}

uint32_t BuddyEngine::PomodoroRemainingMs(uint32_t now_ms) const {
    if (pomo_ == PomodoroPhase::Off || BuddyReached(now_ms, pomo_end_)) return 0;
    return pomo_end_ - now_ms;
}

void BuddyEngine::StartStopwatch(uint32_t now_ms) {
    sw_on_ = true;
    sw_start_ = now_ms;
    sw_elapsed_ = 0;
}

void BuddyEngine::StopStopwatch(uint32_t now_ms) {
    if (!sw_on_) return;
    sw_elapsed_ = now_ms - sw_start_;
    sw_on_ = false;
}

uint32_t BuddyEngine::StopwatchMs(uint32_t now_ms) const { return sw_on_ ? now_ms - sw_start_ : sw_elapsed_; }

void BuddyEngine::CancelCountdowns() {
    timer_on_ = false;
    pomo_ = PomodoroPhase::Off;
}

void BuddyEngine::SetAlarm(int hour, int minute) {
    alarm_on_ = true;
    alarm_h_ = hour < 0 ? 0 : (hour > 23 ? 23 : hour);
    alarm_m_ = minute < 0 ? 0 : (minute > 59 ? 59 : minute);
    alarm_last_day_minute_ = -1;
}

BuddyEvents BuddyEngine::Update(uint32_t now_ms, const BuddyWallTime& wall) {
    BuddyEvents ev;
    if (timer_on_ && BuddyReached(now_ms, timer_end_)) {
        timer_on_ = false;
        ev.Add(BuddyEventKind::TimerDone);
    }
    if (pomo_ != PomodoroPhase::Off && BuddyReached(now_ms, pomo_end_)) {
        if (pomo_ == PomodoroPhase::Work) {
            if (pomo_cycle_ >= kWorksPerLongBreak) {
                pomo_ = PomodoroPhase::LongBreak;
                pomo_end_ = now_ms + kLongBreakMs;
                ev.Add(BuddyEventKind::PomodoroLongBreak);
            } else {
                pomo_ = PomodoroPhase::Break;
                pomo_end_ = now_ms + kBreakMs;
                ev.Add(BuddyEventKind::PomodoroBreak);
            }
        } else {
            pomo_cycle_ = pomo_ == PomodoroPhase::LongBreak ? 1 : pomo_cycle_ + 1;
            pomo_ = PomodoroPhase::Work;
            pomo_end_ = now_ms + kWorkMs;
            ev.Add(BuddyEventKind::PomodoroWork);
        }
    }
    if (alarm_on_ && wall.valid) {
        const int minute_of_day = wall.hour * 60 + wall.minute;
        if (wall.hour == alarm_h_ && wall.minute == alarm_m_) {
            if (alarm_last_day_minute_ != minute_of_day) {
                alarm_last_day_minute_ = minute_of_day;
                alarm_on_ = false;  // one shot
                ev.Add(BuddyEventKind::AlarmRing);
            }
        }
    }
    return ev;
}

BuddyShow BuddyEngine::Best(uint32_t now_ms, uint32_t* value_ms) const {
    uint32_t dummy;
    uint32_t& v = value_ms ? *value_ms : dummy;
    if (pomo_ != PomodoroPhase::Off) { v = PomodoroRemainingMs(now_ms); return BuddyShow::Pomodoro; }
    if (timer_on_) { v = TimerRemainingMs(now_ms); return BuddyShow::Timer; }
    if (sw_on_ || sw_elapsed_ > 0) { v = StopwatchMs(now_ms); return BuddyShow::Stopwatch; }
    v = 0;
    return BuddyShow::Nothing;
}
