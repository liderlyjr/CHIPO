// Host-side tests for the gyroscope gesture maths (spin, nod, shake). No ESP-IDF, no LVGL.
// Run under AddressSanitizer/UBSan: see tests/host/run.sh.
#include <cmath>
#include <cstdio>

#include "chipo_gyro.h"

static int g_failures = 0;
#define CHECK(cond, ...)                              \
    do {                                              \
        if (!(cond)) {                                \
            ++g_failures;                             \
            std::printf("FAIL line %d: ", __LINE__);  \
            std::printf(__VA_ARGS__);                 \
            std::printf("\n");                        \
        }                                             \
    } while (0)

namespace {
struct Rig {
    ChipoGyro gyro;
    uint32_t now = 1000;
    bool calibrated = true;
    int vertical_axis = 1;  // sensor Y is the screen's vertical, so X is horizontal
    bool saw_nod = false, saw_shake = false, saw_spin_end = false;
    uint32_t spin_end_ms = 0;
    int nods = 0, shakes = 0;

    void Step(float gx, float gy, float gz, uint32_t dt = 40) {
        now += dt;
        ImuSample s;
        s.has_gyro = true;
        s.gx = gx; s.gy = gy; s.gz = gz;
        const GyroOutput& o = gyro.Update(s, calibrated, vertical_axis, now);
        if (o.nod) { saw_nod = true; ++nods; }
        if (o.shake) { saw_shake = true; ++shakes; }
        if (o.spin_ended) { saw_spin_end = true; spin_end_ms = o.spin_ms; }
    }
    void Hold(uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += 40) Step(0, 0, 0);
    }
    // Square wave around one axis: `swings` half-periods of `half_ms` each, +/- amplitude.
    void Swing(int axis, float amplitude, int swings, uint32_t half_ms) {
        float sign = 1;
        for (int i = 0; i < swings; ++i) {
            for (uint32_t t = 0; t < half_ms; t += 40) {
                float g[3] = {0, 0, 0};
                g[axis] = sign * amplitude;
                Step(g[0], g[1], g[2]);
            }
            sign = -sign;
        }
    }
};
}  // namespace

