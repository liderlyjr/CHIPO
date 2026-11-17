#pragma once

#include <functional>
#include <memory>

#include "chipo_weather.h"
#include "chipo_weather_view.h"
#include "lvgl.h"

/**
 * Opens the weather screen on a voice request or a tap, downloads the data on a worker task
 * (never on the LVGL task), shows it, and closes it after CHIPO_WEATHER_SHOW_MS or on a tap.
 *
 * Start() and every other method run on the LVGL task with the display lock held, like
 * ChipoFace. The worker never touches LVGL: it only fills a result that the LVGL timer picks up.
 */
class ChipoWeather {
public:
    // Blocking download + parse. Runs on a worker task (on a PC test: inline). Return false on failure.
    using Fetcher = std::function<bool(WeatherData* out)>;

    ChipoWeather();
    ~ChipoWeather();
    ChipoWeather(const ChipoWeather&) = delete;
    ChipoWeather& operator=(const ChipoWeather&) = delete;

    // `fetcher` defaults to the Open-Meteo client when built for the ESP32. `fonts` defaults to
    // Montserrat 20/24 when compiled in (else LVGL's default font).
    bool Start(lv_obj_t* parent, Fetcher fetcher = nullptr, const WeatherFonts* fonts = nullptr);
    void Stop();

    void Show();    // open now (cached data if fresh, else download)
    void Hide();
    bool IsShown() const;
    // Feed it every sentence recognised from the user's voice; opens the weather if it is a weather question.
    bool OnVoiceText(const char* text);

    // One step of the internal LVGL timer; public so tests can drive time.
    void Step(uint32_t now_ms);

    struct DebugInfo {
        bool shown, loading, has_cache;
        uint32_t hide_in_ms;
    };
    DebugInfo Debug(uint32_t now_ms) const;

private:
    struct Shared;
    static void TimerCallback(lv_timer_t* timer);
    static void ClickCallback(lv_event_t* e);
    void LaunchFetch(uint32_t now_ms);

    ChipoWeatherView view_;
    std::shared_ptr<Shared> shared_;
    lv_timer_t* timer_ = nullptr;
    bool running_ = false;

    bool have_cache_ = false;
    WeatherData cache_{};
    uint32_t cache_time_ms_ = 0;

    bool loading_ = false;
    uint32_t loading_since_ms_ = 0;
    uint32_t hide_at_ms_ = 0;
    bool auto_hide_armed_ = false;
};
