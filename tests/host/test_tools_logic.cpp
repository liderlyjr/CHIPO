#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <vector>

#include "chipo_tools_logic.h"

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void Is(const char* t, ToolsIntent want, bool open = false, bool canvas = false) {
    const ToolsIntent got = ParseToolsCommand(t, open, canvas);
    CHECK(got == want, "\"%s\" -> %d, want %d", t, static_cast<int>(got), static_cast<int>(want));
}

int main() {
    // ---- voice ----
    Is("Chipo, pon el nivel", ToolsIntent::Level);
    Is("modo nivel", ToolsIntent::Level);
    Is("nivel de burbuja", ToolsIntent::Level);
    Is("la burbuja", ToolsIntent::Level);
    Is("cuál es el nivel de batería", ToolsIntent::None);
    Is("sube el nivel del volumen", ToolsIntent::None);
    Is("activa gravity ball", ToolsIntent::GravityBall);
    Is("pelota de gravedad", ToolsIntent::GravityBall);
    Is("horizonte artificial", ToolsIntent::Horizon);
    Is("pon el horizonte", ToolsIntent::Horizon);
    Is("vamos a dibujar", ToolsIntent::Canvas);
    Is("abre la pizarra", ToolsIntent::Canvas);
    Is("lienzo", ToolsIntent::Canvas);
    Is("borra", ToolsIntent::None);
    Is("borra", ToolsIntent::ClearCanvas, true, true);
    Is("limpia el lienzo", ToolsIntent::Canvas);          // the canvas word wins when the lienzo is named
    Is("activa los subtítulos", ToolsIntent::SubtitlesOn);
    Is("subtítulos", ToolsIntent::SubtitlesOn);
    Is("quita los subtítulos", ToolsIntent::SubtitlesOff);
    Is("sin subtítulos", ToolsIntent::SubtitlesOff);
    Is("muestra la información", ToolsIntent::InfoOn);
    Is("oculta la información", ToolsIntent::InfoOff);
    Is("quita los datos", ToolsIntent::InfoOff);
    Is("muestra los datos del tiempo", ToolsIntent::None);
    Is("cierra", ToolsIntent::None);
    Is("cierra", ToolsIntent::Close, true);
    Is("salir", ToolsIntent::Close, true);
    Is("vuelve", ToolsIntent::Close, true);
    Is("qué hora es", ToolsIntent::None);
    Is("qué tiempo hace hoy", ToolsIntent::None);
    Is("cuéntame la historia de un niño que dibujaba en la pared de su casa todos los días", ToolsIntent::None);
    Is("", ToolsIntent::None);
    Is(nullptr, ToolsIntent::None);

    // ---- shake ----
    {
        ShakeDetector d;
        uint32_t t = 1000;
        int shakes = 0;
        // Resting: 1 g.
        for (int i = 0; i < 100; ++i, t += 20) shakes += d.Feed(0.0f, 0.0f, 1.0f, t);
        CHECK(shakes == 0, "rest must not shake");
        // A slow tilt (still 1 g in magnitude) is not a shake.
        for (int i = 0; i < 100; ++i, t += 20) shakes += d.Feed(std::sin(i * 0.05f), 0.0f, std::cos(i * 0.05f), t);
        CHECK(shakes == 0, "slow tilt must not shake");
        // One bump (a table knock) is not a shake.
        d.Feed(0.0f, 0.0f, 2.2f, t); t += 20;
        for (int i = 0; i < 40; ++i, t += 20) shakes += d.Feed(0.0f, 0.0f, 1.0f, t);
        CHECK(shakes == 0, "single bump must not shake");
        // A real shake: alternating 2.2 g / -0.1 g every 100 ms.
        for (int i = 0; i < 12; ++i, t += 100) shakes += d.Feed(0.0f, 0.0f, (i % 2) ? 0.1f : 2.2f, t);
        CHECK(shakes == 1, "a real shake must trigger once, got %d", shakes);
        // A second shake right after is blocked by the cooldown, a later one works.
        for (int i = 0; i < 12; ++i, t += 100) shakes += d.Feed(0.0f, 0.0f, (i % 2) ? 0.1f : 2.2f, t);
        CHECK(shakes >= 1 && shakes <= 2, "cooldown, got %d", shakes);
        for (int i = 0; i < 80; ++i, t += 20) d.Feed(0.0f, 0.0f, 1.0f, t);
        const int before = shakes;
        for (int i = 0; i < 12; ++i, t += 100) shakes += d.Feed(0.0f, 0.0f, (i % 2) ? 0.1f : 2.2f, t);
        CHECK(shakes == before + 1, "later shake must trigger, got %d vs %d", shakes, before);
        // Slow jolts (every 2 s) do not add up.
        ShakeDetector s;
        int slow = 0;
        for (int i = 0; i < 10; ++i) slow += s.Feed(0, 0, 2.5f, 10000 + i * 2000);
        CHECK(slow == 0, "slow jolts must not add up");
    }

    // ---- level ----
    {
        LevelReading r = LevelFromAccel(0, 0, 1, false, false);
        CHECK(r.flat && r.level && std::fabs(r.x) < 1e-4f && std::fabs(r.y) < 1e-4f, "flat and level");
        r = LevelFromAccel(0, 0, -1, false, false);
        CHECK(r.flat && r.level, "upside down on the table is flat too");
        const float s = std::sin(3.0f * 0.0174533f), c = std::cos(3.0f * 0.0174533f);
        r = LevelFromAccel(s, 0, c, false, false);
        CHECK(r.flat && !r.level && std::fabs(r.angle_deg - 3.0f) < 0.05f, "3 degrees: %f", r.angle_deg);
        CHECK(r.x > 0.08f && r.x < 0.12f && std::fabs(r.y) < 1e-4f, "3-degree bubble x on 30-degree scale: %f", r.x);
        LevelReading m = LevelFromAccel(s, 0, c, true, false);
        CHECK(std::fabs(m.x + r.x) < 1e-5f, "mirror x");
        m = LevelFromAccel(0, s, c, false, true);
        CHECK(m.y < -0.08f && m.y > -0.12f, "mirror y with 30-degree scale: %f", m.y);
        const float s10 = std::sin(10.0f * 0.0174533f), c10 = std::cos(10.0f * 0.0174533f);
        r = LevelFromAccel(s10, 0, c10, false, false);
        CHECK(r.flat && std::fabs(r.angle_deg - 10.0f) < 0.05f && r.x > 0.30f && r.x < 0.36f,
              "10 degrees must remain visible and use the wider scale, got angle=%f bubble=%f", r.angle_deg, r.x);
        const float s30 = std::sin(30.0f * 0.0174533f), c30 = std::cos(30.0f * 0.0174533f);
        r = LevelFromAccel(s30, 0, c30, false, false);
        CHECK(r.flat && std::fabs(r.x - 1.0f) < 1e-4f, "30 degrees must reach full bubble deflection");
        const float s40 = std::sin(40.0f * 0.0174533f), c40 = std::cos(40.0f * 0.0174533f);
        r = LevelFromAccel(s40, 0, c40, false, false);
        CHECK(!r.flat, "40 degrees should be outside the level's operating range");
        r = LevelFromAccel(0, 1, 0, false, false);
        CHECK(!r.flat, "standing upright is not flat");
        r = LevelFromAccel(0.2f, 0.9f, 0.4f, false, false);
        CHECK(!r.flat, "leaning is not flat");
        r = LevelFromAccel(0, 0, 3.0f, false, false);
        CHECK(!r.flat && !r.level, "a jolt is not a reading");
        r = LevelFromAccel(0.12f, 0.12f, 0.98f, false, false);
        CHECK(std::sqrt(r.x * r.x + r.y * r.y) <= 1.0001f, "bubble stays inside the circle");
    }

    // ---- Gravitysphere axis mapping ----
    {
        GravityBallDelta d = GravityBallFromAccel(0.0f, 1.0f);
        CHECK(std::fabs(d.x + 8.0f) < 1e-5f && std::fabs(d.y) < 1e-5f,
              "Y sensor tilt must move horizontally (Waveshare mapping), got (%f,%f)", d.x, d.y);
        d = GravityBallFromAccel(1.0f, 0.0f);
        CHECK(std::fabs(d.x) < 1e-5f && std::fabs(d.y - 8.0f) < 1e-5f,
              "X sensor tilt must move vertically (Waveshare mapping), got (%f,%f)", d.x, d.y);
        d = GravityBallFromAccel(-1.0f, -1.0f);
        CHECK(std::fabs(d.x - 8.0f) < 1e-5f && std::fabs(d.y + 8.0f) < 1e-5f,
              "negative axes must preserve direction, got (%f,%f)", d.x, d.y);
    }

    // ---- artificial horizon (device upright: gravity along sensor X, screen normal = sensor Z) ----
    {
        HorizonReading h = HorizonFromAccel(1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        CHECK(h.valid && std::fabs(h.roll_deg) < 1e-3f && std::fabs(h.pitch_deg) < 1e-3f, "upright horizon should be zero");
        const float p = 12.0f * 0.0174532925f;
        // Tilting back (screen faces up) must move the horizon down and must NOT roll it.
        h = HorizonFromAccel(std::cos(p), 0.0f, std::sin(p), 0.0f, 0.0f);
        CHECK(h.valid && std::fabs(h.roll_deg) < 0.1f && std::fabs(h.pitch_deg + 12.0f) < 0.1f,
              "tilt back = pure negative pitch, got roll %f pitch %f", h.roll_deg, h.pitch_deg);
        h = HorizonFromAccel(std::cos(p), 0.0f, -std::sin(p), 0.0f, 0.0f);
        CHECK(h.valid && std::fabs(h.roll_deg) < 0.1f && std::fabs(h.pitch_deg - 12.0f) < 0.1f,
              "tilt forward = pure positive pitch, got roll %f pitch %f", h.roll_deg, h.pitch_deg);
        h = HorizonFromAccel(std::cos(p), std::sin(p), 0.0f, 0.0f, 0.0f);
        CHECK(h.valid && std::fabs(h.roll_deg - 12.0f) < 0.1f && std::fabs(h.pitch_deg) < 0.1f,
              "sideways tilt = pure roll, got roll %f pitch %f", h.roll_deg, h.pitch_deg);
        h = HorizonFromAccel(1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        float r0 = 0, p0 = 0;
        CHECK(HorizonRawAngles(std::cos(p), 0.0f, std::sin(p), &r0, &p0), "raw angles valid");
        h = HorizonFromAccel(std::cos(p), 0.0f, std::sin(p), r0, p0);
        CHECK(std::fabs(h.pitch_deg) < 0.1f, "calibration cancels the captured pose");
        h = HorizonFromAccel(0.0f, 0.0f, 0.1f, 0.0f, 0.0f);
        CHECK(!h.valid, "free-fall / invalid gravity magnitude must be rejected");
        h = HorizonFromAccel(0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        CHECK(h.valid && std::fabs(h.roll_deg) < 1e-3f, "lying flat holds roll at 0 instead of jittering");
    }

    // ---- horizon renderer: spans must give the same picture as the old per-pixel formula ----
    {
        const int W = 480, H = 480;
        const uint16_t sky = 0x1111, earth = 0x2222, white = 0x3333, mark = 0x4444;
        std::vector<uint16_t> fast(W * H), ref(W * H);
        const float rolls[] = {0.0f, 0.00005f, 3.0f, -17.0f, 45.0f, -45.0f, 89.9f, 90.0f, -90.0f, 135.0f, 179.0f, -179.0f, 180.0f};
        const float pitches[] = {0.0f, 8.0f, -12.5f, 40.0f, -60.0f};
        int worst = 0;
        for (float roll : rolls) for (float pitch : pitches) {
            HorizonRender(fast.data(), W, H, roll, pitch, sky, earth, white, mark);
            const float rad = roll * 0.0174532925f, sn = std::sin(rad), cs = std::cos(rad), pp = pitch * 5.0f;
            for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
                const float dx = static_cast<float>(x - W / 2), dy = static_cast<float>(y - H / 2);
                const float d = -dx * sn + dy * cs + pp;
                uint16_t c = d >= 0.0f ? earth : sky;
                if (std::fabs(d) < 1.4f) c = white;
                const int rx = std::abs(x - W / 2), ry = std::abs(y - H / 2);
                if ((ry <= 2 && rx >= 30 && rx <= 88) || (rx <= 2 && ry <= 14)) c = mark;
                ref[y * W + x] = c;
            }
            int diff = 0;
            for (int i = 0; i < W * H; ++i) diff += fast[i] != ref[i];
            worst = std::max(worst, diff);
            CHECK(diff < 700, "horizon roll %.1f pitch %.1f: %d pixels differ", roll, pitch, diff);
        }
        std::printf("   horizon renderer: worst difference %d of %d pixels (edge rounding only)\n", worst, W * H);
        HorizonRender(nullptr, W, H, 0, 0, sky, earth, white, mark);  // must not crash
        std::vector<uint16_t> tiny(7 * 5);
        HorizonRender(tiny.data(), 7, 5, 33.0f, 1.0f, sky, earth, white, mark);  // odd width, tiny buffer
    }

    // ---- text ----
    {
        CHECK(FoldAccents("Sí, sin duda") == "Si, sin duda", "fold");
        CHECK(FoldAccents("Pregúntame más tarde ¿ok? ¡ñu!") == "Preguntame mas tarde ok? nu!", "fold 2: %s", FoldAccents("Pregúntame más tarde ¿ok? ¡ñu!").c_str());
        CHECK(FoldAccents(nullptr).empty(), "null");
        char buf[8];
        CHECK(FormatClock(9, 41, buf, sizeof buf) && std::strcmp(buf, "09:41") == 0, "clock");
        CHECK(FormatClock(0, 0, buf, sizeof buf) && std::strcmp(buf, "00:00") == 0, "midnight");
        CHECK(!FormatClock(24, 0, buf, sizeof buf) && !FormatClock(5, 60, buf, sizeof buf) && !FormatClock(-1, 0, buf, sizeof buf), "range");
        CHECK(!FormatClock(5, 5, buf, 4), "small buffer");
        CHECK(RemainingFraction(0, 100) == 0.0f && RemainingFraction(100, 100) == 1.0f && RemainingFraction(50, 100) == 0.5f, "fraction");
        CHECK(RemainingFraction(200, 100) == 1.0f && RemainingFraction(5, 0) == 0.0f, "fraction clamp");
    }

    if (g_fail == 0) std::printf("ALL TOOLS LOGIC TESTS PASSED\n");
    return g_fail == 0 ? 0 : 1;
}
