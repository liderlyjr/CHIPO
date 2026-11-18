// Host tests for the CHIPO DRIVE motion detector. Simulated drives only: nothing here has been
// recorded on a real car. See docs/CHIPO_DRIVE.md for what still has to be validated on the board.
#include <cmath>
#include <cstdio>
#include <vector>

#include "chipo_drive_motion.h"

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
struct Vec { float v[3]; };
Vec Cross(const Vec& a, const Vec& b) {
    return {{a.v[1] * b.v[2] - a.v[2] * b.v[1], a.v[2] * b.v[0] - a.v[0] * b.v[2], a.v[0] * b.v[1] - a.v[1] * b.v[0]}};
}
Vec Rot(const Vec& x, const Vec& axis, float deg) {  // Rodrigues, axis unit
    const float c = std::cos(deg * 0.0174533f), s = std::sin(deg * 0.0174533f);
    const Vec k = Cross(axis, x);
    const float d = axis.v[0] * x.v[0] + axis.v[1] * x.v[1] + axis.v[2] * x.v[2];
    Vec r;
    for (int i = 0; i < 3; ++i) r.v[i] = x.v[i] * c + k.v[i] * s + axis.v[i] * d * (1 - c);
    return r;
}

struct Mount { Vec fwd, left, up; };
// up/fwd given in sensor axes; left = up x fwd keeps the car frame right-handed. Dash pitched by 12 deg.
Mount MakeMount(Vec up, Vec fwd, float pitch_deg = 12.0f) {
    Mount m;
    m.up = up; m.fwd = fwd; m.left = Cross(up, fwd);
    m.fwd = Rot(m.fwd, m.left, pitch_deg);
    m.up = Rot(m.up, m.left, pitch_deg);
    return m;
}

struct Rig {
    DriveMotion motion;
    Mount mount;
    uint32_t now = 10000;
    uint32_t rng = 12345;
    bool gyro = true;
    int accel = 0, brake = 0, curve = 0, bump = 0, abrupt = 0;
    float max_abs_lean = 0, last_lean = 0;
    int curve_side = 0;

    explicit Rig(const Mount& m, bool mirror = false) : motion(mirror), mount(m) {}
    float Noise(float amp) {
        rng = rng * 1664525u + 1013904223u;
        return (static_cast<float>((rng >> 8) & 0xFFFF) / 32768.0f - 1.0f) * amp;
    }
    // Car-frame inputs: fwd/left specific force (g), extra vertical (g), yaw rate about up (dps).
    void Step(float f, float l, float u, float yaw, float pitch_rate = 0, float roll_rate = 0, uint32_t dt = 20) {
        now += dt;
        ImuSample s;
        s.has_gyro = gyro;
        float a[3], w[3];
        for (int i = 0; i < 3; ++i) {
            a[i] = mount.fwd.v[i] * f + mount.left.v[i] * l + mount.up.v[i] * (1.0f + u) + Noise(0.012f);
            w[i] = mount.up.v[i] * yaw + mount.left.v[i] * pitch_rate + mount.fwd.v[i] * roll_rate + Noise(0.6f);
        }
        s.x = a[0]; s.y = a[1]; s.z = a[2];
        s.gx = w[0]; s.gy = w[1]; s.gz = w[2];
        const DriveOutput& o = motion.Update(s, now);
        accel += o.accelerate; brake += o.brake; curve += o.curve_start; bump += o.bump; abrupt += o.abrupt;
        if (o.curve_start) curve_side = o.curve_side;
        max_abs_lean = std::fmax(max_abs_lean, std::fabs(o.lean));
        last_lean = o.lean;
    }
    void Hold(uint32_t ms, float f = 0, float l = 0, float yaw = 0) {
        for (uint32_t t = 0; t < ms; t += 20) Step(f, l, 0, yaw);
    }
    void Clear() { accel = brake = curve = bump = abrupt = 0; max_abs_lean = 0; curve_side = 0; }
    // A steady turn: speed v (m/s), lateral acceleration `lat_g` toward the left (negative: right).
    void Turn(uint32_t ms, float v, float lat_g) {
        const float yaw_dps = (lat_g * 9.81f / v) * 57.29578f;
        Hold(ms, 0, lat_g, yaw_dps);
    }
};

