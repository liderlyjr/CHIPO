#include "chipo_fx_logic.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr float kTwoPi = 6.2831853f;
constexpr float kGravity = 260.0f;  // tears fall
}  // namespace

FxEngine::FxEngine(Rand rand) : rand_(std::move(rand)) {}

int FxEngine::R(int lo, int hi) const {
    if (hi <= lo) return lo;
    if (rand_) return rand_(lo, hi);
    return lo + std::rand() % (hi - lo + 1);
}

int FxEngine::Spawn(FxKind kind, float x, float y, float size, uint32_t rgb) {
    for (int i = 0; i < kMax; ++i) {
        FxParticle& p = slots_[i];
        if (p.alive) continue;
        p = FxParticle{};
        p.alive = true;
        p.kind = kind;
        p.x = x;
        p.y = y;
        p.size = size;
        p.rgb = rgb;
        p.phase = static_cast<float>(R(0, 628)) / 100.0f;
        switch (kind) {
        case FxKind::Heart:
            p.vy = -(52.0f + static_cast<float>(R(0, 36)));
            p.vx = static_cast<float>(R(-8, 8));
            p.life = 1.9f + static_cast<float>(R(0, 6)) / 10.0f;
            break;
        case FxKind::Sparkle:
            p.life = 0.9f + static_cast<float>(R(0, 5)) / 10.0f;
            break;
        case FxKind::Tear:
            p.vy = 10.0f;
            p.life = 1.3f;
            break;
        case FxKind::Zzz:
            p.vx = 16.0f;
            p.vy = -22.0f;
            p.life = 2.6f;
            break;
        case FxKind::Sweat:
            p.vy = 6.0f;
            p.life = 1.5f;
            break;
        case FxKind::Star:
            p.vx = size * 4.0f;  // orbit radius (px)
            p.life = 600.0f;     // until Release()
            break;
        }
        return i;
    }
    return -1;
}

int FxEngine::SpawnStars(int count, float size, float radius) {
    int made = 0;
    for (int k = 0; k < count; ++k) {
        const int slot = Spawn(FxKind::Star, orbit_x_, orbit_y_, size, 0xFFE066);
        if (slot < 0) break;
        slots_[slot].vx = radius;
        slots_[slot].phase = kTwoPi * static_cast<float>(k) / static_cast<float>(count > 0 ? count : 1);
        slots_[slot].x = orbit_x_ + std::cos(slots_[slot].phase) * radius;
        slots_[slot].y = orbit_y_ + std::sin(slots_[slot].phase) * radius * 0.4f;
        ++made;
    }
    return made;
}

void FxEngine::Release(FxKind kind) {
    for (FxParticle& p : slots_) {
        if (p.alive && p.kind == kind && p.life - p.age > 0.35f) p.life = p.age + 0.35f;
    }
}

int FxEngine::Burst(FxKind kind, int count, float cx, float cy, float radius) {
    int made = 0;
    for (int k = 0; k < count; ++k) {
        const float a = kTwoPi * (static_cast<float>(k) + static_cast<float>(R(0, 50)) / 100.0f) /
                        static_cast<float>(count > 0 ? count : 1);
        const float r = radius * (0.45f + static_cast<float>(R(0, 55)) / 100.0f);
        const float x = cx + std::cos(a) * r;
        const float y = cy + std::sin(a) * r * 0.8f;
        const float size = kind == FxKind::Heart ? 16.0f + static_cast<float>(R(0, 10))
                                                 : 12.0f + static_cast<float>(R(0, 12));
        const uint32_t rgb = kind == FxKind::Heart ? (R(0, 1) ? 0xFF4F9A : 0xFF8FC4)
                                                   : (R(0, 1) ? 0xFFF0A0 : 0xBFF2FF);
        const int slot = Spawn(kind, x, y, size, rgb);
        if (slot < 0) break;
        if (kind == FxKind::Sparkle) {  // fly out from the centre, slowing down
            slots_[slot].vx = std::cos(a) * 46.0f;
            slots_[slot].vy = std::sin(a) * 40.0f;
        }
        ++made;
    }
    return made;
}

void FxEngine::Step(float dt) {
    for (FxParticle& p : slots_) {
        if (!p.alive) continue;
        p.age += dt;
        if (p.age >= p.life) {
            p.alive = false;
            continue;
        }
        switch (p.kind) {
        case FxKind::Heart:
            p.x += (p.vx + 10.0f * std::sin(p.phase + p.age * 3.2f)) * dt;
            p.y += p.vy * dt;
            break;
        case FxKind::Sparkle: {
            const float drag = std::exp(-2.2f * dt);
            p.vx *= drag;
            p.vy *= drag;
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            break;
        }
        case FxKind::Tear:
            p.vy += kGravity * dt;
            p.y += p.vy * dt;
            break;
        case FxKind::Zzz:
            p.x += (p.vx + 6.0f * std::sin(p.phase + p.age * 2.0f)) * dt;
            p.y += p.vy * dt;
            break;
        case FxKind::Sweat:
            p.vy += 120.0f * dt;  // slides slowly down the temple, then speeds up
            p.y += p.vy * dt;
            break;
        case FxKind::Star: {
            const float a = p.phase + p.age * 4.4f;
            p.x = orbit_x_ + std::cos(a) * p.vx;
            p.y = orbit_y_ + std::sin(a) * p.vx * 0.4f;
            break;
        }
        }
    }
}

void FxEngine::Clear() {
    for (FxParticle& p : slots_) p.alive = false;
}

int FxEngine::alive() const {
    int n = 0;
    for (const FxParticle& p : slots_) n += p.alive ? 1 : 0;
    return n;
}

