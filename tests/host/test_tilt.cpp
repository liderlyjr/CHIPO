// Host-side unit tests for ChipoTilt. No ESP-IDF or LVGL needed:
//   see tests/host/run.sh
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>

#include "chipo_tilt.h"

static int g_failures = 0;
#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (!(cond)) {                                    \
            ++g_failures;                                 \
            std::printf("FAIL line %d: ", __LINE__);      \
            std::printf(__VA_ARGS__);                     \
            std::printf("\n");                            \
        }                                                 \
    } while (0)

constexpr float kDeg = 3.14159265f / 180.0f;

// Four possible sensor mountings: which sensor axis carries gravity when CHIPO stands upright.
struct Mount {
    const char* name;
    int axis;     // 0 = x, 1 = y
    float sign;   // +1 / -1
};
static const Mount kMounts[] = {{"+Y", 1, 1}, {"-Y", 1, -1}, {"+X", 0, 1}, {"-X", 0, -1}};

// Builds a sample for a body rotated by `roll` (tipping sideways, rotation around the screen
// normal) and `pitch` (leaning towards the viewer), in the mounting's own axes.
static ImuSample Make(const Mount& m, float roll_deg, float pitch_deg, float noise = 0,
                      std::mt19937* rng = nullptr) {
    const float r = roll_deg * kDeg, p = pitch_deg * kDeg;
    const float vert = std::cos(r) * std::cos(p) * m.sign;  // along the vertical axis
    const float horiz = std::sin(r) * std::cos(p);          // along the horizontal axis
    const float z = std::sin(p);
    float v[3];
    const int h_axis = 1 - m.axis;
    v[m.axis] = vert;
    v[h_axis] = horiz;
    v[2] = z;
    if (noise > 0 && rng) {
        std::normal_distribution<float> n(0.0f, noise);
        for (float& c : v) c += n(*rng);
    }
    return {v[0], v[1], v[2]};
}

static uint32_t Run(ChipoTilt& t, const ImuSample& s, uint32_t from_ms, uint32_t duration_ms,
                    uint32_t step = 40) {
    uint32_t now = from_ms;
    for (; now < from_ms + duration_ms; now += step) t.Update(s, now);
    return now;
}

