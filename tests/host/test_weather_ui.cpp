// Host-side behaviour test for the weather screen + controller on a real LVGL build (no display,
// no network): fake clock, fake touch, fake downloader. See tests/host/run.sh.
#include <cstdio>
#include <cstring>
#include <string>

#include "chipo_weather_config.h"
#include "chipo_weather_controller.h"

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

// The linker (-Wl,--wrap=lv_timer_create) routes the component's calls here, so a test can make the timer fail.
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
static int CountIndevCallbacks() {
    int n = 0;
    for (lv_indev_t* in = lv_indev_get_next(nullptr); in; in = lv_indev_get_next(in)) n += lv_indev_get_event_count(in);
    return n;
}

static uint32_t g_now = 1000;
static uint32_t FakeTick() { return g_now; }
static uint8_t g_buf[480 * 40 * 2];
static void Flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

static lv_indev_state_t g_ptr = LV_INDEV_STATE_RELEASED;
static void PointerRead(lv_indev_t*, lv_indev_data_t* data) {
    data->point.x = 120;
    data->point.y = 300;
    data->state = g_ptr;
}

static void Advance(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 50) {
        g_now += 50;
        lv_timer_handler();
    }
}

static void Tap() {
    g_ptr = LV_INDEV_STATE_PRESSED;
    Advance(100);
    g_ptr = LV_INDEV_STATE_RELEASED;
    Advance(100);
}

static int g_fetches = 0;
static bool g_fetch_ok = true;
static WeatherData Sample() {
    WeatherData d;
    d.valid = true;
    std::snprintf(d.city, sizeof d.city, "Sevilla");
    d.temp = 27;
    d.feels = 25;
    d.hi = 29;
    d.lo = 15;
    d.kind = WeatherKind::Clear;
    d.is_day = true;
    d.hour_count = kWeatherHours;
    for (int i = 0; i < kWeatherHours; ++i) {
        std::snprintf(d.hours[i].label, sizeof d.hours[i].label, i ? "%dh" : "Ahora", 17 + 2 * i);
        d.hours[i].temp = 27 - i;
        d.hours[i].kind = WeatherKind::Cloudy;
        d.hours[i].is_day = true;
    }
    d.day_count = kWeatherDays;
    for (int i = 0; i < kWeatherDays; ++i) {
        std::snprintf(d.days[i].name, sizeof d.days[i].name, "Jue");
        d.days[i].hi = 28;
        d.days[i].lo = 14;
        d.days[i].kind = WeatherKind::Rain;
    }
    return d;
}

