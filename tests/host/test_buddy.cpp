// CHIPO BUDDY on a real LVGL build: timer, pomodoro, alarm, ringing, bedside mode. Simulated time.
#include <cstdio>
#include <string>
#include <vector>

#include "chipo_buddy.h"

static int g_failures = 0;
#define CHECK(cond, ...)                              \
    do {                                              \
        if (!(cond)) {                                \
            ++g_failures;                             \
            std::printf("FAIL line %d: ", __LINE__);  \
            std::printf(__VA_ARGS__);                 \
            std::printf("\n");                        \
        }                                             \
    } while (0)

static uint32_t g_now = 1000;
static uint32_t FakeTick() { return g_now; }
static uint8_t g_buf[480 * 40 * 4];
static void Flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

struct Harness {
    ChipoBuddy buddy;
    std::vector<std::string> clips, notes, scenes;
    bool clips_exist = true, busy = false, drive = false;
    BuddyWallTime wall;

    bool Start() {
        ChipoBuddy::Hooks h;
        h.play_clip = [this](const char* id) { if (!clips_exist) return false; clips.push_back(id); return true; };
        h.notify = [this](const char* t) { notes.push_back(t); };
        h.assistant_busy = [this] { return busy; };
        h.in_drive = [this] { return drive; };
        h.clock = [] { return g_now; };
        h.wall_time = [this](BuddyWallTime* o) { *o = wall; return wall.valid; };
        h.scene = [this](const char* w) { scenes.push_back(w); };
        return buddy.Start(lv_screen_active(), std::move(h));
    }
    void Run(uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += 250) {
            g_now += 250;
            buddy.Step(g_now);
            lv_timer_handler();
        }
    }
    void SetWall(int h, int m, int s = 0) { wall.valid = true; wall.hour = h; wall.minute = m; wall.second = s; }
    int Count(const char* id) const { int n = 0; for (const auto& c : clips) n += c == id; return n; }
};

