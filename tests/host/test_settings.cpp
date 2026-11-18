// CHIPO SETTINGS on a real LVGL build: menu, battery, screen saver, burn-in shift, taps. Simulated time.
#include <cstdio>
#include <string>
#include <vector>

#include "chipo_settings.h"

static int g_failures = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_failures; std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static uint32_t g_now = 1000;
static uint32_t FakeTick() { return g_now; }
static uint8_t g_buf[368 * 40 * 4];
static void Flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

struct Rig {
    ChipoSettings s;
    int volume = 70, brightness = 80, applied_brightness = 80;
    int level = 60;
    bool charging = false, battery_ok = true, overlay = false, awake = false, restarted = false;
    std::vector<std::string> notes, clips;
    std::vector<TapKind> reactions;
    int shifts = 0, sdx = 0, sdy = 0;
    bool Start() {
        ChipoSettings::Hooks h;
        h.get_volume = [this] { return volume; };
        h.set_volume = [this](int v) { volume = v; };
        h.get_brightness = [this] { return brightness; };
        h.set_brightness = [this](int v, bool permanent) { applied_brightness = v; if (permanent) brightness = v; };
        h.battery = [this](int* l, bool* c, bool* d) { *l = level; *c = charging; *d = !charging; return battery_ok; };
        h.wifi = [](WifiView* w) { w->connected = true; w->ssid = "Casa"; w->rssi_dbm = -55; return true; };
        h.restart = [this] { restarted = true; };
        h.notify = [this](const char* t) { notes.push_back(t); };
        h.play_clip = [this](const char* id) { clips.push_back(id); return true; };
        h.react = [this](TapKind k) { reactions.push_back(k); };
        h.shift = [this](int dx, int dy) { ++shifts; sdx = dx; sdy = dy; };
        h.keep_awake = [this] { return awake; };
        h.overlay_active = [this] { return overlay; };
        h.clock = [] { return g_now; };
        return s.Start(lv_screen_active(), std::move(h), "0.6.0");
    }
    void Run(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 1000) { g_now += 1000; s.Step(g_now); } lv_timer_handler(); }
};

