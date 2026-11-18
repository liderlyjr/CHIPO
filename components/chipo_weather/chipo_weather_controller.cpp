#include "chipo_weather_controller.h"

#include <atomic>

#include "chipo_weather_config.h"

#ifdef ESP_PLATFORM
#include "chipo_weather_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const char* TAG = "chipo_weather";
#endif

namespace {
constexpr uint32_t kTimerPeriodMs = 200;
constexpr uint32_t kErrorShowMs = 5000;
}  // namespace

// State shared with the worker task. Held by shared_ptr so a late worker never touches freed memory.
struct ChipoWeather::Shared {
    Fetcher fetcher;
    std::atomic<int> state{0};  // 0 idle, 1 running, 2 finished ok, 3 finished with error
    WeatherData result;         // written by the worker before state becomes 2, read after
};

ChipoWeather::ChipoWeather() = default;
ChipoWeather::~ChipoWeather() { Stop(); }

static WeatherFonts DefaultFonts() {
#if LV_FONT_MONTSERRAT_20
    const lv_font_t* small = &lv_font_montserrat_20;
#else
    const lv_font_t* small = LV_FONT_DEFAULT;
#endif
#if LV_FONT_MONTSERRAT_24
    const lv_font_t* medium = &lv_font_montserrat_24;
#else
    const lv_font_t* medium = LV_FONT_DEFAULT;
#endif
    return {small, medium};
}

bool ChipoWeather::Start(lv_obj_t* parent, Fetcher fetcher, const WeatherFonts* fonts) {
    if (running_) return true;
    if (!parent) return false;
    if (!fetcher) {
#ifdef ESP_PLATFORM
        fetcher = [](WeatherData* out) { return ChipoWeatherFetch(out); };
#else
        return false;
#endif
    }
    if (!view_.Create(parent, fonts ? *fonts : DefaultFonts())) return false;
    shared_ = std::make_shared<Shared>();
    shared_->fetcher = std::move(fetcher);
    timer_ = lv_timer_create(TimerCallback, kTimerPeriodMs, this);
    // Indev-level click: fires for every tap wherever it lands, without taking over any object.
    for (lv_indev_t* in = lv_indev_get_next(nullptr); in; in = lv_indev_get_next(in)) {
        if (lv_indev_get_type(in) == LV_INDEV_TYPE_POINTER) {
            lv_indev_add_event_cb(in, ClickCallback, LV_EVENT_CLICKED, this);
        }
    }
    running_ = true;
    return true;
}

void ChipoWeather::Stop() {
    if (!running_) return;
    for (lv_indev_t* in = lv_indev_get_next(nullptr); in; in = lv_indev_get_next(in)) {
        lv_indev_remove_event_cb_with_user_data(in, ClickCallback, this);
    }
    if (timer_) lv_timer_delete(timer_);
    timer_ = nullptr;
    view_.Destroy();
    shared_.reset();
    running_ = false;
}

void ChipoWeather::TimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoWeather*>(lv_timer_get_user_data(timer));
    self->Step(lv_tick_get());
}

void ChipoWeather::ClickCallback(lv_event_t* e) {
    auto* self = static_cast<ChipoWeather*>(lv_event_get_user_data(e));
    if (!self->running_) return;
    if (self->view_.IsVisible()) {
        self->Hide();
        return;
    }
    if (self->tap_blocker_) {
        lv_point_t pt{0, 0};
        lv_indev_t* in = lv_event_get_indev(e);
        if (in) lv_indev_get_point(in, &pt);
        if (self->tap_blocker_(pt.y)) return;
    }
    self->Show();
}

bool ChipoWeather::IsShown() const { return running_ && view_.IsVisible(); }

bool ChipoWeather::OnVoiceText(const char* text) {
    if (!running_ || !WeatherIntent(text)) return false;
    Show();
    return true;
}

void ChipoWeather::Hide() {
    if (!running_) return;
    view_.Hide();
    auto_hide_armed_ = false;
}

