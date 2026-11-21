#pragma once

#include <cstdint>
#include <functional>

/**
 * CHIPO SCENES logic: voice commands and the little simulations behind the screens (bars, fish, lava blobs).
 * Pure code, no LVGL: unit tested on a PC. Everything is given the screen size explicitly (480 x 480 on the device).
 */
enum class SceneKind : uint8_t { None, Visualizer, Aquarium, Lava };

struct SceneCommand {
    bool matched = false;
    bool off = false;                 // close the screen
    SceneKind kind = SceneKind::None;  // which one to open (None with off = close whatever is open)
};
// "pon el visualizador", "muéstrame el acuario", "enciende la lámpara de lava", "quita el acuario", "cierra eso" (only when active).
SceneCommand ParseSceneCommand(const char* text, bool scene_active);

enum class DanceIntent : uint8_t { None, On, Off };
// "baila", "ponte a bailar", "bailemos"; "para de bailar", "no bailes", "quieto" (only while dancing).
DanceIntent ParseDanceCommand(const char* text, bool dancing);

using SceneRand = std::function<int(int lo, int hi)>;  // inclusive

// ---- equaliser bars: rise fast, fall slowly, peak caps hold then drop -------------------------------------------
struct BarsModel {
    static constexpr int kBars = 8;
    float bar[kBars] = {};
    float peak[kBars] = {};
    float hold[kBars] = {};
    // `in` = 8 values 0..1, or nullptr for silence (a faint idle wave so the screen does not look dead).
    void Update(const float* in, float dt, float t_s);
};

// ---- aquarium ---------------------------------------------------------------------------------------------------
struct Fish {
    float x = 0, y = 0, base_y = 0, vx = 0, size = 30, phase = 0;
    uint32_t rgb = 0xFF8A3D;
    int dir = 1;  // +1 swims right
};
struct Bubble {
    float x = 0, y = 0, vy = 0, r = 4;
};
struct AquariumModel {
    static constexpr int kFish = 4;
    static constexpr int kBubbles = 6;
    static constexpr int kPlants = 3;
    Fish fish[kFish];
    Bubble bubble[kBubbles];
    float plant_phase[kPlants] = {};
    int w = 368, h = 448;
    void Init(int width, int height, const SceneRand& rand);
    void Update(float dt, const SceneRand& rand);
};

// ---- lava lamp --------------------------------------------------------------------------------------------------
struct LavaBlob {
    float x = 0, y = 0, vx = 0, vy = 0, r = 50, hue = 0;
};
struct LavaModel {
    static constexpr int kBlobs = 6;
    LavaBlob blob[kBlobs];
    int w = 368, h = 448;
    float t = 0;
    void Init(int width, int height, const SceneRand& rand);
    void Update(float dt);
};
