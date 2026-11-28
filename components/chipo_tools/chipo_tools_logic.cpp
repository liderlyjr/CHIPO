#include "chipo_tools_logic.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <vector>

#include "chipo_drive_voice.h"  // DriveNormalizeText

// ---- Shake -------------------------------------------------------------------------------------------------------
bool ShakeDetector::Feed(float ax, float ay, float az, uint32_t now_ms) {
    const float mag = std::sqrt(ax * ax + ay * ay + az * az);
    if (std::fabs(mag - 1.0f) < kJoltG) return false;
    if (static_cast<int32_t>(now_ms - blocked_until_ms_) < 0) return false;
    if (had_jolt_ && now_ms - last_jolt_ms_ < kJoltGapMs) return false;  // still the same jolt
    had_jolt_ = true;
    last_jolt_ms_ = now_ms;
    // Drop jolts that fell out of the window, then add this one.
    int kept = 0;
    for (int i = 0; i < count_; ++i) {
        if (now_ms - jolt_ms_[i] <= kWindowMs) jolt_ms_[kept++] = jolt_ms_[i];
    }
    count_ = kept;
    if (count_ < kJoltsNeeded) jolt_ms_[count_++] = now_ms;
    if (count_ >= kJoltsNeeded) {
        count_ = 0;
        blocked_until_ms_ = now_ms + kCooldownMs;
        return true;
    }
    return false;
}

// ---- Spirit level ------------------------------------------------------------------------------------------------
LevelReading LevelFromAccel(float ax, float ay, float az, bool mirror_x, bool mirror_y) {
    LevelReading r;
    const float mag = std::sqrt(ax * ax + ay * ay + az * az);
    if (mag < 0.5f || mag > 1.6f) return r;  // moving: the reading is not gravity
    const float nz = std::fabs(az) / mag;
    r.flat = nz > std::cos(kLevelMaxTiltDeg * 0.0174533f); // accept up to 35 degrees from horizontal
    const float in_plane = std::sqrt(ax * ax + ay * ay) / mag;
    r.angle_deg = std::asin(in_plane > 1.0f ? 1.0f : in_plane) * 57.29578f;
    if (!r.flat) return r;
    const float full = std::sin(kLevelFullDeg * 0.0174533f);
    float bx = ax / mag / full, by = ay / mag / full;
    if (mirror_x) bx = -bx;
    if (mirror_y) by = -by;
    const float len = std::sqrt(bx * bx + by * by);
    if (len > 1.0f) { bx /= len; by /= len; }
    r.x = bx;
    r.y = by;
    r.level = r.angle_deg < kLevelOkDeg;
    return r;
}

// ---- Gravitysphere -----------------------------------------------------------------------------------------------
GravityBallDelta GravityBallFromAccel(float accel_x_mps2, float accel_y_mps2) {
    // The display is rotated 90 degrees from the QMI8658 board: screen right = sensor +X, screen up = sensor -Y
    // (screen normal = sensor -Z). The ball rolls along gravity, which is minus the measured specific force.
    // Waveshare's own demo (x -= accelY, y += accelX) assumed the unrotated panel; on CHIPO it made a tilt to the left
    // roll the ball downwards (checked on the board).
    return {-accel_x_mps2 * 8.0f, -accel_y_mps2 * 8.0f};
}

// Frame of the sensor on the CHIPO board, derived from Waveshare's demo mapping plus the rotated display and confirmed
// by the gravity ball on the board: screen right = +X, screen up = -Y, out of the screen = -Z (specific force readings).
// The other signs stay configurable: flip a macro if an axis looks mirrored.
#ifndef CHIPO_HORIZON_VERT_SIGN
#define CHIPO_HORIZON_VERT_SIGN -1.0f
#endif
#ifndef CHIPO_HORIZON_ROLL_SIGN
#define CHIPO_HORIZON_ROLL_SIGN 1.0f
#endif
#ifndef CHIPO_HORIZON_PITCH_SIGN
#define CHIPO_HORIZON_PITCH_SIGN 1.0f
#endif

