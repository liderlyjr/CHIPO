#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "chipo_pet_logic.h"
#include "lvgl.h"

/**
 * CHIPO PET: the little computer's mood (rises with attention, drops when ignored) and two voice games.
 * No widgets of its own: it answers with a sound, a notification text and a face expression.
 * Call every method on the LVGL task (display lock held).
 */
class ChipoPet {
public:
    struct Hooks {
        std::function<bool(const char* clip_id)> play_clip;           // false if the clip does not exist
        std::function<void(const char* text)> notify;
        std::function<void(const char* emotion, uint32_t ms)> emotion;  // ChipoFace::ShowEmotionByName
        std::function<void(uint32_t ms)> cry;                          // very neglected: tears (ChipoFace::Cry)
        std::function<void()> sore_loser;                              // lost a game: tantrum, then tears
        std::function<int(int lo, int hi)> rand;                       // default: lv_rand
        std::function<uint32_t()> clock;                               // ms, defaults to lv_tick_get
    };

    ChipoPet();
    ~ChipoPet();
    ChipoPet(const ChipoPet&) = delete;
    ChipoPet& operator=(const ChipoPet&) = delete;

    bool Start(Hooks hooks);
    void Stop();

    // True when CHIPO answered with a sound, so the server's spoken reply must be dropped.
    bool OnVoiceText(const char* text);
    enum class Touch { Tap, Tickle };
    void OnTouch(Touch kind);  // the owner touched the face: attention

    int mood() const;
    PetLevel level() const;
    PetGames::Game game() const { return games_ ? games_->game() : PetGames::Game::None; }
    int pouts() const { return pouts_; }

    void Step(uint32_t now_ms);  // public so tests can drive time

private:
    static void TimerCallback(lv_timer_t* timer);
    uint32_t Now() const;

    Hooks hooks_;
    PetMood mood_;
    std::unique_ptr<PetGames> games_;
    lv_timer_t* timer_ = nullptr;
    bool running_ = false;
    int pouts_ = 0;
};
