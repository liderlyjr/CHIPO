#include "chipo_tilt.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kRadToDeg = 57.29578f;
constexpr float kFilterTauMs = 80.0f;
constexpr float kStillThresholdG = 0.06f;    // |raw - filtered| below this counts as "still"
constexpr uint32_t kCalibrateAfterMs = 1200; // still time needed to define the home pose
constexpr float kUprightInPlane = 0.80f;     // share of gravity that must lie in the screen plane
constexpr uint32_t kRehomeAfterMs = 10000;   // resting in a new, slightly tilted pose
constexpr float kRehomeMinDeg = 3.0f;
constexpr float kRehomeMaxDeg = 30.0f;
constexpr float kLyingEnterDeg = 75.0f;
constexpr float kLyingExitDeg = 50.0f;
constexpr uint32_t kLyingAfterMs = 1500;
constexpr float kJerkUnitChange = 0.45f;     // sudden change of direction between samples
constexpr float kShakeMagnitudeG = 0.6f;     // |a| differs from 1 g by this much: shake / impact
constexpr float kFreeFallG = 0.35f;          // |a| below this: the whole device is falling
constexpr uint32_t kStartleCooldownMs = 1200;

float Norm(const float v[3]) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
}  // namespace

void ChipoTilt::Reset() {
    *this = ChipoTilt(mirror_x_);
}

const TiltOutput& ChipoTilt::Update(const ImuSample& s, uint32_t now_ms) {
    out_.startle = false;

    uint32_t dt = have_last_ ? now_ms - last_ms_ : 40;
    if (dt < 1) dt = 1;
    if (dt > 200) dt = 200;
    last_ms_ = now_ms;
    have_last_ = true;

    const float raw[3] = {s.x, s.y, s.z};
    const float mag = Norm(raw);

    if (!have_filtered_) {
        std::copy(raw, raw + 3, f_);
        have_filtered_ = true;
    } else {
        const float a = 1.0f - std::exp(-static_cast<float>(dt) / kFilterTauMs);
        for (int i = 0; i < 3; ++i) f_[i] += (raw[i] - f_[i]) * a;
    }

    const float diff[3] = {raw[0] - f_[0], raw[1] - f_[1], raw[2] - f_[2]};
    const bool still = Norm(diff) < kStillThresholdG;
    stable_ms_ = still ? stable_ms_ + dt : 0;

    // Sudden events work even before calibration: a real fall is a fall.
    if (now_ms >= startle_block_until_) {
        bool startle = mag < kFreeFallG || std::fabs(mag - 1.0f) > kShakeMagnitudeG;
        const float fm = Norm(f_);
        if (!startle && out_.calibrated && fm > 0.2f && mag > 0.05f) {
            float change = 0;
            for (int i = 0; i < 3; ++i) {
                const float d = raw[i] / mag - f_[i] / fm;
                change += d * d;
            }
            startle = std::sqrt(change) > kJerkUnitChange;
        }
        if (startle) {
            out_.startle = true;
            startle_block_until_ = now_ms + kStartleCooldownMs;
        }
    }

    const float fm = Norm(f_);
    if (fm < 0.2f) return out_;  // free fall: direction is meaningless

    if (!out_.calibrated) {
        if (stable_ms_ >= kCalibrateAfterMs && fm > 0.8f && fm < 1.2f) {
            const float in_plane = std::max(std::fabs(f_[0]), std::fabs(f_[1])) / fm;
            if (in_plane > kUprightInPlane) {
                for (int i = 0; i < 3; ++i) u0_[i] = f_[i] / fm;
                v_axis_ = std::fabs(f_[0]) > std::fabs(f_[1]) ? 0 : 1;
                out_.calibrated = true;
                stable_ms_ = 0;
            }
        }
        return out_;
    }

    float u[3] = {f_[0] / fm, f_[1] / fm, f_[2] / fm};
    float cosine = u[0] * u0_[0] + u[1] * u0_[1] + u[2] * u0_[2];
    cosine = std::clamp(cosine, -1.0f, 1.0f);
    out_.angle_deg = std::acos(cosine) * kRadToDeg;

    const int h_axis = 1 - v_axis_;
    const float right = mirror_x_ ? -1.0f : 1.0f;
    out_.push_x = std::clamp(-(u[h_axis] - u0_[h_axis]) * right, -1.0f, 1.0f);
    out_.lean_z = std::min(1.0f, std::fabs(u[2] - u0_[2]));

    if (out_.angle_deg > kLyingEnterDeg) {
        over_ms_ += dt;
        if (over_ms_ >= kLyingAfterMs) out_.lying = true;
    } else if (out_.angle_deg < kLyingExitDeg) {
        over_ms_ = 0;
        out_.lying = false;
    }

    // If CHIPO is left resting in a slightly different pose, that becomes the new normal.
    if (!out_.lying && stable_ms_ >= kRehomeAfterMs && out_.angle_deg > kRehomeMinDeg &&
        out_.angle_deg < kRehomeMaxDeg) {
        for (int i = 0; i < 3; ++i) u0_[i] = u[i];
        stable_ms_ = 0;
        out_.angle_deg = 0;
        out_.push_x = 0;
        out_.lean_z = 0;
    }
    return out_;
}