bool HorizonRawAngles(float ax_g, float ay_g, float az_g, float* roll_deg, float* pitch_deg) {
    const float magnitude = std::sqrt(ax_g * ax_g + ay_g * ay_g + az_g * az_g);
    if (magnitude < 0.55f || magnitude > 1.55f) return false;
    // Tilting the top of the screen backwards (screen facing up) makes Z more negative on this board: the horizon moves down.
    const float zn = std::clamp(az_g / magnitude, -1.0f, 1.0f);
    const float pitch = std::asin(zn) * 57.2957795f * CHIPO_HORIZON_PITCH_SIGN;
    // Roll is the in-plane angle of gravity; undefined when the board lies flat, so it holds 0 instead of jittering.
    const float in_plane = std::sqrt(ax_g * ax_g + ay_g * ay_g);
    const float roll = in_plane < 0.2f ? 0.0f
        : std::atan2(ax_g, CHIPO_HORIZON_VERT_SIGN * ay_g) * 57.2957795f * CHIPO_HORIZON_ROLL_SIGN;
    if (roll_deg) *roll_deg = roll;
    if (pitch_deg) *pitch_deg = pitch;
    return true;
}

HorizonReading HorizonFromAccel(float ax_g, float ay_g, float az_g, float zero_roll_deg, float zero_pitch_deg) {
    HorizonReading out{};
    float roll = 0.0f, pitch = 0.0f;
    if (!HorizonRawAngles(ax_g, ay_g, az_g, &roll, &pitch)) return out;
    // Roll is circular: crossing +180/-180 must be a small change, not a 360-degree jump.
    out.roll_deg = std::clamp(std::remainder(roll - zero_roll_deg, 360.0f), -89.0f, 89.0f);
    out.pitch_deg = std::clamp(pitch - zero_pitch_deg, -45.0f, 45.0f);
    out.valid = true;
    return out;
}

// ---- Text helpers ------------------------------------------------------------------------------------------------
std::string FoldAccents(const char* utf8) {
    std::string out;
    if (!utf8) return out;
    struct Map { const char* from; char to; };
    static const Map kMap[] = {
        {"á", 'a'}, {"é", 'e'}, {"í", 'i'}, {"ó", 'o'}, {"ú", 'u'}, {"ü", 'u'}, {"ñ", 'n'},
        {"Á", 'A'}, {"É", 'E'}, {"Í", 'I'}, {"Ó", 'O'}, {"Ú", 'U'}, {"Ü", 'U'}, {"Ñ", 'N'},
        {"¿", 0}, {"¡", 0},
    };
    for (const char* p = utf8; *p;) {
        bool done = false;
        for (const Map& m : kMap) {
            const size_t n = std::char_traits<char>::length(m.from);
            if (std::char_traits<char>::compare(p, m.from, n) == 0) {
                if (m.to) out.push_back(m.to);
                p += n;
                done = true;
                break;
            }
        }
        if (!done) out.push_back(*p++);
    }
    return out;
}

bool FormatClock(int hour, int minute, char* out, size_t size) {
    if (!out || size < 6 || hour < 0 || hour > 23 || minute < 0 || minute > 59) return false;
    std::snprintf(out, size, "%02d:%02d", hour, minute);
    return true;
}

float RemainingFraction(uint32_t remaining_ms, uint32_t total_ms) {
    if (total_ms == 0) return 0.0f;
    const float f = static_cast<float>(remaining_ms) / static_cast<float>(total_ms);
    return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
}

// ---- Voice -------------------------------------------------------------------------------------------------------
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
}  // namespace

ToolsIntent ParseToolsCommand(const char* text, bool tool_open, bool canvas_open) {
    Tokens t = Split(DriveNormalizeText(text));
    Tokens core;
    for (const auto& w : t) {
        if (In(w, {"chipo", "chippo", "shipo", "chipu", "chepo", "cipo", "por", "favor", "porfa", "oye", "venga", "ya", "pues", "hey", "ey", "la", "el", "los", "las", "un", "una", "de", "del", "en", "a", "mi", "tu"})) continue;
        core.push_back(w);
    }
    if (core.empty() || core.size() > 6) return ToolsIntent::None;

    const bool on_verb = Has(core, {"activa", "activar", "pon", "ponme", "muestra", "muestrame", "enciende", "quiero", "usa", "abre", "modo"});
    const bool off_verb = Has(core, {"quita", "quitar", "desactiva", "desactivar", "oculta", "ocultar", "apaga", "esconde", "sin"});

    if (Has(core, {"subtitulos", "subtitulo", "subtitulado"})) {
        if (off_verb) return ToolsIntent::SubtitlesOff;
        if (on_verb || core.size() <= 2) return ToolsIntent::SubtitlesOn;
    }
    if (Has(core, {"informacion", "datos"}) && !Has(core, {"tiempo", "clima"})) {
        if (off_verb) return ToolsIntent::InfoOff;
        if (on_verb) return ToolsIntent::InfoOn;
    }
    if (Has(core, {"horizonte"}) && Has(core, {"artificial", "modo", "pon", "activa"})) return ToolsIntent::Horizon;
    if (Has(core, {"gravity", "gravedad"}) || (Has(core, {"pelota"}) && Has(core, {"gravedad", "inclinar"}))) return ToolsIntent::GravityBall;
    if (Has(core, {"pizarra", "lienzo", "dibujar", "dibujo", "dibuja"}) && !off_verb) return ToolsIntent::Canvas;
    if (Has(core, {"nivel", "burbuja"}) && !Has(core, {"bateria", "volumen", "brillo", "agua", "sonido", "ruido", "luz", "carga", "animo", "bateria"})) {
        if (Has(core, {"burbuja"}) || on_verb || core.size() <= 2) return ToolsIntent::Level;
    }
    if (canvas_open && Has(core, {"borra", "borrar", "limpia", "limpiar"})) return ToolsIntent::ClearCanvas;
    if (tool_open && core.size() <= 3 && Has(core, {"cierra", "cerrar", "salir", "sal", "vuelve", "volver", "atras", "basta", "termina", "fuera"})) {
        return ToolsIntent::Close;
    }
    return ToolsIntent::None;
}

