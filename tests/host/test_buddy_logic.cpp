// CHIPO BUDDY: spoken commands and the time engine (no LVGL).
#include <cstdio>

#include "chipo_buddy_engine.h"
#include "chipo_buddy_voice.h"

static int g_fail = 0;
#define CHECK(c, ...)                                   \
    do {                                                \
        if (!(c)) {                                     \
            ++g_fail;                                   \
            std::printf("FAIL line %d: ", __LINE__);    \
            std::printf(__VA_ARGS__);                   \
            std::printf("\n");                          \
        }                                               \
    } while (0)

static void Timer(const char* text, uint32_t secs) {
    const BuddyCommand c = ParseBuddyCommand(text);
    CHECK(c.intent == BuddyIntent::SetTimer && c.seconds == secs, "timer \"%s\" -> intent %d, %u s (want %u)", text, static_cast<int>(c.intent), c.seconds, secs);
}
static void Is(const char* text, BuddyIntent i) {
    const BuddyCommand c = ParseBuddyCommand(text);
    CHECK(c.intent == i, "\"%s\" -> %d (want %d)", text, static_cast<int>(c.intent), static_cast<int>(i));
}
static void Alarm(const char* text, int h, int m) {
    const BuddyCommand c = ParseBuddyCommand(text);
    CHECK(c.intent == BuddyIntent::SetAlarm && c.hour == h && c.minute == m, "alarm \"%s\" -> %d %02d:%02d (want %02d:%02d)", text, static_cast<int>(c.intent), c.hour, c.minute, h, m);
}

