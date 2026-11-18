#include "chipo_drive_motion.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
constexpr float kRadToDeg = 57.29578f;
constexpr float kDegToRad = 0.017453293f;

// Thresholds for the NORMAL sensitivity (see DriveThresholdScale). Units: g, dps, ms.
constexpr float kStillDevG = 0.04f;
constexpr float kStillGyroDps = 6.0f;
constexpr uint32_t kCaptureAfterStillMs = 1000;
constexpr uint32_t kRecaptureAfterStillMs = 1500;
constexpr float kUprightShare = 0.60f;       // share of gravity that must lie in the screen plane
constexpr float kOffMountDeg = 35.0f;        // gravity this far from the reference: not on its mount
constexpr uint32_t kOffMountMs = 1200;
constexpr float kDisturbedTiltDps = 18.0f;   // slow average of non-yaw rotation: held in a hand

constexpr float kAccelOnG = 0.14f, kAccelOffG = 0.08f;
constexpr uint32_t kAccelMs = 300;
constexpr float kBrakeOnG = 0.18f, kBrakeOffG = 0.10f;
constexpr uint32_t kBrakeMs = 250;
constexpr uint32_t kLongCooldownMs = 1500;

constexpr float kCurveYawDps = 8.0f;
constexpr float kCurveLatG = 0.09f;
constexpr uint32_t kCurveEnterMs = 350;
constexpr uint32_t kCurveExitMs = 300;
constexpr uint32_t kCurveCooldownMs = 1500;
constexpr float kCurveLeanFullG = 0.30f;
constexpr float kMinPlausibleMps = 2.5f;     // consistency of lateral g with yaw rate, never reported
constexpr float kMaxPlausibleMps = 55.0f;

constexpr float kBumpG = 0.30f;
constexpr uint32_t kBumpCooldownMs = 900;
constexpr uint32_t kBumpQuietMs = 450;

constexpr float kAbruptG = 0.55f;
constexpr float kAbruptYawDps = 45.0f;
constexpr float kAbruptYawLatG = 0.18f;
constexpr uint32_t kAbruptCooldownMs = 3000;

constexpr float kStartPushG = 0.10f;
constexpr uint32_t kStartPushMs = 350;
constexpr uint32_t kStartWindowMs = 4000;

float Dot(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
float Norm(const float a[3]) { return std::sqrt(Dot(a, a)); }
void Lowpass(float& state, float input, float dt_ms, float tau_ms) {
    state += (input - state) * (1.0f - std::exp(-dt_ms / tau_ms));
}
float Clamp(float v, float lo, float hi) { return std::min(hi, std::max(lo, v)); }
int Sign(float v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }
}  // namespace

void DriveMotion::SetForwardSign(int sign) {
    forced_sign_ = sign > 0 ? 1 : (sign < 0 ? -1 : 0);
}

void DriveMotion::Reset() {
    const DriveSensitivity s = sens_;
    const bool m = mirror_x_;
    const int forced = forced_sign_;
    *this = DriveMotion(m);
    sens_ = s;
    forced_sign_ = forced;
}

void DriveMotion::ComputeAxes() {
    const float gn = Norm(ref_);
    for (int i = 0; i < 3; ++i) g_hat_[i] = ref_[i] / gn;
    float eh[3] = {0, 0, 0};
    eh[h_axis_] = 1.0f;
    const float d = Dot(eh, g_hat_);
    for (int i = 0; i < 3; ++i) lat_[i] = eh[i] - d * g_hat_[i];
    const float ln = Norm(lat_);
    for (int i = 0; i < 3; ++i) lat_[i] /= ln;
    float ez[3] = {0, 0, 1};
    const float dz = Dot(ez, g_hat_);
    float z[3];
    for (int i = 0; i < 3; ++i) z[i] = ez[i] - dz * g_hat_[i];
    const float dl = Dot(z, lat_);
    for (int i = 0; i < 3; ++i) fwd0_[i] = z[i] - dl * lat_[i];
    const float fn = Norm(fwd0_);
    for (int i = 0; i < 3; ++i) fwd0_[i] /= fn;
}

void DriveMotion::Capture(const float a[3]) {
    for (int i = 0; i < 3; ++i) ref_[i] = a[i];
    h_axis_ = std::fabs(a[0]) > std::fabs(a[1]) ? 1 : 0;  // the vertical in-plane axis is X or Y
    ComputeAxes();
    have_ref_ = true;
    off_mount_ms_ = 0;
}

