#include "chipo_buddy.h"

#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t kTickMs = 250;
constexpr uint32_t kPeekMs = 6000;
constexpr uint32_t kRingEveryMs = 4000;
constexpr int kRingMaxPlays = 8;
constexpr int kMaxAlarmSnoozes = 3;
constexpr int kSnoozeMinutes = 9;
constexpr uint32_t kAmber = 0xFF9A2E;
constexpr uint32_t kGreen = 0x5CFF8A;
constexpr uint32_t kCyan = 0x59D8FF;
constexpr uint32_t kRed = 0xFF4A3D;

// MM:SS below one hour, HH:MM from one hour up (the footer says which).
void Digits(uint32_t ms, char out[4], bool* hours) {
    uint32_t total = ms / 1000u;
    *hours = total >= 3600u;
    uint32_t a, b;
    if (*hours) {
        a = total / 3600u;
        b = (total / 60u) % 60u;
        if (a > 99) a = 99;
    } else {
        a = total / 60u;
        b = total % 60u;
    }
    out[0] = static_cast<char>('0' + a / 10);
    out[1] = static_cast<char>('0' + a % 10);
    out[2] = static_cast<char>('0' + b / 10);
    out[3] = static_cast<char>('0' + b % 10);
}
}  // namespace

ChipoBuddy::ChipoBuddy() = default;
ChipoBuddy::~ChipoBuddy() { Stop(); }

uint32_t ChipoBuddy::Now() const { return hooks_.clock ? hooks_.clock() : lv_tick_get(); }

bool ChipoBuddy::WallNow(BuddyWallTime* out) const {
    *out = BuddyWallTime{};
    return hooks_.wall_time && hooks_.wall_time(out) && out->valid;
}

bool ChipoBuddy::Start(lv_obj_t* parent, Hooks hooks, const lv_font_t* font) {
    if (running_) return true;
    hooks_ = std::move(hooks);
#if LV_FONT_MONTSERRAT_24
    if (!font) font = &lv_font_montserrat_24;
#endif
    if (!font) font = lv_font_get_default();
    if (!view_.Create(parent, font)) return false;
    view_.SetTouchCallback([this](bool long_press) { OnTouch(long_press); });
    timer_ = lv_timer_create(TimerCallback, kTickMs, this);
    if (!timer_) {
        view_.Destroy();
        return false;
    }
    running_ = true;
    last_engine_ms_ = Now();
    return true;
}

void ChipoBuddy::Stop() {
    if (timer_) lv_timer_delete(timer_);
    timer_ = nullptr;
    view_.Destroy();
    running_ = false;
    bedtime_ = false;
    ring_ = Ring::None;
    peeking_ = false;
}

void ChipoBuddy::TimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoBuddy*>(lv_timer_get_user_data(timer));
    if (self) self->Step(self->Now());
}

bool ChipoBuddy::Chime(const char* clip) { return hooks_.play_clip && hooks_.play_clip(clip); }

void ChipoBuddy::Note(const char* text) {
    if (hooks_.notify) hooks_.notify(text);
}

void ChipoBuddy::Peek(uint32_t ms) {
    peeking_ = true;
    peek_until_ = Now() + ms;
}

void ChipoBuddy::StartRinging(Ring kind, const char* text) {
    ring_ = kind;
    ring_started_ = Now();
    next_sound_ = ring_started_;
    ring_plays_ = 0;
    std::snprintf(ring_text_, sizeof ring_text_, "%s", text);
    Note(text);
}

void ChipoBuddy::StopRinging(bool snooze) {
    const Ring was = ring_;
    ring_ = Ring::None;
    peeking_ = false;
    if (was == Ring::Alarm) {
        if (snooze) {
            BuddyWallTime w;
            if (WallNow(&w)) {
                int total = w.hour * 60 + w.minute + kSnoozeMinutes;
                engine_.SetAlarm((total / 60) % 24, total % 60);
            }
            Note("Alarma pospuesta 9 min");
        } else {
            alarm_snoozes_ = 0;
            if (bedtime_) LeaveBedtime();  // good morning
            Scene("morning");
        }
    }
}

void ChipoBuddy::SetDeskClock(bool on, int side) {
    if (!running_) return;
    desk_clock_ = on;
    view_.SetRotation(on ? (side > 0 ? 2 : 1) : 0);
    if (!on && !bedtime_ && ring_ == Ring::None && !peeking_) view_.Hide();
}

void ChipoBuddy::EnterBedtime() {
    bedtime_ = true;
    peeking_ = false;
    Scene("night");
}

void ChipoBuddy::LeaveBedtime() {
    bedtime_ = false;
    peeking_ = false;
    view_.Hide();
}