int main() {
    lv_init();
    lv_tick_set_cb(FakeTick);
    lv_display_t* d = lv_display_create(368, 448);
    lv_display_set_buffers(d, g_buf, nullptr, sizeof(g_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, Flush);

    std::printf("== menu\n");
    {
        Rig r; CHECK(r.Start(), "start");
        CHECK(!r.s.open(), "closed at first");
        r.s.OnLongPress();
        CHECK(r.s.open(), "long press opens it");
        lv_timer_handler();
        r.s.Press(ChipoSettings::kVolUp);
        CHECK(r.volume == 80 && r.s.Debug().volume == 80, "volume +10 (%d)", r.volume);
        CHECK(!r.clips.empty() && r.clips.back() == "buddy_chime", "a chime lets you hear the new level");
        for (int i = 0; i < 5; ++i) r.s.Press(ChipoSettings::kVolUp);
        CHECK(r.volume == 100, "volume capped at 100");
        for (int i = 0; i < 12; ++i) r.s.Press(ChipoSettings::kVolDown);
        CHECK(r.volume == 0, "volume down to 0");
        for (int i = 0; i < 12; ++i) r.s.Press(ChipoSettings::kBriDown);
        CHECK(r.brightness == 5, "brightness never below 5 (%d)", r.brightness);
        r.s.Press(ChipoSettings::kBriUp);
        CHECK(r.brightness == 15, "brightness +10");
        r.s.Press(ChipoSettings::kRestart);
        CHECK(!r.restarted && r.s.Debug().restart_armed, "restart asks to be confirmed");
        r.Run(4000);
        CHECK(!r.s.Debug().restart_armed, "confirmation expires after 3 s");
        r.s.Press(ChipoSettings::kRestart);
        r.s.Press(ChipoSettings::kRestart);
        CHECK(r.restarted, "second tap restarts");
        r.s.Press(ChipoSettings::kClose);
        CHECK(!r.s.open(), "close");
        r.s.Press(ChipoSettings::kVolUp);
        CHECK(r.volume == 0, "buttons do nothing while closed");
        r.s.Open();
        r.Run(29000);
        CHECK(r.s.open(), "still open after 29 s");
        r.s.OnPressed();
        r.Run(29000);
        CHECK(r.s.open(), "a touch keeps it open");
        r.Run(2000);
        CHECK(!r.s.open(), "closes itself after 30 s without touches");
        r.overlay = true;
        r.s.OnLongPress();
        CHECK(!r.s.open(), "no menu over the weather, clock or games");
        r.s.Stop();
        r.s.Step(g_now);
    }

    std::printf("== weather taps and face taps\n");
    {
        Rig r; r.Start();
        CHECK(r.s.BlocksWeatherTap(200) && !r.s.BlocksWeatherTap(40) && !r.s.BlocksWeatherTap(420), "the face band is not for the weather");
        r.overlay = true;
        CHECK(r.s.BlocksWeatherTap(40), "nothing opens the weather over another screen");
        r.overlay = false;
        r.s.OnTap(180, 200);
        CHECK(r.reactions.size() == 1 && r.reactions[0] == TapKind::Poke, "tap on the face pokes it");
        r.s.OnTap(180, 400);
        CHECK(r.reactions.size() == 1, "taps outside the face band are ignored here");
        g_now += 300; r.s.OnTap(180, 200); g_now += 300; r.s.OnTap(180, 200);
        CHECK(r.reactions.size() == 3 && r.reactions.back() == TapKind::Tickle, "three quick taps tickle");
        CHECK(r.clips.back() == "murmur_2", "and it giggles");
        r.s.OnLongPress();
        CHECK(r.s.BlocksWeatherTap(40), "menu open blocks the weather");
        const size_t n = r.reactions.size();
        r.s.OnTap(180, 200);
        CHECK(r.reactions.size() == n, "no face reactions under the menu");
    }

    std::printf("== battery\n");
    {
        Rig r; r.Start();
        r.Run(6000);
        CHECK(!r.s.Debug().indicator_visible && r.notes.empty(), "normal battery: out of the way");
        r.charging = true; r.level = 70;
        r.Run(6000);
        CHECK(r.s.Debug().indicator_visible && r.s.Debug().battery_state == BatteryState::Charging, "charging: indicator");
        r.charging = false; r.level = 16;
        r.Run(6000);
        CHECK(!r.s.Debug().indicator_visible && r.notes.empty(), "16 %% is not low yet");
        r.level = 15;
        r.Run(6000);
        CHECK(r.s.Debug().indicator_visible && r.s.Debug().battery_state == BatteryState::Low, "low: indicator");
        CHECK(r.notes.size() == 1 && r.notes[0] == "Batería baja: 15 %", "warning at 15 %% (%zu)", r.notes.size());
        r.level = 14; r.Run(6000);
        CHECK(r.notes.size() == 1, "once per threshold");
        r.level = 5; r.Run(6000);
        CHECK(r.notes.size() == 2 && r.s.Debug().battery_state == BatteryState::Critical, "warning at 5 %%");
        for (int i = 0; i < 4; ++i) r.Run(1000);  // blinking: must not crash
        r.battery_ok = false; r.Run(6000);
        CHECK(!r.s.Debug().indicator_visible && r.s.Debug().battery_state == BatteryState::Unknown, "no reading: no indicator, nothing invented");
    }

    std::printf("== screen saver and burn-in shift\n");
    {
        Rig r; r.Start();
        r.Run(60000);
        CHECK(!r.s.Debug().dimmed && r.applied_brightness == 80, "awake for the first minute");
        r.Run(40000);
        CHECK(r.s.Debug().dimmed && r.applied_brightness == 32, "dim to 40 %% after 90 s (%d)", r.applied_brightness);
        r.Run(210000);
        CHECK(r.applied_brightness == 12, "darker after 5 min (%d)", r.applied_brightness);
        r.s.NotifyActivity();
        CHECK(!r.s.Debug().dimmed && r.applied_brightness == 80, "a touch restores the user's level (%d)", r.applied_brightness);
        r.Run(100000);
        CHECK(r.s.Debug().dimmed, "dims again");
        r.awake = true;
        r.Run(5000);
        CHECK(!r.s.Debug().dimmed && r.applied_brightness == 80, "never dimmed while something is happening");
        r.awake = false;
        r.Run(60000);
        CHECK(!r.s.Debug().dimmed, "the idle clock restarted");
        CHECK(r.shifts > 0 && r.sdx >= -3 && r.sdx <= 3 && r.sdy >= -3 && r.sdy <= 3, "the face drifted (%d changes)", r.shifts);
        r.Run(120000);
        r.s.Open();
        CHECK(r.applied_brightness == 80 || !r.s.Debug().dimmed, "opening the menu wakes the screen");
        r.Run(60000);
        CHECK(!r.s.Debug().dimmed || !r.s.open(), "no dimming while the menu is open");
        r.s.Stop();
    }

    if (g_failures == 0) std::printf("ALL SETTINGS TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