const DriveOutput& DriveMotion::Update(const ImuSample& s, uint32_t now_ms) {
    out_.accelerate = out_.brake = out_.curve_start = out_.bump = out_.abrupt = false;
    out_.intensity = 0;
    out_.curve_side = 0;

    float dt = have_last_ ? static_cast<float>(now_ms - last_ms_) : 20.0f;
    dt = Clamp(dt, 1.0f, 100.0f);
    last_ms_ = now_ms;
    have_last_ = true;

    const float a[3] = {s.x, s.y, s.z};
    float w[3] = {0, 0, 0};
    if (s.has_gyro) { w[0] = s.gx; w[1] = s.gy; w[2] = s.gz; }
    const float mag = Norm(a);
    if (!(mag > 0.05f)) return out_;  // free fall / bad data: nothing to say

    if (!have_filters_) {
        for (int i = 0; i < 3; ++i) a_f_[i] = a_slow_[i] = a[i];
        have_filters_ = true;
    } else {
        for (int i = 0; i < 3; ++i) {
            Lowpass(a_f_[i], a[i], dt, 100.0f);
            Lowpass(a_slow_[i], a[i], dt, 300.0f);
        }
    }
    const float scale = DriveThresholdScale(sens_);
    const float wmag = Norm(w);
    float dev_v[3] = {a[0] - a_slow_[0], a[1] - a_slow_[1], a[2] - a_slow_[2]};
    const bool still = Norm(dev_v) < kStillDevG && wmag < kStillGyroDps && mag > 0.85f && mag < 1.15f;
    still_ms_ = still ? still_ms_ + static_cast<uint32_t>(dt) : 0;
    const float slow_mag = Norm(a_slow_);
    const bool upright = slow_mag > 0.5f &&
                         std::max(std::fabs(a_slow_[0]), std::fabs(a_slow_[1])) / slow_mag > kUprightShare;

    // ---- gravity reference (the mount) ------------------------------------------------------
    if (!have_ref_) {
        if (still_ms_ >= kCaptureAfterStillMs && upright) Capture(a_slow_);
        return out_;
    }
    float cosang = Dot(a_slow_, g_hat_) / slow_mag;
    const float angle = std::acos(Clamp(cosang, -1.0f, 1.0f)) * kRadToDeg;
    if (angle > kOffMountDeg) off_mount_ms_ += static_cast<uint32_t>(dt);
    else off_mount_ms_ = 0;
    if (off_mount_ms_ >= kOffMountMs && still_ms_ >= kRecaptureAfterStillMs && upright) {
        // Put down somewhere else (or the mount changed): start over with the new pose.
        Capture(a_slow_);
        fwd_votes_ = 0;
        curve_relation_votes_ = 0;
        accelerating_ = braking_ = out_.in_curve = false;
        accel_ms_ = brake_ms_ = curve_ms_ = curve_off_ms_ = 0;
        lean_ = 0;
    }

    // ---- signals in the car frame -----------------------------------------------------------
    float lin_f[3], lin_raw[3];
    for (int i = 0; i < 3; ++i) {
        lin_f[i] = a_f_[i] - ref_[i];
        lin_raw[i] = a[i] - ref_[i];
    }
    const float lat_now = Dot(lin_f, lat_);
    const float fwd_now = Dot(lin_f, fwd0_);          // along +fwd0_, sign not applied yet
    const float vert_raw = Dot(lin_raw, g_hat_);
    const float yaw_now = Dot(w, g_hat_);
    float w_tilt[3];
    for (int i = 0; i < 3; ++i) w_tilt[i] = w[i] - yaw_now * g_hat_[i];
    const float tilt_now = Norm(w_tilt);
    const float hmag_now = std::sqrt(lat_now * lat_now + fwd_now * fwd_now);

    Lowpass(yaw_f_, yaw_now, dt, 120.0f);
    Lowpass(tilt_rate_f_, tilt_now, dt, 120.0f);
    Lowpass(tilt_slow_, tilt_now, dt, 800.0f);
    Lowpass(lat_f_, lat_now, dt, 100.0f);
    Lowpass(fwd_f_, fwd_now, dt, 100.0f);
    Lowpass(hmag_f_, hmag_now, dt, 60.0f);

    const int sign = forced_sign_ != 0 ? forced_sign_ : Sign(static_cast<float>(fwd_votes_));
    const float forward = static_cast<float>(sign) * fwd_f_;  // + = accelerating
    const float right = mirror_x_ ? -1.0f : 1.0f;
    const float push = -lat_f_ * right;

    const bool mounted = off_mount_ms_ < kOffMountMs;
    const bool disturbed = !mounted || tilt_slow_ > kDisturbedTiltDps;
    out_.mounted = mounted;
    out_.forward_known = sign != 0;
    out_.yaw_dps = yaw_f_;
    out_.lateral_g = lat_f_;
    out_.forward_g = forward;
    out_.vertical_g = vert_raw;

    // ---- slow gravity tracking, frozen whenever the gyroscope says the device is moving ----
    if (!disturbed && !out_.in_curve && !accelerating_ && !braking_ && wmag < 12.0f && mag > 0.96f &&
        mag < 1.04f) {
        for (int i = 0; i < 3; ++i) Lowpass(ref_[i], a_slow_[i], dt, 10000.0f);
        ComputeAxes();
    }

    if (disturbed) {
        // In a hand, being re-mounted, or off its mount: nothing it feels is the car.
        accel_ms_ = brake_ms_ = curve_ms_ = curve_off_ms_ = 0;
        accelerating_ = braking_ = false;
        out_.in_curve = false;
        start_armed_ = false;
        start_push_ms_ = 0;
        bump_hits_ = 0;
        Lowpass(lean_, 0.0f, dt, 150.0f);
        out_.lean = lean_;
        return out_;
    }

    // ---- bumps: a short vertical jolt that the horizontal plane does not explain -------------
    if (std::fabs(vert_raw) >= kBumpG * scale && std::fabs(vert_raw) > 1.3f * std::sqrt(hmag_now * hmag_now)) {
        ++bump_hits_;
    } else {
        bump_hits_ = 0;
    }
    if (bump_hits_ >= 2 && now_ms >= bump_block_until_) {
        out_.bump = true;
        out_.intensity = Clamp(std::fabs(vert_raw) / 0.9f, 0.2f, 1.0f);
        bump_block_until_ = now_ms + kBumpCooldownMs;
        quiet_long_until_ = now_ms + kBumpQuietMs;
        bump_hits_ = 0;
    }
    const bool quiet_long = now_ms < quiet_long_until_;
    if (quiet_long) {
        accel_ms_ = brake_ms_ = 0;
        start_push_ms_ = 0;
    }

    // ---- learn which way is forward: the first push after standing still is a start ---------
    if (std::abs(fwd_votes_) >= 2) {
        start_armed_ = false;
    } else if (still_ms_ >= 1500) {
        start_armed_ = true;
        start_window_ms_ = 0;
        start_push_ms_ = 0;
    } else if (start_armed_) {
        start_window_ms_ += static_cast<uint32_t>(dt);
        const bool clean = std::fabs(yaw_f_) < 10.0f && std::fabs(vert_raw) < 0.25f && !quiet_long;
        if (clean && std::fabs(fwd_f_) > kStartPushG) start_push_ms_ += static_cast<uint32_t>(dt);
        else start_push_ms_ = 0;
        if (start_push_ms_ >= kStartPushMs && forced_sign_ == 0) {
            // The first push after the mode starts (CHIPO was told "vámonos" while parked) is a start
            // from rest: it fixes "forward" for the rest of the session. Standing still is also what
            // smooth cruising looks like, so later pushes must not be able to change it.
            fwd_votes_ = 3 * Sign(fwd_f_);
            start_armed_ = false;
            start_push_ms_ = 0;
            const int now_sign = Sign(static_cast<float>(fwd_votes_));
            if (now_sign != 0 && Sign(fwd_f_) == now_sign && std::fabs(fwd_f_) > kAccelOnG * scale && !accelerating_ &&
                now_ms >= accel_block_until_) {
                out_.accelerate = true;
                out_.intensity = Clamp(std::fabs(fwd_f_) / 0.35f, 0.2f, 1.0f);
                accelerating_ = true;
                accel_block_until_ = now_ms + kLongCooldownMs;
            }
            out_.forward_known = now_sign != 0;
        } else if (start_window_ms_ > kStartWindowMs) {
            start_armed_ = false;
        }
    }

    // ---- acceleration and braking (need the forward direction) ------------------------------
    if (sign != 0 && !quiet_long) {
        if (forward > kAccelOnG * scale) accel_ms_ += static_cast<uint32_t>(dt);
        else if (forward < kAccelOffG * scale) accel_ms_ = 0;
        if (accelerating_ && forward < kAccelOffG * scale) {
            accelerating_ = false;
            accel_block_until_ = std::max(accel_block_until_, now_ms + kLongCooldownMs);
        }
        if (!accelerating_ && accel_ms_ >= kAccelMs && now_ms >= accel_block_until_ && !out_.accelerate) {
            out_.accelerate = true;
            out_.intensity = Clamp(forward / 0.35f, 0.2f, 1.0f);
            accelerating_ = true;
        }

        if (forward < -kBrakeOnG * scale) brake_ms_ += static_cast<uint32_t>(dt);
        else if (forward > -kBrakeOffG * scale) brake_ms_ = 0;
        if (braking_ && forward > -kBrakeOffG * scale) {
            braking_ = false;
            brake_block_until_ = std::max(brake_block_until_, now_ms + kLongCooldownMs);
        }
        if (!braking_ && brake_ms_ >= kBrakeMs && now_ms >= brake_block_until_) {
            out_.brake = true;
            out_.intensity = std::max(out_.intensity, Clamp(-forward / 0.5f, 0.2f, 1.0f));
            braking_ = true;
        }
    }

    // ---- curves: sustained yaw about gravity + coherent lateral acceleration ---------------
    const float yaw_abs = std::fabs(yaw_f_);
    const float lat_abs = std::fabs(lat_f_);
    bool plausible = false;
    if (yaw_abs > 1.0f) {
        const float ratio = lat_abs * 9.81f / (yaw_abs * kDegToRad);  // m/s, only a consistency check
        plausible = ratio >= kMinPlausibleMps && ratio <= kMaxPlausibleMps;
    }
    const int relation = Sign(yaw_f_ * lat_f_);
    const bool relation_ok = std::abs(curve_relation_votes_) < 2 || relation == Sign(static_cast<float>(curve_relation_votes_));
    const bool mostly_yaw = tilt_rate_f_ < 0.7f * yaw_abs + 4.0f;
    const bool curve_cond = s.has_gyro && yaw_abs >= kCurveYawDps * scale && lat_abs >= kCurveLatG * scale &&
                            plausible && relation_ok && mostly_yaw;
    if (!out_.in_curve) {
        if (curve_cond) curve_ms_ += static_cast<uint32_t>(dt);
        else curve_ms_ = 0;
        if (curve_ms_ >= kCurveEnterMs && now_ms >= curve_block_until_) {
            out_.in_curve = true;
            out_.curve_start = true;
            out_.curve_side = push >= 0 ? 1 : -1;
            out_.intensity = std::max(out_.intensity, Clamp(lat_abs / 0.4f, 0.2f, 1.0f));
            curve_relation_votes_ = static_cast<int>(Clamp(static_cast<float>(curve_relation_votes_ + relation), -4.0f, 4.0f));
            curve_off_ms_ = 0;
        }
    } else {
        const bool leaving = yaw_abs < 0.5f * kCurveYawDps * scale || lat_abs < 0.5f * kCurveLatG * scale;
        curve_off_ms_ = leaving ? curve_off_ms_ + static_cast<uint32_t>(dt) : 0;
        if (curve_off_ms_ >= kCurveExitMs) {
            out_.in_curve = false;
            curve_ms_ = 0;
            curve_block_until_ = now_ms + kCurveCooldownMs;
        }
    }
    Lowpass(lean_, out_.in_curve ? Clamp(push / kCurveLeanFullG, -1.0f, 1.0f) : 0.0f, dt, 150.0f);
    out_.lean = lean_;

    // ---- abrupt movements -------------------------------------------------------------------
    const bool jerk = hmag_f_ >= kAbruptG * scale;
    const bool swerve = s.has_gyro && yaw_abs >= kAbruptYawDps * scale && lat_abs >= kAbruptYawLatG;
    if ((jerk || swerve) && now_ms >= abrupt_block_until_) {
        out_.abrupt = true;
        out_.intensity = std::max(out_.intensity, Clamp(std::max(hmag_f_, lat_abs) / 0.9f, 0.3f, 1.0f));
        abrupt_block_until_ = now_ms + kAbruptCooldownMs;
    }
    return out_;
}