int main() {
    // ---- samples without gyroscope data do nothing -------------------------------------------
    {
        Rig r;
        ImuSample s;  // has_gyro = false
        for (int i = 0; i < 50; ++i) r.gyro.Update(s, true, 1, 1000 + 40 * i);
        CHECK(!r.gyro.output().available, "no gyro data must not be reported as available");
        CHECK(!r.gyro.output().spinning, "no spin without data");
    }

    // ---- spin ----------------------------------------------------------------------------------
    {
        Rig r;
        r.Hold(400);
        CHECK(r.gyro.output().available, "gyro available after samples");
        for (int i = 0; i < 9; ++i) r.Step(0, 0, 300);
        CHECK(r.gyro.output().spinning, "fast rotation around the screen normal must be a spin");
        for (int i = 0; i < 50; ++i) r.Step(0, 0, 300);
        CHECK(r.gyro.output().spinning && r.gyro.output().spin_ms > 1500, "spin lasts (%u ms)",
              static_cast<unsigned>(r.gyro.output().spin_ms));
        r.Hold(1000);
        CHECK(!r.gyro.output().spinning, "spin ends when rotation stops");
        CHECK(r.saw_spin_end && r.spin_end_ms >= 2000, "spin_ended reported with its length (%u)",
              static_cast<unsigned>(r.spin_end_ms));
    }
    {
        Rig r;  // slow turning is not a spin
        for (int i = 0; i < 60; ++i) r.Step(0, 0, 60);
        CHECK(!r.gyro.output().spinning, "slow turn is not a spin");
    }
    {
        Rig r;  // spin works before the home pose is known, and in both directions
        r.calibrated = false;
        for (int i = 0; i < 12; ++i) r.Step(0, 0, -280);
        CHECK(r.gyro.output().spinning, "spin needs no calibration and ignores direction");
    }
    {
        Rig r;  // a fast turn around an in-plane axis is not a spin
        for (int i = 0; i < 30; ++i) r.Step(300, 0, 0);
        CHECK(!r.gyro.output().spinning, "rotation around an in-plane axis is not a spin");
    }

    // ---- nod ("yes"): back and forth around the horizontal in-plane axis (sensor X) -----------
    {
        Rig r;
        r.Hold(400);
        r.Swing(0, 220, 5, 160);
        CHECK(r.saw_nod && !r.saw_shake, "nod recognised (nod=%d shake=%d)", r.saw_nod, r.saw_shake);
    }
    {
        Rig r;  // mirrored sensor mounting (vertical axis is X): the nod is around Y
        r.vertical_axis = 0;
        r.Hold(400);
        r.Swing(1, 220, 5, 160);
        CHECK(r.saw_nod && !r.saw_shake, "nod recognised with the other mounting");
    }

    // ---- shake ("no"): back and forth around the vertical in-plane axis (sensor Y) ------------
    {
        Rig r;
        r.Hold(400);
        r.Swing(1, 220, 5, 160);
        CHECK(r.saw_shake && !r.saw_nod, "shake recognised (nod=%d shake=%d)", r.saw_nod, r.saw_shake);
    }

    // ---- things that must NOT be gestures -----------------------------------------------------
    {
        Rig r;  // one turn out and back (a single reversal): picking CHIPO up and putting it down
        r.Hold(400);
        r.Swing(1, 220, 2, 240);
        r.Hold(1500);
        CHECK(!r.saw_nod && !r.saw_shake, "one turn and back is not a gesture");
    }
    {
        Rig r;  // slow wobble: strong samples more than 600 ms apart never chain up
        r.Hold(400);
        for (int i = 0; i < 6; ++i) {
            r.Step(0, (i % 2 ? -1 : 1) * 200, 0);
            r.Hold(800);
        }
        CHECK(!r.saw_nod && !r.saw_shake, "slow wobble is not a gesture");
    }
    {
        Rig r;  // wild shake: both axes about equally: no clean "yes" or "no"
        r.Hold(400);
        float sign = 1;
        for (int i = 0; i < 6; ++i) {
            for (int k = 0; k < 4; ++k) r.Step(sign * 220, sign * 215, 0);
            sign = -sign;
        }
        CHECK(!r.saw_nod && !r.saw_shake, "a diagonal shake is neither a nod nor a shake");
    }
    {
        Rig r;  // nothing before the home pose is known
        r.calibrated = false;
        r.Swing(0, 220, 6, 160);
        CHECK(!r.saw_nod && !r.saw_shake, "no nod or shake without calibration");
    }
    {
        Rig r;  // rotations smaller than the threshold are ignored
        r.Swing(0, 60, 10, 160);
        CHECK(!r.saw_nod && !r.saw_shake, "gentle swinging is not a gesture");
    }
    {
        Rig r;  // no nod while spinning
        for (int i = 0; i < 12; ++i) r.Step(0, 0, 300);
        float sign = 1;
        for (int i = 0; i < 12; ++i) {
            r.Step(sign * 220, 0, 300);
            if (i % 3 == 2) sign = -sign;
        }
        CHECK(!r.saw_nod, "no nod while spinning");
    }

    // ---- cooldown ------------------------------------------------------------------------------
    {
        Rig r;
        r.Hold(400);
        r.Swing(0, 220, 5, 160);
        const int first = r.nods;
        r.Swing(0, 220, 5, 160);  // right away: still cooling down
        CHECK(first == 1 && r.nods == 1, "cooldown stops a second nod right away (%d, %d)", first, r.nods);
        r.Hold(1600);
        r.Swing(0, 220, 5, 160);
        CHECK(r.nods == 2, "a nod after the cooldown counts again (%d)", r.nods);
    }

    // ---- bad input -----------------------------------------------------------------------------
    {
        Rig r;
        r.vertical_axis = 7;  // garbage must not read outside the array
        r.Swing(0, 220, 6, 160);
        r.vertical_axis = -1;
        r.Swing(0, 220, 6, 160);
        CHECK(!r.saw_nod && !r.saw_shake, "an unknown vertical axis disables nod and shake");
        ImuSample s;
        s.has_gyro = true;
        s.gx = 1e9f; s.gy = -1e9f; s.gz = 1e9f;
        r.gyro.Update(s, true, 1, r.now + 40);  // must not crash or hang
    }

    if (g_failures == 0) std::printf("ALL GYRO TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
