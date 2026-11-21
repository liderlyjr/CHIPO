#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

/**
 * Pure logic of CHIPO TOOLS (no ESP-IDF, no LVGL): unit tested on a PC.
 */

// ---- Shake -------------------------------------------------------------------------------------------------------
// Counts strong jolts of the accelerometer magnitude (|a| far from 1 g). Orientation independent.
class ShakeDetector {
public:
    // Feed every accelerometer sample (g). Returns true once per recognised shake.
    bool Feed(float ax, float ay, float az, uint32_t now_ms);
    void Reset() { *this = ShakeDetector(); }

    static constexpr float kJoltG = 0.55f;          // |  |a| - 1 | above this is a jolt
    static constexpr uint32_t kJoltGapMs = 90;      // jolts closer than this are the same one
    static constexpr uint32_t kWindowMs = 1000;     // jolts must come within this window
    static constexpr int kJoltsNeeded = 4;
    static constexpr uint32_t kCooldownMs = 1500;   // after a shake nothing triggers for this long

private:
    uint32_t jolt_ms_[kJoltsNeeded] = {};
    int count_ = 0;
    uint32_t last_jolt_ms_ = 0;
    uint32_t blocked_until_ms_ = 0;
    bool had_jolt_ = false;
};

// ---- Spirit level ------------------------------------------------------------------------------------------------
struct LevelReading {
    bool flat = false;      // CHIPO lies roughly flat (gravity mostly on the screen normal): the level makes sense
    float x = 0, y = 0;     // bubble position, -1..1 (full deflection at kLevelFullDeg)
    float angle_deg = 0;    // total tilt away from horizontal
    bool level = false;     // within kLevelOkDeg
};
constexpr float kLevelFullDeg = 10.0f;
constexpr float kLevelOkDeg = 0.6f;
// ax, ay, az in g. The sign of the sensor axes is not documented for the board: mirror_x / mirror_y flip them.
LevelReading LevelFromAccel(float ax, float ay, float az, bool mirror_x, bool mirror_y);

// ---- Magic 8-ball -------------------------------------------------------------------------------------------------
int EightBallCount();
const char* EightBallAnswer(int index);  // UTF-8 Spanish; index wraps
// -1 = no, 0 = maybe, 1 = yes (for the face)
int EightBallMood(int index);

// ---- Text helpers ------------------------------------------------------------------------------------------------
// Replaces accented letters by plain ones (for fonts that lack them): "Sí, sin duda" -> "Si, sin duda".
std::string FoldAccents(const char* utf8);
// "09:41" (24 h). Returns false when the hour/minute are out of range.
bool FormatClock(int hour, int minute, char* out, size_t size);
// 0..1 of how much of a countdown is left (clamped); total 0 gives 0.
float RemainingFraction(uint32_t remaining_ms, uint32_t total_ms);

// ---- Voice -------------------------------------------------------------------------------------------------------
enum class ToolsIntent : uint8_t {
    None,
    Level,         // "nivel", "burbuja", "modo nivel"
    EightBall,     // "bola mágica", "bola ocho", "oráculo"
    Canvas,        // "pizarra", "lienzo", "vamos a dibujar"
    ClearCanvas,   // "borra" / "limpia" (only while the canvas is open)
    SubtitlesOn,   // "activa los subtítulos"
    SubtitlesOff,  // "quita los subtítulos"
    InfoOn,        // "muestra la información"
    InfoOff,       // "oculta la información"
    Close          // "cierra", "salir", "vuelve" (only while a tool is open)
};
ToolsIntent ParseToolsCommand(const char* text, bool tool_open, bool canvas_open);
