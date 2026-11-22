#include "chipo_remind.h"

#include <utility>

namespace {
constexpr int kRingsPerReminder = 3;
constexpr uint32_t kRingEveryMs = 20000;
}  // namespace

ChipoRemind::ChipoRemind() = default;
ChipoRemind::~ChipoRemind() { Stop(); }

uint32_t ChipoRemind::Now() const { return hooks_.clock ? hooks_.clock() : lv_tick_get(); }

bool ChipoRemind::Start(Hooks hooks) {
    if (running_) return true;
    hooks_ = std::move(hooks);
    if (hooks_.load) store_.Load(hooks_.load());
    timer_ = lv_timer_create(TimerCallback, 1000, this);
    if (timer_ == nullptr) {
        // Without the tick nothing here would ever run: undo what Start() set up so it can be called again.
        Stop();
        hooks_ = Hooks{};
        return false;
    }
    running_ = true;
    return true;
}

void ChipoRemind::Stop() {
    if (timer_) lv_timer_delete(timer_);
    timer_ = nullptr;
    running_ = false;
    ring_queue_.clear();
    ring_left_ = 0;
}

void ChipoRemind::TimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoRemind*>(lv_timer_get_user_data(timer));
    if (self) self->Step(self->Now());
}

void ChipoRemind::Persist() {
    if (hooks_.save) hooks_.save(store_.Serialize());
}

bool ChipoRemind::Say(const std::string& text, const char* emotion) {
    if (hooks_.notify) hooks_.notify(text.c_str());
    if (emotion && hooks_.emotion) hooks_.emotion(emotion, 2500);
    return hooks_.play_clip && hooks_.play_clip("buddy_chime");
}

void ChipoRemind::Ring(uint32_t now_ms) {
    if (ring_queue_.empty()) return;
    if (hooks_.assistant_busy && hooks_.assistant_busy()) return;  // never talk over the assistant: try again next second
    if (hooks_.notify) hooks_.notify(("Recordatorio: " + RemindCapitalise(ring_queue_.front())).c_str());
    if (hooks_.emotion) hooks_.emotion("surprised", 2500);
    if (hooks_.play_clip) hooks_.play_clip("buddy_alarm");
    ++rung_;
    next_ring_ms_ = now_ms + kRingEveryMs;
    if (--ring_left_ <= 0) {
        ring_queue_.erase(ring_queue_.begin());
        ring_left_ = ring_queue_.empty() ? 0 : kRingsPerReminder;
    }
}

void ChipoRemind::Step(uint32_t now_ms) {
    if (!running_) return;
    Wall wall;
    if (hooks_.wall_time) hooks_.wall_time(&wall);
    const size_t before = store_.items().size();
    for (const Reminder& r : store_.Due(wall.valid, wall.hour, wall.minute)) {
        if (ring_queue_.size() < 8) ring_queue_.push_back(r.text);
        if (ring_left_ == 0) { ring_left_ = kRingsPerReminder; next_ring_ms_ = now_ms; }
    }
    if (store_.items().size() != before) Persist();  // a one-shot reminder was used up
    if (ring_left_ > 0 && static_cast<int32_t>(now_ms - next_ring_ms_) >= 0) Ring(now_ms);
}

bool ChipoRemind::OnVoiceText(const char* text) {
    if (!running_) return false;
    const RemindCommand cmd = ParseRemindCommand(text, ringing());
    switch (cmd.intent) {
    case RemindIntent::None: return false;
    case RemindIntent::Dismiss:
        ring_queue_.clear();
        ring_left_ = 0;
        return Say("Vale, recordatorio hecho", "happy");
    case RemindIntent::Add:
        if (!store_.Add(cmd.reminder)) return Say("Ya tengo 8 recordatorios. Borra alguno primero", "sad");
        Persist();
        return Say(RemindConfirmation(cmd.reminder), "happy");
    case RemindIntent::AddNeedsDate:
        return Say("Solo sé recordar a una hora de hoy o todos los días", "confused");
    case RemindIntent::List:
        return Say(RemindListText(store_.items()), nullptr);
    case RemindIntent::ClearAll:
        store_.Clear();
        Persist();
        return Say("Recordatorios borrados", "happy");
    case RemindIntent::ClearAt: {
        const size_t n = store_.RemoveAt(cmd.hour, cmd.minute);
        if (n) Persist();
        return Say(n ? "Recordatorio borrado" : "No tengo ningún recordatorio a esa hora", n ? "happy" : "confused");
    }
    }
    return false;
}
