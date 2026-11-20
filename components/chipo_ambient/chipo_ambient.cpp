#include "chipo_ambient.h"

#include <cstdio>
#include <utility>

namespace {
constexpr uint32_t kTickMs = 50;
constexpr uint32_t kFirstDelayMs = 1200;  // after the confirmation chime
constexpr uint32_t kSecondDelayMs = 750;  // second piece early: a ~0.2 s cushion in the playback queue
}  // namespace

ChipoAmbient::ChipoAmbient() = default;
ChipoAmbient::~ChipoAmbient() { Stop(); }

uint32_t ChipoAmbient::Now() const { return hooks_.clock ? hooks_.clock() : lv_tick_get(); }

bool ChipoAmbient::Start(Hooks hooks) {
    if (running_) return true;
    hooks_ = std::move(hooks);
    timer_ = lv_timer_create(TimerCallback, kTickMs, this);
    running_ = timer_ != nullptr;
    return running_;
}

void ChipoAmbient::Stop() {
    if (timer_) lv_timer_delete(timer_);
    timer_ = nullptr;
    running_ = false;
    playing_ = false;
}

void ChipoAmbient::TimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoAmbient*>(lv_timer_get_user_data(timer));
    if (self) self->Step(self->Now());
}

void ChipoAmbient::StopPlaying(const char* text) {
    playing_ = false;
    paused_ = false;
    if (text && hooks_.notify) hooks_.notify(text);
}

void ChipoAmbient::Step(uint32_t now_ms) {
    if (!running_ || !playing_) return;
    if (now_ms - started_ms_ >= kMaxPlayMs) {
        StopPlaying("Sonido ambiente apagado (30 min)");
        return;
    }
    if (hooks_.assistant_busy && hooks_.assistant_busy()) {
        paused_ = true;  // the assistant clears the audio queue by itself; nothing more is queued meanwhile
        return;
    }
    if (paused_) {
        paused_ = false;
        next_push_ms_ = now_ms + 600;
        return;
    }
    if (static_cast<int32_t>(now_ms - next_push_ms_) < 0) return;
    char id[40];
    std::snprintf(id, sizeof(id), "ambient_%s_%d", AmbientName(sound_), piece_);
    if (!hooks_.play_clip || !hooks_.play_clip(id)) {
        StopPlaying("No encuentro el sonido ambiente");
        return;
    }
    ++pieces_sent_;
    piece_ = (piece_ + 1) % kPieces;
    // Absolute schedule (no drift from timer jitter); the second piece comes early to build the cushion.
    const uint32_t step = pieces_sent_ == 1 ? kSecondDelayMs : kPieceMs;
    next_push_ms_ += step;
    if (static_cast<int32_t>(now_ms - next_push_ms_) > 2000) next_push_ms_ = now_ms + 100;  // we were away for long: resync
}

bool ChipoAmbient::OnVoiceText(const char* text) {
    if (!running_) return false;
    const AmbientCommand cmd = ParseAmbientCommand(text, playing_);
    const uint32_t now = Now();
    if (cmd.intent == AmbientIntent::Stop) {
        StopPlaying(nullptr);
        if (hooks_.notify) hooks_.notify("Sonido ambiente apagado");
        return hooks_.play_clip && hooks_.play_clip("buddy_chime");
    }
    if (cmd.intent != AmbientIntent::Start) return false;
    char id[40];
    std::snprintf(id, sizeof(id), "ambient_%s_0", AmbientName(cmd.sound));
    if (!hooks_.clip_exists || !hooks_.clip_exists(id)) {
        if (hooks_.notify) hooks_.notify("No tengo ese sonido en el firmware");
        return false;  // the assistant may answer
    }
    sound_ = cmd.sound;
    piece_ = 0;
    pieces_sent_ = 0;
    playing_ = true;
    paused_ = false;
    started_ms_ = now;
    next_push_ms_ = now + kFirstDelayMs;
    if (hooks_.notify) hooks_.notify(AmbientLabel(sound_));
    if (hooks_.emotion) hooks_.emotion("relaxed", 2500);
    return hooks_.play_clip && hooks_.play_clip("buddy_chime");
}