int FxEngine::alive(FxKind kind) const {
    int n = 0;
    for (const FxParticle& p : slots_) n += (p.alive && p.kind == kind) ? 1 : 0;
    return n;
}

float FxEngine::Alpha(const FxParticle& p) {
    if (!p.alive || p.life <= 0) return 0;
    if (p.kind == FxKind::Star) {  // fixed fade times: the life is "forever" until released
        const float a = std::fmin(std::fmin(1.0f, p.age / 0.25f), (p.life - p.age) / 0.35f);
        return a < 0 ? 0 : a;
    }
    const float t = p.age / p.life;
    const float in = std::fmin(1.0f, t / 0.12f);
    const float out = std::fmin(1.0f, (1.0f - t) / 0.35f);
    const float a = std::fmin(in, out);
    return a < 0 ? 0 : a;
}

float FxEngine::Scale(const FxParticle& p) {
    const float t = p.life > 0 ? p.age / p.life : 1.0f;
    switch (p.kind) {
    case FxKind::Sparkle: return 0.35f + 0.85f * std::fabs(std::sin(kTwoPi * 0.5f * (t * 2.0f) + p.phase * 0.2f));
    case FxKind::Zzz: return 0.6f + 0.6f * t;
    case FxKind::Heart: return 0.7f + 0.4f * std::fmin(1.0f, t * 4.0f);
    case FxKind::Tear: return 1.0f;
    case FxKind::Sweat: return 0.8f + 0.2f * std::fmin(1.0f, t * 3.0f);
    case FxKind::Star: return 0.85f + 0.2f * std::sin(p.age * 9.0f + p.phase);
    }
    return 1.0f;
}

// ---- costumes ----------------------------------------------------------------------------------------
Costume CostumeForDate(int month, int day) {
    if (month == 12) return Costume::SantaHat;
    if (month == 1 && day >= 1 && day <= 6) return Costume::SantaHat;
    return Costume::None;
}

const char* CostumeName(Costume c) {
    switch (c) {
    case Costume::SantaHat: return "el gorro de Papa Noel";
    case Costume::Sunglasses: return "las gafas de sol";
    case Costume::Mustache: return "el bigote";
    case Costume::None: return "el disfraz";
    }
    return "";
}

namespace {
std::string Normalize(const char* text) {
    std::string out;
    if (!text) return out;
    const unsigned char* s = reinterpret_cast<const unsigned char*>(text);
    for (; *s; ++s) {
        unsigned char c = *s;
        if (c == 0xC3 && s[1]) {  // UTF-8 Latin-1 letters
            ++s;
            switch (*s) {
            case 0xA1: case 0x81: c = 'a'; break;
            case 0xA9: case 0x89: c = 'e'; break;
            case 0xAD: case 0x8D: c = 'i'; break;
            case 0xB3: case 0x93: c = 'o'; break;
            case 0xBA: case 0x9A: case 0xBC: case 0x9C: c = 'u'; break;
            case 0xB1: case 0x91: c = 'n'; break;
            default: c = ' '; break;
            }
        } else if (c >= 'A' && c <= 'Z') {
            c = static_cast<unsigned char>(c - 'A' + 'a');
        } else if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) {
            c = ' ';
        }
        out.push_back(static_cast<char>(c));
    }
    return out;
}

std::vector<std::string> Words(const std::string& s) {
    std::vector<std::string> w;
    std::string cur;
    for (char c : s) {
        if (c == ' ') {
            if (!cur.empty()) w.push_back(cur), cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) w.push_back(cur);
    return w;
}

bool Has(const std::vector<std::string>& w, const char* a) {
    for (const auto& x : w) if (x == a) return true;
    return false;
}
}  // namespace

CostumeCommand ParseCostumeCommand(const char* text) {
    CostumeCommand cmd;
    const auto w = Words(Normalize(text));
    if (w.size() < 2 || w.size() > 8) return cmd;
    const bool put = Has(w, "ponte") || Has(w, "pontelo") || Has(w, "disfrazate") || Has(w, "colocate");
    const bool take = Has(w, "quitate") || Has(w, "sacate") || Has(w, "quita");
    if (!put && !take) return cmd;
    Costume which = Costume::None;
    if (Has(w, "gorro") || Has(w, "gorrito") || Has(w, "noel") || Has(w, "navidad") || Has(w, "navideno")) which = Costume::SantaHat;
    else if (Has(w, "gafas") || Has(w, "lentes")) which = Costume::Sunglasses;
    else if (Has(w, "bigote") || Has(w, "mostacho")) which = Costume::Mustache;
    if (take) {
        // "quitate el disfraz / el gorro / las gafas / el bigote": all of them come off
        if (which == Costume::None && !Has(w, "disfraz") && !Has(w, "todo")) return cmd;
        cmd.matched = true;
        cmd.costume = Costume::None;
        return cmd;
    }
    if (which == Costume::None) return cmd;
    cmd.matched = true;
    cmd.costume = which;
    return cmd;
}

bool ChipoIsThanks(const char* text) {
    const auto w = Words(Normalize(text));
    if (w.empty() || w.size() > 8) return false;
    if (Has(w, "gracias") || Has(w, "agradecido") || Has(w, "agradecida")) return true;
    for (size_t i = 0; i + 1 < w.size(); ++i) {
        if (w[i] == "te" && (w[i + 1] == "quiero" || w[i + 1] == "adoro" || w[i + 1] == "amo")) return true;
        if (w[i] == "eres" && (w[i + 1] == "un" || w[i + 1] == "genial" || w[i + 1] == "el")) {
            if (w[i + 1] == "genial") return true;
            if (i + 2 < w.size() && (w[i + 2] == "amor" || w[i + 2] == "encanto" || w[i + 2] == "mejor")) return true;
        }
    }
    return false;
}