void ChipoBuddy::OnTouch(bool long_press) {
    if (ring_ != Ring::None) {
        const bool snooze = ring_ == Ring::Alarm && alarm_snoozes_ < kMaxAlarmSnoozes && !long_press;
        if (snooze) ++alarm_snoozes_;
        StopRinging(snooze);
        return;
    }
    if (long_press && bedtime_) {
        LeaveBedtime();
        Scene("morning");
        return;
    }
    // A short tap closes a peek of a countdown.
    if (peeking_ && !bedtime_ && !desk_clock_) {
        peeking_ = false;
        view_.Hide();
    }
}

bool ChipoBuddy::OnVoiceText(const char* text) {
    if (!running_) return false;
    const BuddyCommand cmd = ParseBuddyCommand(text);
    if (cmd.intent == BuddyIntent::None) return false;
    return Handle(cmd);
}

bool ChipoBuddy::Handle(const BuddyCommand& cmd) {
    const uint32_t now = Now();
    char buf[48];
    switch (cmd.intent) {
    case BuddyIntent::SetTimer: {
        engine_.StartTimer(cmd.seconds, now);
        // unsigned (not uint32_t) for printf: on the ESP32 uint32_t is `unsigned long`.
        const unsigned hours = static_cast<unsigned>(cmd.seconds / 3600u);
        const unsigned mins = static_cast<unsigned>((cmd.seconds / 60u) % 60u);
        const unsigned secs = static_cast<unsigned>(cmd.seconds % 60u);
        if (hours > 0) std::snprintf(buf, sizeof buf, "Temporizador: %u h %u min", hours, mins);
        else if (secs == 0) std::snprintf(buf, sizeof buf, "Temporizador: %u min", mins);
        else std::snprintf(buf, sizeof buf, "Temporizador: %u min %u s", mins, secs);
        Note(buf);
        Peek(kPeekMs);
        return Chime();
    }
    case BuddyIntent::Pomodoro:
        engine_.StartPomodoro(now);
        Note("Pomodoro: 25 min de concentración");
        Peek(kPeekMs);
        return Chime();
    case BuddyIntent::StopwatchStart:
        engine_.StartStopwatch(now);
        Note("Cronómetro en marcha");
        Peek(kPeekMs);
        return Chime();
    case BuddyIntent::StopwatchStop:
        if (!engine_.stopwatch_running()) return false;
        engine_.StopStopwatch(now);
        Peek(kPeekMs * 2);
        return Chime();
    case BuddyIntent::CancelTimers:
        engine_.CancelCountdowns();
        if (ring_ == Ring::Timer || ring_ == Ring::Pomodoro) StopRinging(false);
        Note("Cancelado");
        return Chime();
    case BuddyIntent::TimeLeft: {
        uint32_t v;
        if (engine_.Best(now, &v) == BuddyShow::Nothing) return false;  // nothing running: the assistant answers
        Peek(kPeekMs);
        return Chime();
    }
    case BuddyIntent::SetAlarm:
        engine_.SetAlarm(cmd.hour, cmd.minute);
        alarm_snoozes_ = 0;
        std::snprintf(buf, sizeof buf, "Alarma a las %02d:%02d", cmd.hour, cmd.minute);
        Note(buf);
        return Chime();
    case BuddyIntent::CancelAlarm:
        if (!engine_.alarm_set()) return false;
        engine_.CancelAlarm();
        Note("Alarma cancelada");
        return Chime();
    case BuddyIntent::Bedtime:
        if (hooks_.in_drive && hooks_.in_drive()) return false;
        if (bedtime_) return false;
        EnterBedtime();
        Note("Buenas noches");
        return Chime();
    case BuddyIntent::Wake:
        if (!bedtime_) return false;  // an ordinary "buenos días": the assistant greets
        LeaveBedtime();
        Scene("morning");
        Note("Buenos días");
        return Chime();
    case BuddyIntent::AskTime:
        Peek(kPeekMs);  // the assistant tells the time by voice; the clock shows it
        return false;
    case BuddyIntent::Dismiss:
        if (ring_ == Ring::None) return false;
        StopRinging(false);
        return Chime();
    case BuddyIntent::None:
        break;
    }
    return false;
}