int main() {
    lv_init();
    lv_tick_set_cb(FakeTick);
    lv_display_t* d = lv_display_create(480, 480);
    lv_display_set_buffers(d, g_buf, nullptr, sizeof(g_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, Flush);

    std::printf("== timer: set, peek, hide, ring, stop by touch\n");
    {
        Harness h;
        CHECK(h.Start(), "start");
        h.Run(500);
        CHECK(!h.buddy.Debug().shown, "nothing on screen at start");
        CHECK(h.buddy.OnVoiceText("Chipo, pon un temporizador de 10 minutos"), "timer command answered by CHIPO");
        CHECK(h.Count("buddy_chime") == 1 && !h.notes.empty(), "chime and text");
        h.Run(500);
        auto dbg = h.buddy.Debug();
        CHECK(dbg.shown && std::string(dbg.digits) == "0959", "peek shows the countdown (%s)", dbg.digits);
        h.Run(7000);
        CHECK(h.buddy.Debug().shown, "countdown stays visible until dismissed");
        CHECK(h.buddy.engine().timer_active(), "the timer keeps running");
        h.buddy.OnTouch(false);
        CHECK(!h.buddy.Debug().shown, "a tap hides the countdown without cancelling it");
        CHECK(h.buddy.engine().timer_active(), "the timer remains active after hiding");

        CHECK(h.buddy.OnVoiceText("cuánto queda"), "time left answered");
        h.Run(500);
        CHECK(h.buddy.Debug().shown, "time left shows the screen");
        h.Run(7000);

        h.Run(10u * 60u * 1000u);
        CHECK(h.buddy.ringing(), "timer rings");
        CHECK(h.buddy.Debug().shown && h.buddy.Debug().color == 0xFF4A3D, "red ringing screen");
        const int rings = h.Count("buddy_timer_done");
        CHECK(rings >= 1, "ring sound played");
        h.Run(9000);
        CHECK(h.Count("buddy_timer_done") > rings, "it repeats while nobody answers");
        h.buddy.OnTouch(false);
        CHECK(!h.buddy.ringing(), "a tap stops it");
        h.Run(500);
        CHECK(!h.buddy.Debug().shown, "screen closes");
    }

    std::printf("== timer: voice stop, cancel, auto end\n");
    {
        Harness h;
        h.Start();
        h.buddy.OnVoiceText("temporizador de 1 minuto");
        h.Run(65000);
        CHECK(h.buddy.ringing(), "rings");
        CHECK(h.buddy.OnVoiceText("para"), "voice 'para' stops it");
        CHECK(!h.buddy.ringing(), "stopped");
        CHECK(!h.buddy.OnVoiceText("para"), "'para' when nothing rings is left to the assistant");
        h.buddy.OnVoiceText("temporizador de 5 minutos");
        CHECK(h.buddy.OnVoiceText("cancela el temporizador"), "cancel");
        h.Run(6 * 60 * 1000);
        CHECK(!h.buddy.ringing(), "cancelled timer does not ring");
        const int before = h.Count("buddy_timer_done");
        h.buddy.OnVoiceText("temporizador de 1 minuto");
        h.Run(65000 + 60000);
        CHECK(!h.buddy.ringing(), "nobody answers: it ends by itself");
        CHECK(h.Count("buddy_timer_done") - before == 8, "after 8 rings (%d)", h.Count("buddy_timer_done") - before);
    }

    std::printf("== pomodoro\n");
    {
        Harness h;
        h.Start();
        CHECK(h.buddy.OnVoiceText("pomodoro"), "pomodoro starts");
        h.Run(500);
        CHECK(std::string(h.buddy.Debug().digits) == "2459" || std::string(h.buddy.Debug().digits) == "2500", "25 min (%s)", h.buddy.Debug().digits);
        h.Run(25u * 60u * 1000u + 1000);
        CHECK(h.buddy.engine().pomodoro_phase() == PomodoroPhase::Break, "break after 25 min");
        CHECK(h.Count("buddy_timer_done") == 1, "sound at the break");
        CHECK(h.buddy.Debug().shown, "screen shows the break for a moment");
        CHECK(!h.buddy.ringing(), "the pomodoro does not ring until touched");
        h.buddy.OnVoiceText("para el pomodoro");
        CHECK(h.buddy.engine().pomodoro_phase() == PomodoroPhase::Off, "stopped");
    }

    std::printf("== alarm: ring, snooze by tap, stop by voice\n");
    {
        Harness h;
        h.Start();
        h.SetWall(7, 20);
        CHECK(h.buddy.OnVoiceText("despiértame a las 7 y media"), "alarm set");
        CHECK(h.buddy.engine().alarm_set() && h.buddy.engine().alarm_hour() == 7 && h.buddy.engine().alarm_minute() == 30, "07:30");
        h.Run(1000);
        h.SetWall(7, 30);
        h.Run(500);
        CHECK(h.buddy.ringing(), "rings at 07:30");
        CHECK(h.Count("buddy_alarm") >= 1, "alarm sound");
        CHECK(std::string(h.buddy.Debug().digits) == "0730", "the screen shows the time (%s)", h.buddy.Debug().digits);
        h.buddy.OnTouch(false);
        CHECK(!h.buddy.ringing() && h.buddy.engine().alarm_set() && h.buddy.engine().alarm_minute() == 39, "tap snoozes 9 minutes");
        h.SetWall(7, 39);
        h.Run(500);
        CHECK(h.buddy.ringing(), "rings again");
        CHECK(h.buddy.OnVoiceText("vale"), "voice stops it");
        CHECK(!h.buddy.engine().alarm_set(), "no more alarm");
        CHECK(!h.scenes.empty() && h.scenes.back() == "morning", "morning scene requested");
    }

    std::printf("== alarm: no real time known\n");
    {
        Harness h;
        h.Start();
        h.buddy.OnVoiceText("alarma a las 7");
        h.Run(2000);
        CHECK(!h.buddy.ringing(), "without a valid clock it never rings");
    }

    std::printf("== bedtime\n");
    {
        Harness h;
        h.Start();
        h.SetWall(23, 5);
        CHECK(!h.buddy.OnVoiceText("buenos días"), "'buenos días' outside bedtime: the assistant answers");
        CHECK(h.buddy.OnVoiceText("Chipo, buenas noches"), "bedtime starts");
        CHECK(h.buddy.bedtime() && !h.scenes.empty() && h.scenes[0] == "night", "night scene requested");
        h.Run(500);
        auto dbg = h.buddy.Debug();
        CHECK(dbg.shown && std::string(dbg.digits) == "2305", "dim clock (%s)", dbg.digits);
        h.buddy.OnTouch(false);
        h.Run(500);
        CHECK(h.buddy.bedtime() && h.buddy.Debug().shown, "a short tap (a knock on the table) does not wake it");
        h.SetWall(23, 6);
        h.Run(500);
        CHECK(std::string(h.buddy.Debug().digits) == "2306", "clock follows the time");
        h.buddy.OnVoiceText("despiértame a las 7");
        h.Run(500);
        h.buddy.OnTouch(true);
        CHECK(!h.buddy.bedtime(), "long press leaves bedtime");
        CHECK(h.scenes.back() == "morning", "morning scene");
        h.Run(500);
        CHECK(!h.buddy.Debug().shown, "screen back to the face");

        h.buddy.OnVoiceText("modo dormir");
        CHECK(h.buddy.bedtime(), "again");
        CHECK(h.buddy.OnVoiceText("buenos días"), "voice wakes it");
        CHECK(!h.buddy.bedtime(), "awake");

        h.drive = true;
        CHECK(!h.buddy.OnVoiceText("buenas noches") && !h.buddy.bedtime(), "no bedtime while driving");
    }

    std::printf("== bedtime ends with the alarm\n");
    {
        Harness h;
        h.Start();
        h.SetWall(23, 0);
        h.buddy.OnVoiceText("alarma a las 7");
        h.buddy.OnVoiceText("buenas noches");
        h.SetWall(7, 0);
        h.Run(1000);
        CHECK(h.buddy.ringing() && h.buddy.bedtime(), "alarm rings in bedtime");
        h.buddy.OnVoiceText("para");
        CHECK(!h.buddy.bedtime() && h.scenes.back() == "morning", "stopping the alarm is good morning");
    }

    std::printf("== desk clock (CHIPO lying on its side)\n");
    {
        Harness h;
        h.Start();
        h.SetWall(9, 41);
        h.buddy.SetDeskClock(true, -1);
        h.Run(60);
        CHECK(h.buddy.Debug().fx == 1, "the clock lights up with an animation (fx %d)", h.buddy.Debug().fx);
        h.Run(440);
        auto dbg = h.buddy.Debug();
        CHECK(h.buddy.desk_clock() && dbg.shown && dbg.rotation == 1, "clock up, turned for the left side (rot %d)", dbg.rotation);
        CHECK(std::string(dbg.digits) == "0941" && dbg.brightness >= 200, "bright time (%s, %d)", dbg.digits, dbg.brightness);
        h.SetWall(9, 42);
        h.Run(500);
        CHECK(std::string(h.buddy.Debug().digits) == "0942", "follows the time");
        h.buddy.OnVoiceText("temporizador de 1 minuto");
        h.Run(8000);
        CHECK(h.buddy.Debug().shown && h.buddy.Debug().rotation == 1, "a countdown peek ending does not close the clock");
        h.Run(60000);
        CHECK(h.buddy.ringing() && h.buddy.Debug().rotation == 1, "the ring screen is turned too");
        h.buddy.OnTouch(false);
        h.Run(500);
        CHECK(h.buddy.Debug().shown, "after the ring the clock is still there");
        h.Run(1000);
        CHECK(h.buddy.Debug().fx == 0, "the appear animation ends");
        h.buddy.SetDeskClock(false, -1);
        h.Run(60);
        CHECK(!h.buddy.Debug().shown && h.buddy.Debug().fx == 2 && h.buddy.Debug().root_visible,
              "stood up: the digits switch off with an animation first");
        h.Run(700);
        CHECK(!h.buddy.Debug().shown && !h.buddy.Debug().root_visible && h.buddy.Debug().fx == 0 &&
              h.buddy.Debug().rotation == 0, "stood up: clock gone, upright again");
        h.buddy.SetDeskClock(true, +1);
        h.Run(500);
        CHECK(h.buddy.Debug().rotation == 2, "right side down turns the other way");
        h.buddy.SetDeskClock(false, +1);
        h.SetWall(0, 0);
        h.wall.valid = false;
        h.buddy.SetDeskClock(true, -1);
        h.Run(500);
        CHECK(h.buddy.Debug().shown && std::string(h.buddy.Debug().digits) == "    ", "no real time: blank display, no invented time");
        h.buddy.Stop();
    }

    std::printf("== no clips: still works, the assistant is not silenced\n");
    {
        Harness h;
        h.clips_exist = false;
        h.Start();
        CHECK(!h.buddy.OnVoiceText("temporizador de 3 minutos"), "no sound: server not silenced");
        CHECK(h.buddy.engine().timer_active() && !h.notes.empty(), "but the timer is set and the text shown");
        h.Run(500);
        CHECK(h.buddy.Debug().shown, "and the countdown is on the screen");
        h.buddy.Stop();
    }

    std::printf("== ordinary speech\n");
    {
        Harness h;
        h.Start();
        CHECK(!h.buddy.OnVoiceText("qué tiempo hace hoy"), "weather is not ours");
        CHECK(!h.buddy.OnVoiceText(nullptr), "null");
        CHECK(h.clips.empty() && h.notes.empty(), "nothing happened");
        CHECK(!h.buddy.OnVoiceText("cuánto queda"), "time left with nothing running: the assistant answers");
    }

    if (g_failures == 0) std::printf("ALL BUDDY TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
