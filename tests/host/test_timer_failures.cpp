// What happens when LVGL cannot create a timer (out of memory): the small modules (ambient, pet, reminders and the
// BUDDY clock effect) must fail cleanly and be startable again. The linker (-Wl,--wrap=lv_timer_create) routes the
// calls to the function below so a test can make the next ones fail.
#include <cstdio>
#include <string>

#include "chipo_ambient.h"
#include "chipo_buddy_view.h"
#include "chipo_pet.h"
#include "chipo_remind.h"

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); ++g_fail; } } while (0)

static uint32_t g_now = 1000;
static uint32_t FakeTick() { return g_now; }
static uint8_t g_buf[480 * 40 * 2];
static void Flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

static bool g_fail_timer = false;
extern "C" lv_timer_t* __real_lv_timer_create(lv_timer_cb_t cb, uint32_t period, void* user_data);
extern "C" lv_timer_t* __wrap_lv_timer_create(lv_timer_cb_t cb, uint32_t period, void* user_data) {
    return g_fail_timer ? nullptr : __real_lv_timer_create(cb, period, user_data);
}
static int CountTimers() {
    int n = 0;
    for (lv_timer_t* t = lv_timer_get_next(nullptr); t; t = lv_timer_get_next(t)) ++n;
    return n;
}

int main() {
    lv_init();
    lv_tick_set_cb(FakeTick);
    lv_display_t* d = lv_display_create(480, 480);
    lv_display_set_buffers(d, g_buf, nullptr, sizeof(g_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, Flush);
    const int timers = CountTimers();

    std::printf("== ambient sound\n");
    {
        int clips = 0;
        ChipoAmbient a;
        auto hooks = [&] {
            ChipoAmbient::Hooks h;
            h.clip_exists = [](const char*) { return true; };
            h.play_clip = [&](const char*) { ++clips; return true; };
            h.clock = [] { return g_now; };
            return h;
        };
        g_fail_timer = true;
        const bool ok = a.Start(hooks());
        g_fail_timer = false;
        CHECK(!ok, "ambient: Start must report the failure");
        CHECK(CountTimers() == timers, "ambient: no timer left behind");
        a.Step(g_now += 60000);
        CHECK(clips == 0, "ambient: a module that did not start must stay silent (%d)", clips);
        CHECK(a.Start(hooks()) && CountTimers() == timers + 1, "ambient: Start works again");
        a.Stop();
        CHECK(CountTimers() == timers, "ambient: Stop leaves no timer");
    }

    std::printf("== pet\n");
    {
        int notes = 0;
        ChipoPet p;
        auto hooks = [&] {
            ChipoPet::Hooks h;
            h.play_clip = [](const char*) { return true; };
            h.notify = [&](const char*) { ++notes; };
            h.rand = [](int, int) { return 0; };
            h.clock = [] { return g_now; };
            return h;
        };
        g_fail_timer = true;
        const bool ok = p.Start(hooks());
        g_fail_timer = false;
        CHECK(!ok, "pet: Start must report the failure");
        CHECK(CountTimers() == timers, "pet: no timer left behind");
        p.Step(g_now += 600000);
        CHECK(notes == 0, "pet: a module that did not start must stay quiet (%d)", notes);
        CHECK(p.Start(hooks()) && CountTimers() == timers + 1, "pet: Start works again");
        p.Stop();
        CHECK(CountTimers() == timers, "pet: Stop leaves no timer");
    }

    std::printf("== reminders\n");
    {
        int rings = 0;
        ChipoRemind r;
        auto hooks = [&] {
            ChipoRemind::Hooks h;
            h.play_clip = [&](const char*) { ++rings; return true; };
            h.wall_time = [](ChipoRemind::Wall* w) { w->valid = true; w->hour = 9; w->minute = 0; return true; };
            h.load = [] { return std::string(); };
            h.save = [](const std::string&) {};
            h.clock = [] { return g_now; };
            return h;
        };
        g_fail_timer = true;
        const bool ok = r.Start(hooks());
        g_fail_timer = false;
        CHECK(!ok, "remind: Start must report the failure");
        CHECK(CountTimers() == timers, "remind: no timer left behind");
        r.Step(g_now += 120000);
        CHECK(rings == 0, "remind: a module that did not start must not ring (%d)", rings);
        CHECK(r.Start(hooks()) && CountTimers() == timers + 1, "remind: Start works again");
        r.Stop();
        CHECK(CountTimers() == timers, "remind: Stop leaves no timer");
    }

    std::printf("== BUDDY clock effect\n");
    {
        ChipoBuddyView v;
        CHECK(v.Create(lv_screen_active(), LV_FONT_DEFAULT), "view created");
        BuddyScreen s;
        g_fail_timer = true;
        v.Show(s);  // switch-on effect: with no timer it must end at once instead of leaving the digits invisible
        auto dbg = v.Debug();
        CHECK(dbg.fx == 0 && dbg.root_visible, "switch-on without a timer: effect over, clock visible (fx=%d)", dbg.fx);
        v.Hide();  // switch-off effect: ends at once too and hides the clock
        dbg = v.Debug();
        CHECK(dbg.fx == 0 && !dbg.root_visible, "switch-off without a timer: effect over, clock hidden (fx=%d)", dbg.fx);
        g_fail_timer = false;
        v.Show(s);  // and with timers it animates as before
        CHECK(v.Debug().fx == 1, "switch-on with a timer still animates (fx=%d)", v.Debug().fx);
        v.Destroy();
        CHECK(CountTimers() == timers, "view: Destroy leaves no timer");
    }

    if (g_fail == 0) std::printf("ALL TIMER-FAILURE TESTS PASSED\n");
    return g_fail == 0 ? 0 : 1;
}
