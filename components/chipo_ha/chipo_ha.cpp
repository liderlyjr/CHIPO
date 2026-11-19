#include "chipo_ha.h"

#include <utility>

#include "chipo_ha_media.h"

ChipoHa::ChipoHa() = default;
ChipoHa::~ChipoHa() { Stop(); }

bool ChipoHa::Start(const HaConfig& cfg, Hooks hooks) {
    Stop();
    cfg_ = cfg;
    hooks_ = std::move(hooks);
    configured_ = cfg_.usable() && hooks_.post && hooks_.run_async;
    sensors_ = hooks_.get ? HaParseSensors(cfg_.sensors) : std::vector<HaSensor>{};
    shared_ = std::make_shared<Shared>();
    if (!configured_) return true;  // nothing to do, nothing to run
    timer_ = lv_timer_create(TimerCallback, 300, this);
    if (!timer_) {
        configured_ = false;
        return false;
    }
    return true;
}

void ChipoHa::Stop() {
    if (timer_) lv_timer_delete(timer_);
    timer_ = nullptr;
    configured_ = false;
    in_flight_ = false;
    shared_.reset();  // a worker still running keeps its own reference and writes into nothing we read
}

void ChipoHa::TimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoHa*>(lv_timer_get_user_data(timer));
    if (self) self->Step();
}

void ChipoHa::Step() {
    if (!shared_ || !in_flight_) return;
    const int st = shared_->state.load(std::memory_order_acquire);
    if (st == 0) return;
    in_flight_ = false;
    if (hooks_.notify) hooks_.notify(st == 1 ? shared_->ok_text.c_str() : shared_->fail_text.c_str());
    shared_->state.store(0, std::memory_order_release);
}

bool ChipoHa::Launch(const std::string& domain, const std::string& service, const std::string& entity,
                     const std::string& ok_text, const std::string& fail_text) {
    if (!configured_ || in_flight_) return false;
    HaRequest req;
    if (!HaBuildServiceCall(cfg_, domain, service, entity, &req)) return false;
    auto shared = shared_;
    shared->ok_text = ok_text;
    shared->fail_text = fail_text;
    shared->state.store(0, std::memory_order_release);
    in_flight_ = true;
    auto post = hooks_.post;
    const std::string token = cfg_.token;
    hooks_.run_async([shared, post, req, token]() {
        const bool ok = post(req, token);
        shared->state.store(ok ? 1 : 2, std::memory_order_release);
    });
    return true;
}

bool ChipoHa::LaunchRead(const HaSensor& sensor) {
    if (!configured_ || in_flight_ || !hooks_.get) return false;
    HaRequest req;
    if (!HaBuildStateGet(cfg_, sensor.entity_id, &req)) return false;
    auto shared = shared_;
    shared->ok_text.clear();
    shared->fail_text = "Home Assistant: no puedo leer " + sensor.alias;
    shared->state.store(0, std::memory_order_release);
    in_flight_ = true;
    auto get = hooks_.get;
    const std::string token = cfg_.token;
    const std::string alias = sensor.alias;
    hooks_.run_async([shared, get, req, token, alias]() {
        std::string body;
        HaState st;
        if (get(req, token, &body) && HaParseState(body, &st)) {
            shared->ok_text = HaSpeakState(alias, st);
            shared->state.store(1, std::memory_order_release);
        } else {
            shared->state.store(2, std::memory_order_release);
        }
    });
    return true;
}

bool ChipoHa::OnVoiceText(const char* text) {
    if (!configured_) return false;
    const HaIntent in = ParseHaIntent(text);
    bool started = false;
    switch (in.kind) {
    case HaIntentKind::None: {
        // Music through the configured media_player ("pon música", "siguiente canción").
        if (HaValidMediaPlayer(cfg_.media_player)) {
            const HaMediaCommand media = ParseHaMediaCommand(text);
            if (media.action != HaMediaAction::None) {
                if (!Launch("media_player", media.service, cfg_.media_player, media.ok_text, "Home Assistant: no responde el reproductor")) {
                    if (hooks_.notify) hooks_.notify("Home Assistant ocupado");
                    return false;
                }
                return hooks_.play_clip && hooks_.play_clip("buddy_chime");
            }
        }
        HaSensor sensor;
        if (sensors_.empty() || !ParseHaSensorQuery(text, sensors_, &sensor)) return false;
        if (!LaunchRead(sensor)) {
            if (hooks_.notify) hooks_.notify("Home Assistant ocupado");
            return false;
        }
        return hooks_.play_clip && hooks_.play_clip("buddy_chime");
    }
    case HaIntentKind::Scene:
        started = Launch("scene", "turn_on", in.entity_id, "Escena " + in.spoken, "Home Assistant: no encuentro o no responde la escena " + in.spoken);
        break;
    case HaIntentKind::LightOn:
        started = Launch("light", "turn_on", in.entity_id, "Luz " + in.spoken + " encendida", "Home Assistant: no responde la luz " + in.spoken);
        break;
    case HaIntentKind::LightOff:
        started = Launch("light", "turn_off", in.entity_id, "Luz " + in.spoken + " apagada", "Home Assistant: no responde la luz " + in.spoken);
        break;
    case HaIntentKind::AllLightsOff:
        started = Launch("light", "turn_off", "all", "Luces apagadas", "Home Assistant: no responde");
        break;
    }
    if (!started) {
        if (hooks_.notify) hooks_.notify("Home Assistant ocupado");
        return false;
    }
    return hooks_.play_clip && hooks_.play_clip("buddy_chime");
}

void ChipoHa::RunScene(const char* which) {
    if (!configured_ || !which) return;
    const std::string& entity = std::string(which) == "night" ? cfg_.scene_night : cfg_.scene_morning;
    if (entity.empty()) return;
    Launch("scene", "turn_on", entity, std::string("Escena ") + which, "Home Assistant no responde");
}
