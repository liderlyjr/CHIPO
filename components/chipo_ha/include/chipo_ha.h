#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include "chipo_ha_logic.h"
#include "lvgl.h"

/**
 * Home Assistant by voice, through the documented REST API, only when the device has an address
 * and token stored (docs/CHIPO_HOME_ASSISTANT.md). Without them it does nothing and the
 * assistant answers as usual.
 *
 * The network call runs on a worker task, never on the LVGL task; the result comes back through
 * a flag that an LVGL timer reads. All other methods run on the LVGL task with the display lock held.
 */
class ChipoHa {
public:
    struct Hooks {
        std::function<bool(const char* clip_id)> play_clip;
        std::function<void(const char* text)> notify;
        // Blocking HTTP POST with the bearer token; true on a 2xx answer. Default: esp_http_client.
        std::function<bool(const HaRequest&, const std::string& token)> post;
        // Runs `job` off the LVGL task. Default: a FreeRTOS task (host tests: inline).
        std::function<void(std::function<void()> job)> run_async;
    };

    ChipoHa();
    ~ChipoHa();
    ChipoHa(const ChipoHa&) = delete;
    ChipoHa& operator=(const ChipoHa&) = delete;

    bool Start(const HaConfig& cfg, Hooks hooks);  // cfg may be empty: then everything is off
    void Stop();

    bool configured() const { return configured_; }

    // True when CHIPO handled the sentence and answered with a sound (the server's reply is dropped).
    bool OnVoiceText(const char* text);
    // "night" / "morning": runs the configured scene, if any.
    void RunScene(const char* which);

    void Step();  // public for tests

private:
    struct Shared {
        std::atomic<int> state{0};  // 0 idle/pending, 1 ok, 2 failed
        std::string ok_text, fail_text;
    };
    static void TimerCallback(lv_timer_t* timer);
    bool Launch(const std::string& domain, const std::string& service, const std::string& entity,
                const std::string& ok_text, const std::string& fail_text);

    HaConfig cfg_;
    Hooks hooks_;
    lv_timer_t* timer_ = nullptr;
    bool configured_ = false;
    bool in_flight_ = false;
    std::shared_ptr<Shared> shared_;
};