int main() {
    std::printf("== timer phrases\n");
    Timer("Pon un temporizador de 10 minutos", 600);
    Timer("Chipo, temporizador de diez minutos.", 600);
    Timer("avísame en media hora", 1800);
    Timer("temporizador de una hora y media", 5400);
    Timer("cuenta atrás de 30 segundos", 30);
    Timer("temporizador de dos minutos y treinta segundos", 150);
    Timer("temporizador de treinta y cinco minutos", 2100);
    Timer("recuérdame en 1 hora", 3600);
    Timer("temporizador 5", 300);
    Timer("Pon un temporizador de un cuarto de hora", 900);
    Timer("despiértame en 20 minutos", 1200);
    Is("temporizador", BuddyIntent::None);  // no duration: not a command
    Is("temporizador de 99 horas", BuddyIntent::None);
    Is("cancela el temporizador", BuddyIntent::CancelTimers);
    Is("para el temporizador", BuddyIntent::CancelTimers);

    std::printf("== alarm phrases\n");
    Alarm("despiértame a las 7", 7, 0);
    Alarm("pon una alarma a las 7 y media", 7, 30);
    Alarm("Chipo, alarma a las 6:45", 6, 45);
    Alarm("pon la alarma a las ocho menos cuarto de la mañana", 7, 45);
    Alarm("despiértame a las 7 y cuarto", 7, 15);
    Alarm("alarma a las 10 de la noche", 22, 0);
    Alarm("alarma a las doce de la noche", 0, 0);
    Alarm("pon una alarma a las 5 de la tarde", 17, 0);
    Is("cancela la alarma", BuddyIntent::CancelAlarm);
    Is("quita la alarma", BuddyIntent::CancelAlarm);

    std::printf("== other commands\n");
    Is("pomodoro", BuddyIntent::Pomodoro);
    Is("Chipo, pon un pomodoro", BuddyIntent::Pomodoro);
    Is("modo concentración", BuddyIntent::Pomodoro);
    Is("para el pomodoro", BuddyIntent::CancelTimers);
    Is("cronómetro", BuddyIntent::StopwatchStart);
    Is("pon un cronómetro", BuddyIntent::StopwatchStart);
    Is("para el cronómetro", BuddyIntent::StopwatchStop);
    Is("Chipo, buenas noches", BuddyIntent::Bedtime);
    Is("modo dormir", BuddyIntent::Bedtime);
    Is("buenos días", BuddyIntent::Wake);
    Is("sal del modo dormir", BuddyIntent::Wake);
    Is("qué hora es", BuddyIntent::AskTime);
    Is("¿Qué hora es?", BuddyIntent::AskTime);
    Is("cuánto queda", BuddyIntent::TimeLeft);
    Is("para", BuddyIntent::Dismiss);
    Is("Chipo, vale", BuddyIntent::Dismiss);

    std::printf("== ordinary speech is not a command\n");
    Is("", BuddyIntent::None);
    Is("qué tiempo hace hoy", BuddyIntent::None);
    Is("buenas noches a todos los que nos escuchan esta noche en el programa", BuddyIntent::None);
    Is("cuéntame un chiste sobre un temporizador que se rompió el mes pasado en la cocina de mi abuela", BuddyIntent::None);
    Is("para qué sirve un cronómetro de cocina", BuddyIntent::None);
    Is("la hora de la verdad", BuddyIntent::None);
    Is("vamos a dormir un rato más tarde", BuddyIntent::None);
    Is(nullptr, BuddyIntent::None);

    std::printf("== engine: timer\n");
    {
        BuddyEngine e;
        BuddyWallTime w;
        e.StartTimer(60, 1000);
        CHECK(e.timer_active() && e.TimerRemainingMs(1000) == 60000, "timer starts");
        CHECK(e.Update(30000, w).count == 0, "not yet");
        CHECK(e.TimerRemainingMs(31000) == 30000, "remaining");
        BuddyEvents ev = e.Update(61000, w);
        CHECK(ev.count == 1 && ev.kind[0] == BuddyEventKind::TimerDone, "timer done");
        CHECK(!e.timer_active() && e.Update(62000, w).count == 0, "rings once");
        // 32-bit wrap
        e.StartTimer(10, 0xFFFFFFF0u);
        CHECK(e.Update(0xFFFFFFF0u + 5000, w).count == 0, "wrap: not yet");
        CHECK(e.Update(0xFFFFFFF0u + 10000, w).count == 1, "wrap: done");
        e.StartTimer(60, 0);
        e.CancelCountdowns();
        CHECK(e.Update(120000, w).count == 0, "cancelled timer is silent");
    }

    std::printf("== engine: pomodoro (4 rounds, long break)\n");
    {
        BuddyEngine e;
        BuddyWallTime w;
        uint32_t t = 0;
        e.StartPomodoro(t);
        int breaks = 0, longs = 0, works = 0;
        for (int step = 0; step < 4 * 60 * 60; ++step) {  // simulate 4 h at 1 s
            t += 1000;
            const BuddyEvents ev = e.Update(t, w);
            for (int i = 0; i < ev.count; ++i) {
                breaks += ev.kind[i] == BuddyEventKind::PomodoroBreak;
                longs += ev.kind[i] == BuddyEventKind::PomodoroLongBreak;
                works += ev.kind[i] == BuddyEventKind::PomodoroWork;
            }
            if (longs == 1 && works == 3) break;
        }
        CHECK(breaks == 3 && longs == 1, "3 short breaks then a long one (%d,%d)", breaks, longs);
        t += BuddyEngine::kLongBreakMs + 1000;
        e.Update(t, w);
        CHECK(e.pomodoro_phase() == PomodoroPhase::Work && e.pomodoro_cycle() == 1, "after the long break a new round starts");
        e.StopPomodoro();
        CHECK(e.pomodoro_phase() == PomodoroPhase::Off, "stopped");
    }

    std::printf("== engine: stopwatch\n");
    {
        BuddyEngine e;
        e.StartStopwatch(1000);
        CHECK(e.StopwatchMs(4500) == 3500, "counts");
        e.StopStopwatch(6000);
        CHECK(e.StopwatchMs(99999) == 5000 && !e.stopwatch_running(), "frozen after stop");
        uint32_t v = 0;
        CHECK(e.Best(0, &v) == BuddyShow::Stopwatch && v == 5000, "stopped stopwatch is still shown");
    }

    std::printf("== engine: alarm\n");
    {
        BuddyEngine e;
        BuddyWallTime w;
        e.SetAlarm(7, 30);
        w.valid = true; w.hour = 7; w.minute = 29;
        CHECK(e.Update(1000, w).count == 0, "before");
        w.valid = false; w.hour = 7; w.minute = 30;
        CHECK(e.Update(2000, w).count == 0, "no real time known: never rings");
        w.valid = true;
        BuddyEvents ev = e.Update(3000, w);
        CHECK(ev.count == 1 && ev.kind[0] == BuddyEventKind::AlarmRing, "rings at 07:30");
        CHECK(e.Update(4000, w).count == 0 && !e.alarm_set(), "one shot");
        e.SetAlarm(25, 99);
        CHECK(e.alarm_hour() == 23 && e.alarm_minute() == 59, "values clamped");
    }

    std::printf("== engine: what to show\n");
    {
        BuddyEngine e;
        uint32_t v = 0;
        CHECK(e.Best(0, &v) == BuddyShow::Nothing, "nothing");
        e.StartTimer(100, 0);
        CHECK(e.Best(0, &v) == BuddyShow::Timer && v == 100000, "timer");
        e.StartPomodoro(0);
        CHECK(e.Best(0, &v) == BuddyShow::Pomodoro, "pomodoro wins over timer");
    }

    if (g_fail == 0) std::printf("ALL BUDDY LOGIC TESTS PASSED\n");
    return g_fail == 0 ? 0 : 1;
}
