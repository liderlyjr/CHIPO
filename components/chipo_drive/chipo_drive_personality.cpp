#include "chipo_drive_personality.h"

#include <algorithm>
#include <cstring>

namespace {
constexpr DrivePhrase kPhrases[] = {
    {"drive_accel_1", "¡Ueeee, cómo tira esto!", DriveEventKind::Accelerate},
    {"drive_accel_2", "¡Qué arrancada, jefe!", DriveEventKind::Accelerate},
    {"drive_accel_3", "Uy, nos vamos… pero con cabeza.", DriveEventKind::Accelerate},
    {"drive_brake_1", "¡Uy, frenazo!", DriveEventKind::Brake},
    {"drive_brake_2", "Creo que necesito cinturón de seguridad.", DriveEventKind::Brake},
    {"drive_brake_3", "¡Con calma, jefe!", DriveEventKind::Brake},
    {"drive_curve_1", "¡Esa curva ha estado buena!", DriveEventKind::Curve},
    {"drive_curve_2", "¡Wiii, qué curva!", DriveEventKind::Curve},
    {"drive_curve_3", "Eso sí que es tomar la curva.", DriveEventKind::Curve},
    {"drive_bump_1", "¡Uy, ese bache no me lo esperaba!", DriveEventKind::Bump},
    {"drive_bump_2", "¡Ay, mis circuitos!", DriveEventKind::Bump},
    {"drive_bump_3", "¡Menudo bache!", DriveEventKind::Bump},
    {"drive_abrupt_1", "¡Madre mía, eso ha sido movidito!", DriveEventKind::Abrupt},
    {"drive_abrupt_2", "¡Jefe, que me mareo!", DriveEventKind::Abrupt},
    {"drive_calm_1", "Jefe, vamos finos.", DriveEventKind::Calm},
    {"drive_calm_2", "Qué bien vamos hoy.", DriveEventKind::Calm},
    {"drive_calm_3", "Aquí el copiloto: todo en orden.", DriveEventKind::Calm},
};
constexpr int kPhraseCount = sizeof(kPhrases) / sizeof(kPhrases[0]);

struct FixedLine { const char* id; const char* text; };
constexpr FixedLine kFixed[] = {
    {kDriveClipStart, "Brum, brum… ¡vámonos!"},
    {kDriveClipEnd, "Misión cumplida, jefe. Vuelvo al escritorio."},
    {kDriveClipSoft, "Sensibilidad suave."},
    {kDriveClipNormal, "Sensibilidad normal."},
    {kDriveClipIntense, "Sensibilidad intensa."},
};

constexpr uint32_t kFirstCommentDelayMs = 20000;  // quiet at the start of every drive
constexpr uint32_t kCapWindowMs = 300000;         // at most 4 comments per 5 minutes
constexpr uint32_t kCalmMinMs = 90000;
constexpr uint32_t kCalmJitterMs = 90000;

float BaseChance(DriveEventKind k) {
    switch (k) {
        case DriveEventKind::Accelerate: return 0.30f;
        case DriveEventKind::Brake: return 0.35f;
        case DriveEventKind::Curve: return 0.30f;
        case DriveEventKind::Bump: return 0.45f;
        case DriveEventKind::Abrupt: return 0.65f;
        default: return 1.0f;
    }
}
}  // namespace

const DrivePhrase* DrivePhrases(int* count) {
    if (count) *count = kPhraseCount;
    return kPhrases;
}

const char* DriveTextForClip(const char* id) {
    if (id == nullptr) return nullptr;
    for (const auto& p : kPhrases) {
        if (std::strcmp(p.id, id) == 0) return p.text;
    }
    for (const auto& f : kFixed) {
        if (std::strcmp(f.id, id) == 0) return f.text;
    }
    return nullptr;
}

uint32_t DrivePersonality::Rand() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
}
float DrivePersonality::Rand01() { return static_cast<float>(Rand() & 0xFFFFFF) / 16777216.0f; }

void DrivePersonality::Begin(uint32_t now_ms) {
    begun_ = true;
    begin_ms_ = now_ms;
    spoke_ever_ = false;
    last_speech_ms_ = now_ms;
    for (auto& t : recent_ms_) t = 0;
    next_calm_ms_ = now_ms + kFirstCommentDelayMs + kCalmMinMs + Rand() % kCalmJitterMs;
}

bool DrivePersonality::MaySpeak(uint32_t now_ms, bool can_speak) const {
    if (!can_speak || !begun_) return false;
    if (now_ms - begin_ms_ < kFirstCommentDelayMs) return false;
    if (spoke_ever_ && now_ms - last_speech_ms_ < DriveSpeechGapMs(sens_)) return false;
    int in_window = 0;
    for (uint32_t t : recent_ms_) {
        if (t != 0 && now_ms - t < kCapWindowMs) ++in_window;
    }
    return in_window < 4;
}

void DrivePersonality::Spoke(uint32_t now_ms) {
    spoke_ever_ = true;
    last_speech_ms_ = now_ms;
    recent_ms_[recent_head_] = now_ms ? now_ms : 1;
    recent_head_ = (recent_head_ + 1) % 4;
}

const DrivePhrase* DrivePersonality::Pick(DriveEventKind kind) {
    const int k = static_cast<int>(kind);
    const DrivePhrase* options[8];
    int n = 0;
    for (const auto& p : kPhrases) {
        if (p.kind != kind) continue;
        if (p.id == nullptr) continue;
        const bool recent = last_[k][0] == &p || last_[k][1] == &p;
        if (!recent && n < 8) options[n++] = &p;
    }
    if (n == 0) {  // everything was used recently: allow all but the very last one
        for (const auto& p : kPhrases) {
            if (p.kind == kind && last_[k][0] != &p && n < 8) options[n++] = &p;
        }
    }
    if (n == 0) return nullptr;
    const DrivePhrase* chosen = options[Rand() % static_cast<uint32_t>(n)];
    last_[k][1] = last_[k][0];
    last_[k][0] = chosen;
    return chosen;
}

DriveReaction DrivePersonality::OnEvent(DriveEventKind kind, float intensity, uint32_t now_ms, bool can_speak) {
    DriveReaction r;
    switch (kind) {
        case DriveEventKind::Accelerate: r.look = DriveLook::Excited; break;
        case DriveEventKind::Brake: r.look = DriveLook::Surprised; break;
        case DriveEventKind::Curve: r.look = DriveLook::None; break;  // the lean is the reaction
        case DriveEventKind::Bump: r.look = DriveLook::Surprised; break;
        case DriveEventKind::Abrupt: r.look = intensity > 0.75f ? DriveLook::Dizzy : DriveLook::Surprised; break;
        default: break;
    }
    if (!MaySpeak(now_ms, can_speak)) return r;
    const float chance = std::min(0.9f, BaseChance(kind) * DriveSpeechScale(sens_) * (0.6f + 0.4f * intensity));
    if (Rand01() >= chance) return r;
    r.say = Pick(kind);
    if (r.say) Spoke(now_ms);
    return r;
}

DriveReaction DrivePersonality::Tick(uint32_t now_ms, bool calm, bool can_speak) {
    DriveReaction r;
    if (!begun_ || now_ms < next_calm_ms_) return r;
    if (!calm || !MaySpeak(now_ms, can_speak)) {
        next_calm_ms_ = now_ms + 15000;  // try again soon, it was not a good moment
        return r;
    }
    r.say = Pick(DriveEventKind::Calm);
    if (r.say) Spoke(now_ms);
    next_calm_ms_ = now_ms + kCalmMinMs + Rand() % kCalmJitterMs;
    return r;
}