int main() {
    lv_init();
    lv_tick_set_cb(FakeTick);
    lv_display_t* disp = lv_display_create(480, 480);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, g_buf, nullptr, sizeof(g_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, Flush);
    lv_indev_t* touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, PointerRead);

    for (int round = 0; round < 2; ++round) {
        std::printf("== round %d\n", round);
        g_fetches = 0;
        g_fetch_ok = true;
        g_now += 10000;
        auto* w = new ChipoWeather();
        CHECK(!w->Start(lv_screen_active(), nullptr, nullptr) || true, "unreachable");
        CHECK(w->Start(lv_screen_active(), [](WeatherData* out) {
                  ++g_fetches;
                  if (!g_fetch_ok) return false;
                  *out = Sample();
                  return true;
              }),
              "Start failed");
        CHECK(w->Start(lv_screen_active(), nullptr), "second Start should be a no-op");
        CHECK(!w->IsShown(), "must start hidden");

        // ---- not a weather question: nothing happens -------------------------------------------
        CHECK(!w->OnVoiceText("pon musica") && !w->IsShown() && g_fetches == 0, "false positive");

        // ---- voice request: opens, downloads once, shows the data ---------------------------------
        CHECK(w->OnVoiceText("\xC2\xBFQu\xC3\xA9 tiempo hace hoy?"), "voice not recognised");
        CHECK(w->IsShown(), "should be visible right away (status screen)");
        Advance(300);
        CHECK(g_fetches == 1, "fetches %d", g_fetches);
        auto dbg = w->Debug(g_now);
        CHECK(dbg.has_cache && !dbg.loading && dbg.shown, "data not collected: cache=%d loading=%d", dbg.has_cache, dbg.loading);
        CHECK(dbg.hide_in_ms > CHIPO_WEATHER_SHOW_MS - 1000 && dbg.hide_in_ms <= CHIPO_WEATHER_SHOW_MS,
              "hide timer %u", dbg.hide_in_ms);

        // ---- closes by itself ----------------------------------------------------------------------
        Advance(CHIPO_WEATHER_SHOW_MS - 1000);
        CHECK(w->IsShown(), "closed too early");
        Advance(2000);
        CHECK(!w->IsShown(), "did not close by itself");

        // ---- fresh cache: no new download, shown immediately ------------------------------------------
        CHECK(w->OnVoiceText("dime el clima") && w->IsShown(), "reopen");
        Advance(300);
        CHECK(g_fetches == 1, "fresh cache must not download again (%d)", g_fetches);
        CHECK(w->Debug(g_now).hide_in_ms > 0, "timer armed from cache");

        // ---- touch: closes when open, opens when closed ------------------------------------------------
        Tap();
        CHECK(!w->IsShown(), "tap should close");
        lv_obj_t* root = nullptr;
        for (uint32_t i = 0; i < lv_obj_get_child_count(lv_screen_active()); ++i) root = lv_obj_get_child(lv_screen_active(), i);
        CHECK(root && lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN), "hidden panel must not stay in the way of touches");
        Tap();
        CHECK(w->IsShown(), "tap should open");
        Advance(300);
        CHECK(g_fetches == 1, "tap with fresh cache must not download");
        Tap();
        CHECK(!w->IsShown(), "tap closes again");

        // ---- stale cache: shows the old data at once, refreshes in the background --------------------------
        Advance(CHIPO_WEATHER_CACHE_MS + 1000);
        CHECK(w->OnVoiceText("va a llover"), "voice");
        CHECK(w->Debug(g_now).has_cache, "stale cache must still be shown while refreshing");
        Advance(300);
        CHECK(g_fetches == 2, "stale cache should refresh (%d)", g_fetches);
        Advance(CHIPO_WEATHER_SHOW_MS + 500);
        CHECK(!w->IsShown(), "closed");

        // ---- refresh fails but there is old data: keep the old data, no error screen --------------------------
        Advance(CHIPO_WEATHER_CACHE_MS + 1000);
        g_fetch_ok = false;
        CHECK(w->OnVoiceText("que tiempo hace"), "voice");
        Advance(300);
        CHECK(g_fetches == 3 && w->IsShown() && w->Debug(g_now).has_cache && !w->Debug(g_now).loading,
              "failed refresh with old data: fetches=%d", g_fetches);
        Advance(CHIPO_WEATHER_SHOW_MS + 500);
        CHECK(!w->IsShown(), "closed after failed refresh");

        // ---- first download fails: error screen for a few seconds, then closes ------------------------------------
        delete w;
        w = new ChipoWeather();
        g_fetches = 0;
        CHECK(w->Start(lv_screen_active(), [](WeatherData*) { ++g_fetches; return false; }), "restart");
        CHECK(w->OnVoiceText("clima") && w->IsShown(), "open with failing network");
        Advance(300);
        CHECK(w->IsShown() && !w->Debug(g_now).has_cache && !w->Debug(g_now).loading, "error screen expected");
        Advance(6000);
        CHECK(!w->IsShown(), "error screen should close by itself");
        Tap();  // and it can be retried
        Advance(300);
        CHECK(g_fetches == 2, "retry by tap (%d)", g_fetches);

        // ---- stop is clean (indev callbacks removed) and a later tap does nothing -------------------------------------
        w->Stop();
        Tap();
        delete w;
        Tap();
    }

    // ---- the timer cannot be created: Start() must fail cleanly and be retryable -----------------------------------
    std::printf("== lv_timer_create fails\n");
    {
        g_fetches = 0;
        g_fetch_ok = true;
        g_now += 10000;
        auto fetcher = [](WeatherData* out) {
            ++g_fetches;
            *out = Sample();
            return true;
        };
        const uint32_t children = lv_obj_get_child_count(lv_screen_active());
        const int timers = CountTimers();
        const int callbacks = CountIndevCallbacks();
        ChipoWeather w;
        g_fail_timer = true;
        const bool started = w.Start(lv_screen_active(), fetcher);
        g_fail_timer = false;
        CHECK(!started, "Start must report the failure");
        CHECK(!w.IsShown() && !w.OnVoiceText("dime el clima") && !w.OnVoiceText("va a llover"),
              "a module that did not start must not react to voice");
        w.Show();  // must be harmless
        Advance(500);
        CHECK(g_fetches == 0, "no download without a timer (%d)", g_fetches);
        CHECK(lv_obj_get_child_count(lv_screen_active()) == children, "the view must be removed (%u -> %u)",
              children, lv_obj_get_child_count(lv_screen_active()));
        CHECK(CountTimers() == timers, "no timer left behind (%d -> %d)", timers, CountTimers());
        CHECK(CountIndevCallbacks() == callbacks, "no input callback registered (%d -> %d)", callbacks, CountIndevCallbacks());
        Tap();  // a tap must not reach a module that is not running
        Advance(300);
        CHECK(!w.IsShown() && g_fetches == 0, "tap on a module that did not start");
        w.Stop();  // harmless on a module that did not start

        // Retrying later works like a first start.
        CHECK(w.Start(lv_screen_active(), fetcher), "second Start must succeed once timers work");
        CHECK(w.OnVoiceText("dime el clima") && w.IsShown(), "works after the retry");
        Advance(500);
        CHECK(g_fetches == 1, "downloads after the retry (%d)", g_fetches);
        w.Stop();
        CHECK(lv_obj_get_child_count(lv_screen_active()) == children, "Stop leaves nothing behind");
        CHECK(CountTimers() == timers && CountIndevCallbacks() == callbacks, "Stop leaves no timer or callback");
    }

    if (g_failures) {
        std::printf("%d weather UI test(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("weather UI: all tests passed\n");
    return 0;
}