void ChipoWeather::Show() {
    if (!running_) return;
    const uint32_t now = lv_tick_get();
    const bool fresh = have_cache_ && (now - cache_time_ms_) < CHIPO_WEATHER_CACHE_MS;
    if (have_cache_) view_.Render(cache_);
    else view_.RenderStatus("CHIPO", "Buscando el clima...");
    view_.Show();
    if (fresh) {
        hide_at_ms_ = now + CHIPO_WEATHER_SHOW_MS;
        auto_hide_armed_ = true;
        return;
    }
    // No data yet (or stale): keep it on screen until the download ends, then arm the timer.
    auto_hide_armed_ = false;
    LaunchFetch(now);
}

void ChipoWeather::LaunchFetch(uint32_t now_ms) {
    if (!shared_) return;
    int expected = 0;
    if (!shared_->state.compare_exchange_strong(expected, 1)) {
        // A download is already running (or finished and not collected yet): just keep waiting for it.
        loading_ = true;
        return;
    }
    loading_ = true;
    loading_since_ms_ = now_ms;
#ifdef ESP_PLATFORM
    auto* handle = new std::shared_ptr<Shared>(shared_);
    const BaseType_t ok = xTaskCreate(
        [](void* arg) {
            auto* sp = static_cast<std::shared_ptr<Shared>*>(arg);
            std::shared_ptr<Shared> s = *sp;
            delete sp;
            WeatherData data;
            const bool good = s->fetcher && s->fetcher(&data);
            if (good) s->result = data;
            s->state.store(good ? 2 : 3, std::memory_order_release);
            vTaskDelete(nullptr);
        },
        "chipo_weather", 12 * 1024, handle, 2, nullptr);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "could not start the weather task");
        delete handle;
        shared_->state.store(3, std::memory_order_release);
    }
#else
    WeatherData data;
    const bool good = shared_->fetcher && shared_->fetcher(&data);
    if (good) shared_->result = data;
    shared_->state.store(good ? 2 : 3, std::memory_order_release);
#endif
}

void ChipoWeather::Step(uint32_t now_ms) {
    if (!running_ || !shared_) return;

    const int st = shared_->state.load(std::memory_order_acquire);
    if (st == 2 || st == 3) {
        loading_ = false;
        if (st == 2) {
            cache_ = shared_->result;
            cache_time_ms_ = now_ms;
            have_cache_ = true;
        }
        shared_->state.store(0, std::memory_order_release);
        if (view_.IsVisible()) {
            if (st == 2) {
                view_.Render(cache_);
                hide_at_ms_ = now_ms + CHIPO_WEATHER_SHOW_MS;
            } else if (!have_cache_) {
                view_.RenderStatus("Sin conexion", "No pude obtener el clima");
                hide_at_ms_ = now_ms + kErrorShowMs;
            } else {
                hide_at_ms_ = now_ms + CHIPO_WEATHER_SHOW_MS;  // refresh failed: keep the old data on screen
            }
            auto_hide_armed_ = true;
        }
    } else if (loading_ && now_ms - loading_since_ms_ > CHIPO_WEATHER_FETCH_TIMEOUT_MS + 4000) {
        // The worker is stuck (its own timeout should have fired first). Do not leave a spinner forever.
        loading_ = false;
        if (view_.IsVisible() && !have_cache_) {
            view_.RenderStatus("Sin conexion", "No pude obtener el clima");
            hide_at_ms_ = now_ms + kErrorShowMs;
            auto_hide_armed_ = true;
        }
    }

    if (view_.IsVisible() && auto_hide_armed_ && static_cast<int32_t>(now_ms - hide_at_ms_) >= 0) {
        Hide();
    }
}

ChipoWeather::DebugInfo ChipoWeather::Debug(uint32_t now_ms) const {
    DebugInfo d{};
    d.shown = IsShown();
    d.loading = loading_;
    d.has_cache = have_cache_;
    d.hide_in_ms = (auto_hide_armed_ && static_cast<int32_t>(hide_at_ms_ - now_ms) > 0) ? hide_at_ms_ - now_ms : 0;
    return d;
}
