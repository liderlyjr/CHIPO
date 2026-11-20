#pragma once

#include <cstdint>
#include <functional>
#include <string>

/**
 * CHIPO PET: mood of the little computer and two voice games. Pure logic, no ESP-IDF, unit tested on a PC.
 *
 * Mood (0..100): rises when the owner pays attention (talks, touches, tickles) and slowly drops when ignored.
 * It is NOT saved across reboots (every start-up begins "fine"): honest and simple.
 */
enum class PetLevel { Happy, Fine, Lonely, Neglected };

class PetMood {
public:
    static constexpr int kStart = 70;
    static constexpr uint32_t kGraceMs = 30u * 60u * 1000u;   // no decay during the first 30 min without attention
    static constexpr uint32_t kDecayStepMs = 6u * 60u * 1000u;  // then -1 point every 6 min
    static constexpr uint32_t kPoutEveryMs = 15u * 60u * 1000u;

    void Reset(uint32_t now_ms);
    void Attention(uint32_t now_ms, int amount);  // talk +2, touch +3, tickle +6, a game round +4, asking how it is +8
    int Value(uint32_t now_ms) const;
    PetLevel Level(uint32_t now_ms) const;
    // True (at most every 15 min) when it is lonely/neglected and should show a sulk on the face.
    bool ShouldPout(uint32_t now_ms);

    static PetLevel LevelOf(int value);
    static const char* Describe(PetLevel level);  // the sentence for "¿cómo estás?"

private:
    int stored_ = kStart;
    uint32_t last_attention_ms_ = 0;
    uint32_t last_pout_ms_ = 0;
    bool pouted_ = false;
};

// ---- voice -------------------------------------------------------------------------------------------
enum class PetIntent { None, HowAreYou, RpsStart, RpsMove, GuessStart, GuessNumber, StopGame };

struct PetCommand {
    PetIntent intent = PetIntent::None;
    int value = 0;  // RpsMove: 0 rock, 1 paper, 2 scissors. GuessNumber: the number (0 = out of range / unknown)
};

PetCommand ParsePetCommand(const char* text, bool game_active);

// ---- games -------------------------------------------------------------------------------------------
struct GameReply {
    bool handled = false;
    std::string text;           // shown as a notification
    const char* emotion = nullptr;  // ChipoFace::ShowEmotionByName name, or nullptr
    const char* clip = "buddy_chime";
    bool lost = false;          // CHIPO lost the round (sore-loser tantrum)
};

class PetGames {
public:
    static constexpr uint32_t kIdleEndMs = 60u * 1000u;
    enum class Game { None, Rps, Guess };

    explicit PetGames(std::function<int(int lo, int hi)> rand);
    GameReply Handle(const PetCommand& cmd, uint32_t now_ms);
    // Ends a game nobody plays any more; returns true when it did.
    bool Tick(uint32_t now_ms);
    Game game() const { return game_; }
    int rps_user() const { return score_user_; }
    int rps_chipo() const { return score_chipo_; }
    int guess_secret() const { return secret_; }

private:
    std::function<int(int, int)> rand_;
    Game game_ = Game::None;
    uint32_t last_ms_ = 0;
    int score_user_ = 0, score_chipo_ = 0;
    int secret_ = 0, tries_ = 0;
};
