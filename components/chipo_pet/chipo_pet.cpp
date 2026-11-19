#include "chipo_pet.h"

#include <utility>

namespace {
constexpr uint32_t kTickMs = 30000;
}

ChipoPet::ChipoPet() = default;
ChipoPet::~ChipoPet() { Stop(); }

uint32_t ChipoPet::Now() const { return hooks_.clock ? hooks_.clock() : lv_tick_get(); }

bool ChipoPet::Start(Hooks hooks) {
    if (running_) return true;
    hooks_ = std::move(hooks);
    auto rnd = hooks_.rand ? hooks_.rand : std::function<int(int, int)>([](int lo, int hi) {
        return static_cast<int>(lv_rand(static_cast<uint32_t>(lo), static_cast<uint32_t>(hi)));
    });
    games_ = std::make_unique<PetGames>(rnd);
    mood_.Reset(Now());
    timer_ = lv_timer_create(TimerCallback, kTickMs, this);
    running_ = timer_ != nullptr;
    return running_;
}

void ChipoPet::Stop() {
    if (timer_) lv_timer_delete(timer_);
    timer_ = nullptr;
    games_.reset();
    running_ = false;
}

void ChipoPet::TimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoPet*>(lv_timer_get_user_data(timer));
    if (self) self->Step(self->Now());
}

int ChipoPet::mood() const { return mood_.Value(Now()); }
PetLevel ChipoPet::level() const { return mood_.Level(Now()); }

void ChipoPet::Step(uint32_t now_ms) {
    if (!running_) return;
    if (games_->Tick(now_ms) && hooks_.notify) hooks_.notify("Se acabó el juego. ¡Otra vez cuando quieras!");
    if (mood_.ShouldPout(now_ms)) {
        ++pouts_;
        // Quiet sulk: only the face (no sound, nobody is bothered).
        const bool neglected = mood_.Level(now_ms) == PetLevel::Neglected;
        if (neglected && hooks_.cry) hooks_.cry(8000);  // tears
        else if (hooks_.emotion) hooks_.emotion(neglected ? "sleepy" : "sad", 5000);
    }
}

void ChipoPet::OnTouch(Touch kind) {
    if (!running_) return;
    mood_.Attention(Now(), kind == Touch::Tickle ? 6 : 3);
}

bool ChipoPet::OnVoiceText(const char* text) {
    if (!running_) return false;
    const uint32_t now = Now();
    const bool in_game = games_->game() != PetGames::Game::None;
    const PetCommand cmd = ParsePetCommand(text, in_game);
    mood_.Attention(now, 2);  // any sentence the owner says is attention
    if (cmd.intent == PetIntent::None) return false;

    const char* clip = "buddy_chime";
    const char* emotion = nullptr;
    std::string say;
    bool lost = false;
    if (cmd.intent == PetIntent::HowAreYou) {
        const PetLevel level = mood_.Level(now);
        say = PetMood::Describe(level);
        emotion = level == PetLevel::Happy ? "loving" : level == PetLevel::Fine ? "happy" : "sad";
        mood_.Attention(now, 8);
    } else {
        const GameReply r = games_->Handle(cmd, now);
        if (!r.handled) return false;
        say = r.text;
        emotion = r.emotion;
        clip = r.clip;
        lost = r.lost;
        mood_.Attention(now, 4);
    }
    if (hooks_.notify) hooks_.notify(say.c_str());
    if (lost && hooks_.sore_loser) hooks_.sore_loser();  // a bad loser: angry, then tears
    else if (emotion && hooks_.emotion) hooks_.emotion(emotion, 2500);
    return hooks_.play_clip && hooks_.play_clip(clip);
}