// ---- Artificial horizon renderer ---------------------------------------------------------------------------------
namespace {
// Fills px[x0..x1] (inclusive, already clamped) using 32-bit writes where the address allows it: the buffer lives in
// PSRAM, where fewer, wider writes are noticeably faster.
inline void FillSpan(uint16_t* row, int x0, int x1, uint16_t c) {
    if (x1 < x0) return;
    int x = x0;
    if ((reinterpret_cast<uintptr_t>(row + x) & 3u) != 0 && x <= x1) row[x++] = c;
    const uint32_t pair = (static_cast<uint32_t>(c) << 16) | c;
    for (; x + 1 <= x1; x += 2) *reinterpret_cast<uint32_t*>(row + x) = pair;
    if (x <= x1) row[x] = c;
}
inline int ClampX(float v, int w) {  // float -> column in [-1, w], safe against huge values
    if (!(v > -1.0f)) return -1;
    if (v > static_cast<float>(w)) return w;
    return static_cast<int>(v);
}
}  // namespace

void HorizonRender(uint16_t* px, int w, int h, float roll_deg, float pitch_deg,
                   uint16_t sky, uint16_t earth, uint16_t white, uint16_t mark) {
    if (!px || w <= 0 || h <= 0) return;
    const float rad = roll_deg * 0.0174532925f;
    const float sn = std::sin(rad), cs = std::cos(rad);
    const float pitch_px = pitch_deg * 5.0f;
    const int cx = w / 2, cy = h / 2;
    constexpr float kHalfLine = 1.4f;  // the white line is |d| < 1.4 px
    const bool flat_row = std::fabs(sn) < 1e-4f;
    for (int y = 0; y < h; ++y) {
        uint16_t* row = px + static_cast<size_t>(y) * static_cast<size_t>(w);
        const float a = static_cast<float>(y - cy) * cs + pitch_px;  // d = -dx * sn + a
        if (flat_row) {
            // The line is horizontal: the whole row has one colour.
            FillSpan(row, 0, w - 1, std::fabs(a) < kHalfLine ? white : (a >= 0.0f ? earth : sky));
        } else {
            const float t = a / sn;                       // dx where d = 0
            const float band = kHalfLine / std::fabs(sn);  // half width of the white line along the row
            const float edge = static_cast<float>(cx) + t;
            FillSpan(row, 0, w - 1, sky);
            if (sn > 0.0f) {  // earth where dx <= t
                const int e = std::min(ClampX(std::floor(edge), w), w - 1);
                FillSpan(row, 0, e, earth);
            } else {          // earth where dx >= t
                const int s = std::max(ClampX(std::ceil(edge), w), 0);
                FillSpan(row, s, w - 1, earth);
            }
            const int ws = std::max(ClampX(std::floor(edge - band) + 1.0f, w), 0);
            const int we = std::min(ClampX(std::ceil(edge + band) - 1.0f, w), w - 1);
            FillSpan(row, ws, we, white);
        }
        // Centre mark: a horizontal bar on each side and a short vertical tick.
        const int ry = std::abs(y - cy);
        if (ry <= 2) {
            for (int x = std::max(cx - 88, 0); x <= std::min(cx - 30, w - 1); ++x) row[x] = mark;
            for (int x = std::max(cx + 30, 0); x <= std::min(cx + 88, w - 1); ++x) row[x] = mark;
        }
        if (ry <= 14) {
            for (int x = std::max(cx - 2, 0); x <= std::min(cx + 2, w - 1); ++x) row[x] = mark;
        }
    }
}
