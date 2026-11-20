#include "chipo_scenes_logic.h"

#include <cmath>
#include <string>
#include <vector>

#include "chipo_drive_voice.h"  // DriveNormalizeText

namespace {
using Tokens = std::vector<std::string>;
Tokens Split(const std::string& s) {
    Tokens out;
    std::string cur;
    for (char c : s) {
        if (c == ' ') { if (!cur.empty()) out.push_back(cur), cur.clear(); }
        else cur.push_back(c);
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}
bool In(const std::string& w, std::initializer_list<const char*> list) {
    for (const char* x : list) if (w == x) return true;
    return false;
}
bool Has(const Tokens& t, std::initializer_list<const char*> list) {
    for (const auto& w : t) if (In(w, list)) return true;
    return false;
}
Tokens Core(const char* text) {
    Tokens core;
    for (const auto& w : Split(DriveNormalizeText(text)))
        if (!In(w, {"chipo", "chippo", "shipo", "chipu", "chepo", "cipo", "por", "favor", "porfa", "oye", "venga", "ya", "pues", "hey", "ey"})) core.push_back(w);
    return core;
}
float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
}  // namespace

SceneCommand ParseSceneCommand(const char* text, bool scene_active) {
    SceneCommand cmd;
    const Tokens core = Core(text);
    if (core.empty() || core.size() > 8) return cmd;
    SceneKind kind = SceneKind::None;
    if (Has(core, {"visualizador", "ecualizador", "ecualizacion"})) kind = SceneKind::Visualizer;
    else if (Has(core, {"acuario", "pecera", "peces"})) kind = SceneKind::Aquarium;
    else if (Has(core, {"lava"}) && Has(core, {"lampara", "lamparita"})) kind = SceneKind::Lava;

    const bool off_verb = Has(core, {"quita", "quitalo", "quitalas", "cierra", "cierralo", "apaga", "apagalo", "sal", "salir", "desactiva", "oculta", "vuelve", "basta"});
    const bool on_verb = Has(core, {"pon", "ponme", "muestra", "muestrame", "ensename", "activa", "abre", "enciende", "enciendeme", "quiero", "ver", "veamos"});
    if (kind != SceneKind::None) {
        if (off_verb) { cmd.matched = true; cmd.off = true; cmd.kind = kind; return cmd; }
        if (on_verb || core.size() <= 3) { cmd.matched = true; cmd.kind = kind; return cmd; }
        return cmd;  // "que es un acuario": a question, not an order
    }
    // "cierra eso", "quita eso", "vuelve": only while a screen is open (and not about the lamp or the light).
    if (scene_active && core.size() <= 3 && off_verb && !Has(core, {"luz", "luces", "lampara", "alarma", "musica", "lluvia", "fuego", "ruido", "escena"})) {
        cmd.matched = true;
        cmd.off = true;
        return cmd;
    }
    return cmd;
}

DanceIntent ParseDanceCommand(const char* text, bool dancing) {
    const Tokens core = Core(text);
    if (core.empty() || core.size() > 8) return DanceIntent::None;
    const bool dance_word = Has(core, {"baila", "bailar", "bailes", "bailemos", "bailando", "bailame", "bailas"});
    if (dance_word) {
        if (Has(core, {"no", "para", "deja", "dejes", "pares", "basta", "termina", "acaba", "parar"})) return DanceIntent::Off;
        if (Has(core, {"sabes", "puedes"}) && !Has(core, {"baila", "bailame"})) return DanceIntent::None;  // "sabes bailar?" is a question
        return DanceIntent::On;
    }
    if (dancing && core.size() <= 3 && Has(core, {"para", "basta", "quieto", "quieta", "alto", "suficiente"})) return DanceIntent::Off;
    return DanceIntent::None;
}

void BarsModel::Update(const float* in, float dt, float t_s) {
    for (int i = 0; i < kBars; ++i) {
        float target;
        if (in) target = Clamp(in[i], 0.0f, 1.0f);
        else target = 0.07f + 0.05f * std::sin(t_s * 1.6f + static_cast<float>(i) * 0.7f);
        const float k = target > bar[i] ? 1.0f - std::exp(-dt / 0.05f) : 1.0f - std::exp(-dt / 0.22f);
        bar[i] += (target - bar[i]) * k;
        if (bar[i] >= peak[i]) {
            peak[i] = bar[i];
            hold[i] = 0.55f;
        } else if (hold[i] > 0) {
            hold[i] -= dt;
        } else {
            peak[i] -= 0.5f * dt;
            if (peak[i] < bar[i]) peak[i] = bar[i];
        }
    }
}

void AquariumModel::Init(int width, int height, const SceneRand& rand) {
    w = width;
    h = height;
    static const uint32_t kColors[kFish] = {0xFF8A3D, 0xFFD23F, 0xFF5CC8, 0x4DE1FF};
    for (int i = 0; i < kFish; ++i) {
        Fish& f = fish[i];
        f.dir = rand(0, 1) ? 1 : -1;
        f.size = 30.0f + static_cast<float>(rand(0, 22));
        f.base_y = 90.0f + static_cast<float>(i) * 62.0f + static_cast<float>(rand(0, 20));
        f.y = f.base_y;
        f.x = static_cast<float>(rand(40, w - 40));
        f.vx = (22.0f + static_cast<float>(rand(0, 26))) * static_cast<float>(f.dir);
        f.phase = static_cast<float>(rand(0, 628)) / 100.0f;
        f.rgb = kColors[i];
    }
    for (Bubble& b : bubble) {
        b.x = static_cast<float>(rand(20, w - 20));
        b.y = static_cast<float>(rand(0, h));
        b.vy = 24.0f + static_cast<float>(rand(0, 30));
        b.r = 3.0f + static_cast<float>(rand(0, 5));
    }
    for (int i = 0; i < kPlants; ++i) plant_phase[i] = static_cast<float>(i) * 1.7f;
}

void AquariumModel::Update(float dt, const SceneRand& rand) {
    for (Fish& f : fish) {
        f.phase += dt * 1.3f;
        f.x += f.vx * dt;
        f.y = f.base_y + 14.0f * std::sin(f.phase);
        const float margin = f.size * 0.5f + 6.0f;
        if (f.x > static_cast<float>(w) - margin && f.vx > 0) { f.vx = -f.vx; f.dir = -1; }
        if (f.x < margin && f.vx < 0) { f.vx = -f.vx; f.dir = 1; }
    }
    for (Bubble& b : bubble) {
        b.y -= b.vy * dt;
        b.x += 6.0f * std::sin(b.y * 0.05f) * dt;
        if (b.y < -10.0f) {
            b.y = static_cast<float>(h) + 10.0f;
            b.x = static_cast<float>(rand(20, w - 20));
            b.vy = 24.0f + static_cast<float>(rand(0, 30));
            b.r = 3.0f + static_cast<float>(rand(0, 5));
        }
    }
    for (float& p : plant_phase) p += dt * 1.1f;
}

void LavaModel::Init(int width, int height, const SceneRand& rand) {
    w = width;
    h = height;
    t = 0;
    for (int i = 0; i < kBlobs; ++i) {
        LavaBlob& b = blob[i];
        b.r = 46.0f + static_cast<float>(rand(0, 40));
        b.x = static_cast<float>(rand(60, w - 60));
        b.y = static_cast<float>(rand(80, h - 80));
        b.vx = static_cast<float>(rand(-10, 10));
        b.vy = static_cast<float>(rand(-26, 26));
        if (std::fabs(b.vy) < 8.0f) b.vy = 12.0f;
        b.hue = static_cast<float>(i) / static_cast<float>(kBlobs);
    }
}

void LavaModel::Update(float dt) {
    t += dt;
    for (LavaBlob& b : blob) {
        // Warm blobs rise, cool ones sink: the vertical speed follows a slow wave, bouncing off top and bottom.
        b.x += (b.vx + 8.0f * std::sin(t * 0.7f + b.hue * 9.0f)) * dt;
        b.y += b.vy * dt;
        const float top = b.r, bottom = static_cast<float>(h) - b.r;
        if (b.y < top) { b.y = top; b.vy = std::fabs(b.vy); }
        if (b.y > bottom) { b.y = bottom; b.vy = -std::fabs(b.vy); }
        if (b.x < b.r * 0.5f) { b.x = b.r * 0.5f; b.vx = std::fabs(b.vx); }
        if (b.x > static_cast<float>(w) - b.r * 0.5f) { b.x = static_cast<float>(w) - b.r * 0.5f; b.vx = -std::fabs(b.vx); }
        b.hue += dt * 0.012f;
        if (b.hue > 1.0f) b.hue -= 1.0f;
    }
}