void ChipoBuddy::Step(uint32_t now_ms) {
    if (!running_) return;
    BuddyWallTime wall;
    WallNow(&wall);
    const BuddyEvents ev = engine_.Update(now_ms, wall);
    for (int i = 0; i < ev.count; ++i) {
        switch (ev.kind[i]) {
        case BuddyEventKind::TimerDone: StartRinging(Ring::Timer, "Temporizador terminado"); break;
        case BuddyEventKind::AlarmRing:
            if (ring_ == Ring::None) StartRinging(Ring::Alarm, "Alarma");
            break;
        case BuddyEventKind::PomodoroBreak:
            Note("Pomodoro: descanso de 5 min");
            Chime("buddy_timer_done");
            Peek(kPeekMs);
            break;
        case BuddyEventKind::PomodoroLongBreak:
            Note("Pomodoro: descanso largo de 15 min");
            Chime("buddy_timer_done");
            Peek(kPeekMs);
            break;
        case BuddyEventKind::PomodoroWork:
            Note("Pomodoro: a concentrarse");
            Chime("buddy_chime");
            Peek(kPeekMs);
            break;
        }
    }

    if (ring_ != Ring::None) {
        const bool busy = hooks_.assistant_busy && hooks_.assistant_busy();
        if (BuddyReached(now_ms, next_sound_)) {
            if (ring_plays_ >= kRingMaxPlays) {
                // Nobody came: a timer just ends; the alarm snoozes a few times.
                const bool again = ring_ == Ring::Alarm && alarm_snoozes_ < kMaxAlarmSnoozes;
                if (again) ++alarm_snoozes_;
                StopRinging(again);
            } else {
                if (!busy) Chime(ring_ == Ring::Alarm ? "buddy_alarm" : "buddy_timer_done");
                ++ring_plays_;
                next_sound_ = now_ms + kRingEveryMs;
            }
        }
    }
    if (peeking_ && BuddyReached(now_ms, peek_until_) && ring_ == Ring::None) {
        peeking_ = false;
        if (!bedtime_ && !desk_clock_) view_.Hide();
    }
    Render(now_ms);
}

void ChipoBuddy::Render(uint32_t now_ms) {
    BuddyScreen s;
    BuddyWallTime wall;
    const bool have_wall = WallNow(&wall);
    const bool blink = (now_ms / 500u) % 2u == 0;
    auto wall_digits = [&](bool dim) {
        if (have_wall) {
            s.digits[0] = static_cast<char>('0' + wall.hour / 10);
            s.digits[1] = static_cast<char>('0' + wall.hour % 10);
            s.digits[2] = static_cast<char>('0' + wall.minute / 10);
            s.digits[3] = static_cast<char>('0' + wall.minute % 10);
            s.colon = dim ? true : (wall.second % 2 == 0);
        } else {
            for (char& c : s.digits) c = ' ';
            s.colon = false;
        }
    };

    if (ring_ != Ring::None) {
        if (ring_ == Ring::Alarm) {
            wall_digits(false);
            s.colon = blink;
            s.caption = "DESPIERTA";
            s.footer = "Toca para posponer 9 min";
        } else {
            for (char& c : s.digits) c = blink ? '0' : ' ';
            s.colon = blink;
            s.caption = ring_text_;
            s.footer = "Toca o di \"para\"";
        }
        s.color = kRed;
        view_.Show(s);
        return;
    }
    if (peeking_) {
        uint32_t v = 0;
        const BuddyShow what = engine_.Best(now_ms, &v);
        if (what == BuddyShow::Nothing) {
            wall_digits(false);
            s.caption = "HORA";
            s.color = kAmber;
        } else {
            bool hours = false;
            Digits(v, s.digits, &hours);
            s.colon = true;
            s.color = kAmber;
            if (what == BuddyShow::Pomodoro) {
                const PomodoroPhase ph = engine_.pomodoro_phase();
                if (ph == PomodoroPhase::Work) {
                    std::snprintf(caption_, sizeof caption_, "CONCENTRACION %d/4", engine_.pomodoro_cycle());
                    s.color = kAmber;
                } else {
                    std::snprintf(caption_, sizeof caption_, "%s", ph == PomodoroPhase::LongBreak ? "DESCANSO LARGO" : "DESCANSO");
                    s.color = kGreen;
                }
                s.caption = caption_;
            } else if (what == BuddyShow::Timer) {
                s.caption = "TEMPORIZADOR";
                s.color = kCyan;
            } else {
                s.caption = "CRONOMETRO";
                s.color = kGreen;
            }
            s.footer = hours ? "horas : minutos" : "minutos : segundos";
        }
        view_.Show(s);
        return;
    }
    if (desk_clock_) {
        wall_digits(false);
        s.color = bedtime_ ? 0xC8641A : kAmber;
        s.brightness = bedtime_ ? 90 : 230;
        view_.Show(s);
        return;
    }
    if (bedtime_) {
        wall_digits(true);
        s.color = 0xC8641A;  // deep amber
        s.brightness = 70;   // dim: it is on the bedside table
        s.caption = "";
        if (engine_.alarm_set()) {
            std::snprintf(footer_, sizeof footer_, "alarma %02d:%02d", engine_.alarm_hour(), engine_.alarm_minute());
            s.footer = footer_;
        } else {
            s.footer = have_wall ? "" : "sin hora (sin Wi-Fi)";
        }
        view_.Show(s);
        return;
    }
    if (view_.IsShown()) view_.Hide();
}
