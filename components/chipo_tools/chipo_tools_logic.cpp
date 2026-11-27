#include "chipo_tools_logic.h"

#include <cmath>
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
    r.flat = nz > 0.94f;                    // within ~20 degrees of lying flat
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
    // The AMOLED's screen axes are rotated relative to the sensor axes.
    return {-accel_x_mps2 * 8.0f, accel_y_mps2 * 8.0f};
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