int main() {
    std::mt19937 rng(7);

    for (const Mount& m : kMounts) {
        std::printf("-- mounting %s\n", m.name);
        ChipoTilt t(false);
        uint32_t now = 0;

        // Not calibrated before standing still long enough.
        now = Run(t, Make(m, 0, 0), now, 800);
        CHECK(!t.output().calibrated, "calibrated too early (%s)", m.name);
        now = Run(t, Make(m, 0, 0), now, 800);
        CHECK(t.output().calibrated, "did not calibrate (%s)", m.name);
        CHECK(t.vertical_axis() == m.axis, "wrong vertical axis %d (%s)", t.vertical_axis(), m.name);
        CHECK(t.output().angle_deg < 1.0f, "upright angle %.2f", t.output().angle_deg);

        // Gentle lean one way, then the other: opposite signs, magnitude ~ sin(angle).
        now = Run(t, Make(m, 20, 0), now, 600);
        // Which physical side is "right" depends on the (undocumented) sensor mounting, so only
        // the size of the push and the symmetry are checked here; CHIPO_TILT_MIRROR_X picks the side.
        const float first = t.output().push_x;
        CHECK(std::fabs(std::fabs(first) - std::sin(20 * kDeg)) < 0.06f, "push %.3f vs %.3f", first,
              std::sin(20 * kDeg));
        CHECK(std::fabs(t.output().angle_deg - 20) < 1.5f, "angle %.2f", t.output().angle_deg);
        CHECK(!t.output().lying, "lying at 20 deg");
        now = Run(t, Make(m, -20, 0), now, 800);
        CHECK(t.output().push_x * first < -0.05f && std::fabs(t.output().push_x) > 0.25f,
              "signs not opposite");
        now = Run(t, Make(m, 0, 0), now, 800);

        // Leaning towards the viewer shows up in lean_z, not in push_x.
        now = Run(t, Make(m, 0, 25), now, 600);
        CHECK(std::fabs(t.output().lean_z - std::sin(25 * kDeg)) < 0.06f, "lean_z %.3f",
              t.output().lean_z);
        CHECK(std::fabs(t.output().push_x) < 0.06f, "push_x %.3f on pure pitch", t.output().push_x);
        now = Run(t, Make(m, 0, -25), now, 600);
        CHECK(std::fabs(t.output().lean_z - std::sin(25 * kDeg)) < 0.06f, "lean_z sign not dropped");
        now = Run(t, Make(m, 0, 0), now, 800);

        // Mirror flag flips left/right only.
        ChipoTilt mirrored(true);
        uint32_t n2 = Run(mirrored, Make(m, 0, 0), 0, 1600);
        Run(mirrored, Make(m, 20, 0), n2, 600);
        CHECK(mirrored.output().push_x * first < -0.05f, "mirror did not flip push_x");

        // A sudden tip-over (one sample) is a startle and then a fall-sized angle.
        bool startled = false;
        t.Update(Make(m, 60, 0), now);
        startled = t.output().startle;
        now += 40;
        CHECK(startled, "sudden 60 deg jump did not startle");
        now = Run(t, Make(m, 60, 0), now, 1200);
        CHECK(t.output().angle_deg > 45.0f, "angle after fall %.1f", t.output().angle_deg);

        // Lying: needs 1.5 s over 75 deg; recovers when upright again.
        now = Run(t, Make(m, 90, 0), now, 1000);
        CHECK(!t.output().lying, "lying too soon");
        now = Run(t, Make(m, 90, 0), now, 1200);
        CHECK(t.output().lying, "should be lying");
        now = Run(t, Make(m, 0, 0), now, 1500);
        CHECK(!t.output().lying, "still lying after standing up");
        CHECK(t.output().angle_deg < 2.0f, "angle after standing up %.2f", t.output().angle_deg);

        // Free fall.
        ChipoTilt tf;
        uint32_t n3 = Run(tf, Make(m, 0, 0), 0, 1600);
        tf.Update({0.02f, 0.03f, 0.01f}, n3);
        CHECK(tf.output().startle, "free fall did not startle");

        // Sensor noise (0.01 g) over a minute must not cause false startles or drift.
        ChipoTilt tn;
        uint32_t n4 = 0;
        int false_startles = 0;
        float max_angle = 0;
        for (int i = 0; i < 1500; ++i, n4 += 40) {
            tn.Update(Make(m, 0, 0, 0.01f, &rng), n4);
            false_startles += tn.output().startle ? 1 : 0;
            if (i > 60 && tn.output().angle_deg > max_angle) max_angle = tn.output().angle_deg;
        }
        CHECK(tn.output().calibrated, "no calibration under noise");
        CHECK(false_startles == 0, "%d false startles", false_startles);
        CHECK(max_angle < 3.0f, "noise angle %.2f", max_angle);

        // Resting in a new slightly tilted pose for 10+ s becomes the new home.
        ChipoTilt tr;
        uint32_t n5 = Run(tr, Make(m, 0, 0), 0, 1600);
        n5 = Run(tr, Make(m, 15, 0), n5, 1000);
        CHECK(tr.output().angle_deg > 10.0f, "should see the 15 deg lean first");
        Run(tr, Make(m, 15, 0), n5, 12000);
        CHECK(tr.output().angle_deg < 2.0f, "did not adopt new rest pose (%.1f)", tr.output().angle_deg);
    }

    // Booting lying flat on a desk: never calibrates, never claims to know the tilt.
    {
        ChipoTilt t;
        Run(t, {0.0f, 0.0f, 1.0f}, 0, 6000);
        CHECK(!t.output().calibrated, "calibrated while lying flat");
        CHECK(!t.output().lying && t.output().angle_deg == 0, "reported tilt while uncalibrated");
    }

    // Being carried around (never still) must not calibrate on a random pose.
    {
        ChipoTilt t;
        uint32_t now = 0;
        for (int i = 0; i < 200; ++i, now += 40) {
            const float a = 0.5f * std::sin(i * 0.5f);
            t.Update({a, 1.0f - std::fabs(a) * 0.3f, -a}, now);
        }
        CHECK(!t.output().calibrated, "calibrated while moving");
    }

    if (g_failures == 0) std::printf("ALL TILT TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