const Mount kMounts[] = {
    MakeMount({{0, 1, 0}}, {{0, 0, 1}}),    // upright, +Z forward
    MakeMount({{0, 1, 0}}, {{0, 0, -1}}),   // upright, +Z backward
    MakeMount({{0, -1, 0}}, {{0, 0, 1}}),   // board mounted upside down
    MakeMount({{1, 0, 0}}, {{0, 0, 1}}),    // board turned a quarter: vertical axis is X
    MakeMount({{-1, 0, 0}}, {{0, 0, -1}}),
};
}  // namespace

int main() {
    const int mount_count = sizeof(kMounts) / sizeof(kMounts[0]);
    for (int mi = 0; mi < mount_count; ++mi) {
        std::printf("-- mounting %d\n", mi);
        const Mount& m = kMounts[mi];

        // ---- standing still: nothing happens, mount captured ----------------------------------
        {
            Rig r(m);
            r.Hold(500);
            
            r.Hold(3000);
            CHECK(r.motion.output().mounted, "mount captured after standing still (%d)", mi);
            CHECK(r.accel + r.brake + r.curve + r.bump + r.abrupt == 0, "no events at rest (%d)", mi);
        }

        // ---- start from rest teaches "forward"; then accelerate / brake are told apart --------
        {
            Rig r(m);
            r.Hold(3000);
            r.Hold(2000, 0.22f);  // pulling away
            CHECK(r.motion.output().forward_known, "forward learned from the start (%d)", mi);
            CHECK(r.accel == 1 && r.brake == 0, "start counts as one acceleration (acc=%d brake=%d) (%d)", r.accel, r.brake, mi);
            r.Hold(5000, 0);       // cruising
            r.Clear();
            r.Hold(1800, -0.28f);  // braking
            CHECK(r.brake == 1 && r.accel == 0, "braking recognised (brake=%d acc=%d) (%d)", r.brake, r.accel, mi);
            r.Hold(3000, 0);
            r.Clear();
            r.Hold(1500, 0.2f);
            CHECK(r.accel == 1, "second acceleration (%d) (%d)", r.accel, mi);
        }

        // ---- curves: both sides, leaning opposite ways, once per curve --------------------------
        {
            Rig r(m);
            r.Hold(3000);
            r.Hold(2000, 0.2f);
            r.Hold(4000, 0);
            r.Clear();
            r.Turn(4000, 12.0f, 0.22f);   // left curve at ~12 m/s
            CHECK(r.curve == 1, "left curve detected once (%d) (%d)", r.curve, mi);
            CHECK(r.max_abs_lean > 0.5f, "face leans in the curve (%.2f) (%d)", r.max_abs_lean, mi);
            const int left_side = r.curve_side;
            r.Hold(3000, 0);
            CHECK(std::fabs(r.last_lean) < 0.1f, "lean returns to centre after the curve (%.2f)", r.last_lean);
            r.Clear();
            r.Turn(4000, 12.0f, -0.22f);  // right curve
            CHECK(r.curve == 1, "right curve detected once (%d)", r.curve);
            CHECK(r.curve_side == -left_side && left_side != 0, "opposite curve pushes the face the other way (%d vs %d)", left_side, r.curve_side);
            r.Hold(1000, 0);
            CHECK(r.accel == 0 && r.brake == 0, "a curve does not look like acceleration or braking (%d,%d) (%d)", r.accel, r.brake, mi);
        }

        // ---- a long curve is not absorbed as gravity ----------------------------------------------
        {
            Rig r(m);
            r.Hold(3000);
            r.Hold(2000, 0.2f);
            r.Hold(3000, 0);
            r.Clear();
            r.Turn(12000, 10.0f, 0.2f);   // roundabout
            r.Hold(3000, 0);
            CHECK(r.curve == 1, "one long curve is one curve (%d)", r.curve);
            CHECK(r.accel == 0 && r.brake == 0, "straight after a long curve: no phantom braking (%d,%d) (%d)", r.accel, r.brake, mi);
        }

        // ---- picking CHIPO up and turning it by hand must never look like a curve ------------------
        {
            Rig r(m);
            r.Hold(3000);
            r.Hold(2000, 0.2f);
            r.Hold(4000, 0);
            r.Clear();
            // (a) spin it on the dashboard like a turntable: strong yaw, almost no lateral g
            for (int i = 0; i < 100; ++i) r.Step(0, 0.01f, 0, 90.0f);
            r.Hold(1000);
            // (b) tilt it forward/back about the screen's horizontal by hand, 90 degrees and back
            for (int i = 0; i < 25; ++i) {
                const float ang = 90.0f * i / 24.0f;
                Vec up = Rot(m.up, m.left, ang), fwd = Rot(m.fwd, m.left, ang);
                (void)fwd;
                r.now += 20;
                ImuSample s; s.has_gyro = true;
                s.x = up.v[0]; s.y = up.v[1]; s.z = up.v[2];
                const float rate = 90.0f / (24 * 0.02f);
                s.gx = m.left.v[0] * rate; s.gy = m.left.v[1] * rate; s.gz = m.left.v[2] * rate;
                const DriveOutput& o = r.motion.Update(s, r.now);
                r.curve += o.curve_start; r.accel += o.accelerate; r.brake += o.brake;
            }
            r.Hold(1000);
            // (c) tilt it sideways about the forward axis (leaning the board over)
            for (int i = 0; i < 25; ++i) {
                const float ang = 60.0f * i / 24.0f;
                Vec up = Rot(m.up, m.fwd, ang);
                r.now += 20;
                ImuSample s; s.has_gyro = true;
                s.x = up.v[0]; s.y = up.v[1]; s.z = up.v[2];
                const float rate = 60.0f / (24 * 0.02f);
                s.gx = m.fwd.v[0] * rate; s.gy = m.fwd.v[1] * rate; s.gz = m.fwd.v[2] * rate;
                const DriveOutput& o = r.motion.Update(s, r.now);
                r.curve += o.curve_start; r.accel += o.accelerate; r.brake += o.brake;
            }
            r.Hold(1000);
            // (d) carried while walking and turning the body: yaw 60 dps, a little sway
            for (int i = 0; i < 150; ++i) r.Step(0.02f * std::sin(i * 0.3f), 0.05f * std::sin(i * 0.2f), 0, 60.0f, 15.0f * std::sin(i * 0.4f));
            CHECK(r.curve == 0, "no curve from hand rotation (%d) (%d)", r.curve, mi);
            CHECK(r.accel == 0 && r.brake == 0, "no acceleration or braking from hand rotation (%d,%d) (%d)", r.accel, r.brake, mi);
        }

        // ---- bumps -----------------------------------------------------------------------------------
        {
            Rig r(m);
            r.Hold(3000);
            r.Hold(2000, 0.2f);
            r.Hold(3000, 0);
            r.Clear();
            for (int i = 0; i < 5; ++i) r.Step(0, 0, 0.55f, 0);  // 100 ms jolt
            for (int i = 0; i < 3; ++i) r.Step(0, 0, -0.35f, 0);
            r.Hold(1500);
            CHECK(r.bump == 1, "a bump is one bump (%d) (%d)", r.bump, mi);
            CHECK(r.accel == 0 && r.brake == 0 && r.curve == 0, "a bump is not acceleration/braking/curve (%d,%d,%d) (%d)", r.accel, r.brake, r.curve, mi);
            r.Clear();
            // road vibration: must stay silent
            for (int i = 0; i < 3000; ++i) r.Step(r.Noise(0.05f), r.Noise(0.05f), r.Noise(0.12f), r.Noise(2.0f), r.Noise(3.0f), r.Noise(3.0f));
            CHECK(r.accel + r.brake + r.curve + r.bump + r.abrupt == 0, "normal vibration is silent (%d %d %d %d %d) (%d)", r.accel, r.brake, r.curve, r.bump, r.abrupt, mi);
        }

        // ---- abrupt movement ---------------------------------------------------------------------------
        {
            Rig r(m);
            r.Hold(3000);
            r.Hold(2000, 0.2f);
            r.Hold(3000, 0);
            r.Clear();
            for (int i = 0; i < 15; ++i) r.Step(0, 0.75f, 0, 0);  // sudden sideways shove
            r.Hold(1000);
            CHECK(r.abrupt == 1, "abrupt movement detected once (%d) (%d)", r.abrupt, mi);
        }

        // ---- a slow slope does not move the mount reference into events ------------------------------
        {
            Rig r(m);
            r.Hold(3000);
            r.Hold(2000, 0.2f);
            r.Clear();
            for (int i = 0; i < 600; ++i) {
                const float pitch = 8.0f * i / 599.0f;  // 8 degrees over 12 s
                const float f = std::sin(pitch * 0.0174533f);
                const float u = std::cos(pitch * 0.0174533f) - 1.0f;
                r.Step(f, 0, u, 0, 0.7f);
            }
            r.Hold(2000, 0.13f * 0);
            CHECK(r.accel + r.brake + r.curve + r.bump + r.abrupt == 0, "slow slope is silent (%d %d %d %d %d) (%d)", r.accel, r.brake, r.curve, r.bump, r.abrupt, mi);
        }
    }

    // ---- sensitivity ------------------------------------------------------------------------------------
    for (int level = 0; level < 3; ++level) {
        Rig r(kMounts[0]);
        r.motion.SetSensitivity(static_cast<DriveSensitivity>(level));
        r.Hold(3000);
        r.Hold(2000, 0.30f);  // teach forward with a clear start
        r.Hold(3000, 0);
        r.Clear();
        r.Hold(1500, 0.165f);
        r.Hold(1000, 0);
        std::printf("   sensitivity %d: accel events for a gentle 0.165 g push = %d\n", level, r.accel);
        if (level == 0) CHECK(r.accel == 0, "SUAVE ignores a gentle push (%d)", r.accel);
        if (level == 1) CHECK(r.accel == 1, "NORMAL reacts to a gentle push (%d)", r.accel);
        if (level == 2) CHECK(r.accel == 1, "INTENSA reacts to a gentle push (%d)", r.accel);
    }

    // ---- no gyroscope: acceleration/braking still work, curves are off ---------------------------------------
    {
        Rig r(kMounts[0]);
        r.gyro = false;
        r.Hold(3000);
        r.Hold(2000, 0.22f);
        r.Hold(3000, 0);
        r.Clear();
        r.Turn(4000, 12.0f, 0.22f);
        CHECK(r.curve == 0, "no curve without gyroscope data (%d)", r.curve);
        r.Hold(1500, -0.3f);
        CHECK(r.brake == 1, "braking still works without gyroscope (%d)", r.brake);
    }

    // ---- fixed forward sign ---------------------------------------------------------------------------------------
    {
        Rig r(kMounts[1]);  // +Z backward
        r.motion.SetForwardSign(-1);
        r.Hold(3000);
        r.Hold(3000, 0.0f);
        r.Hold(1500, -0.3f);  // braking right away, no start needed
        CHECK(r.brake == 1 && r.accel == 0, "fixed sign: braking known from the first second (%d,%d)", r.brake, r.accel);
    }

    // ---- remounting: picked up, put down somewhere else, a new reference is taken ------------------------------------
    {
        Rig r(kMounts[0]);
        r.Hold(3000);
        Rig r2(kMounts[3]);  // same motion object cannot change mount; emulate by feeding other pose
        r2.Hold(3000);
        CHECK(r2.motion.output().mounted, "a different upright mounting is captured too");
        // lying flat on a table (gravity along the screen normal) is not a valid mount
        Rig r3(MakeMount({{0, 0, 1}}, {{0, 1, 0}}, 0.0f));
        r3.Hold(4000);
        CHECK(!r3.motion.output().mounted, "lying flat is not a dashboard mount");
        r3.Hold(1500, 0.3f);
        CHECK(r3.accel + r3.brake + r3.curve == 0, "no driving events while not mounted");
    }

    // ---- bad input ------------------------------------------------------------------------------------------------------------
    {
        DriveMotion mo;
        ImuSample s;
        s.has_gyro = true;
        s.x = s.y = s.z = 0;  // zero vector (free fall)
        mo.Update(s, 100);
        s.x = 1e9f; s.y = -1e9f; s.z = 1e9f; s.gx = 1e9f; s.gy = 1e9f; s.gz = -1e9f;
        mo.Update(s, 140);
        s.x = NAN;
        mo.Update(s, 180);
        std::printf("   bad input survived\n");
    }

    if (g_failures == 0) std::printf("ALL DRIVE MOTION TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
