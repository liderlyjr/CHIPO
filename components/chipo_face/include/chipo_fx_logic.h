#pragma once

#include <cstdint>
#include <functional>

/**
 * CHIPO FX: little floating decorations over the face (hearts, sparkles, tears, "Zzz").
 * Pure logic (no LVGL, no ESP-IDF): positions are screen pixels, unit tested on a PC.
 */
enum class FxKind : uint8_t { Heart, Sparkle, Tear, Zzz, Sweat, Star };

struct FxParticle {
    bool alive = false;
    FxKind kind = FxKind::Heart;
    float x = 0, y = 0;    // centre, px
    float vx = 0, vy = 0;  // px/s
    float size = 16;       // px
    float age = 0, life = 1;  // s
    float phase = 0;       // sway / twinkle offset
    uint32_t rgb = 0xFFFFFF;
};

class FxEngine {
public:
    static constexpr int kMax = 10;
    using Rand = std::function<int(int lo, int hi)>;  // inclusive

    explicit FxEngine(Rand rand = nullptr);

    // Returns the slot used, or -1 when all are busy (the oldest is NOT stolen: effects just skip).
    int Spawn(FxKind kind, float x, float y, float size, uint32_t rgb);
    // Several at once around a centre (hearts rise, sparkles fly out). Returns how many were created.
    int Burst(FxKind kind, int count, float cx, float cy, float radius);
    void Step(float dt);
    void Clear();

    // Dizzy stars orbit one centre (an ellipse above the eyes): `count` evenly spread, kept until Release().
    int SpawnStars(int count, float size, float radius);
    void SetOrbit(float cx, float cy) { orbit_x_ = cx; orbit_y_ = cy; }
    // Lets every particle of that kind fade out in a moment (instead of vanishing).
    void Release(FxKind kind);

    int alive() const;
    int alive(FxKind kind) const;
    const FxParticle& at(int i) const { return slots_[i]; }

    // 0..1: fades in at the start and out at the end.
    static float Alpha(const FxParticle& p);
    // 0..1.2 size factor: sparkles twinkle, zzz grows, tears are constant.
    static float Scale(const FxParticle& p);

private:
    int R(int lo, int hi) const;
    Rand rand_;
    float orbit_x_ = 0, orbit_y_ = 0;
    FxParticle slots_[kMax];
};

// ---- costumes ----------------------------------------------------------------------------------------
enum class Costume : uint8_t { None, SantaHat, Sunglasses, Mustache };

// The costume that belongs to a calendar day (month 1..12, day 1..31): Santa hat from 1 Dec to 6 Jan.
Costume CostumeForDate(int month, int day);
// Voice: "ponte el gorro de Papá Noel", "ponte las gafas de sol", "ponte bigote", "quítate el gorro/las gafas/el bigote",
// "quítate el disfraz". `none_on_off` is set when the command takes the costume off.
struct CostumeCommand {
    bool matched = false;
    Costume costume = Costume::None;  // the one to put on; None with matched = take off
};
CostumeCommand ParseCostumeCommand(const char* text);
const char* CostumeName(Costume c);  // Spanish, for notifications

// "gracias", "muchas gracias", "te quiero", "eres un amor"...: the owner is being nice (hearts).
bool ChipoIsThanks(const char* text);
