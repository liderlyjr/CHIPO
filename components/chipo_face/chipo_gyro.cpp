#include "chipo_gyro.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kSmoothTauMs = 60.0f;

// Spin: a steady, fast rotation around the screen normal.
constexpr float kSpinStartDps = 200.0f;
constexpr uint32_t kSpinStartMs = 250;
constexpr float kSpinStopDps = 90.0f;
constexpr uint32_t kSpinStopMs = 350;
constexpr float kSpinDominance = 0.7f;  // share of the total rotation that must be around the normal

// Nod / shake: quick back and forth rotation around one in-plane axis.
constexpr float kSwingMinDps = 90.0f;
constexpr uint32_t kSwingGapMs = 600;  // a swing sequence breaks if nothing strong happens for this long
constexpr int kSwingReversals = 2;     // out, back, out again
constexpr float kSwingDominance = 1.5f;
constexpr uint32_t kGestureCooldownMs = 1500;
}  // namespace

bool ChipoGyro::Swing::Feed(float rate_dps, uint32_t now_ms) {
    if (last_sign != 0 && now_ms - last_ms > kSwingGapMs) Reset();
    const float strength = std::fabs(rate_dps);
    if (strength < kSwingMinDps) return false;
    peak = std::max(peak, strength);
    const int sign = rate_dps > 0 ? 1 : -1;
    const bool first = last_sign == 0;
    const bool flipped = !first && sign != last_sign;
    last_sign = sign;
    last_ms = now_ms;
    if (flipped) ++reversals;
    return reversals >= kSwingReversals;
}

void ChipoGyro::Reset() {
    *this = ChipoGyro();
}

const GyroOutput& ChipoGyro::Update(const ImuSample& s, bool calibrated, int vertical_axis,
                                    uint32_t now_ms) {
    out_.nod = false;
    out_.shake = false;
    out_.spin_ended = false;
    if (!s.has_gyro) return out_;
    out_.available = true;

    uint32_t dt = have_last_ ? now_ms - last_ms_ : 40;
    if (dt < 1) dt = 1;
    if (dt > 200) dt = 200;
    last_ms_ = now_ms;
    have_last_ = true;

    const float rate[3] = {s.gx, s.gy, s.gz};
    const float mag = std::sqrt(rate[0] * rate[0] + rate[1] * rate[1] + rate[2] * rate[2]);
    const float a = 1.0f - std::exp(-static_cast<float>(dt) / kSmoothTauMs);
    smooth_rate_ += (mag - smooth_rate_) * a;
    out_.rate_dps = smooth_rate_;

    // ---- spin ------------------------------------------------------------------------------
    const float around_normal = std::fabs(rate[2]);
    smooth_spin_ += (around_normal - smooth_spin_) * a;
    const bool normal_dominates = around_normal >= kSpinDominance * mag;
    if (!out_.spinning) {
        if (normal_dominates && smooth_spin_ >= kSpinStartDps) spin_fast_ms_ += dt;
        else spin_fast_ms_ = 0;
        if (spin_fast_ms_ >= kSpinStartMs) {
            out_.spinning = true;
            out_.spin_ms = spin_fast_ms_;
            spin_slow_ms_ = 0;
            pitch_.Reset();
            yaw_.Reset();
        }
    } else {
        out_.spin_ms += dt;
        spin_slow_ms_ = smooth_spin_ < kSpinStopDps ? spin_slow_ms_ + dt : 0;
        if (spin_slow_ms_ >= kSpinStopMs) {
            out_.spinning = false;
            out_.spin_ended = true;
            spin_fast_ms_ = 0;
            spin_slow_ms_ = 0;
        }
    }

    // ---- nod / shake -----------------------------------------------------------------------
    if (out_.spinning || !calibrated || (vertical_axis != 0 && vertical_axis != 1)) {
        pitch_.Reset();
        yaw_.Reset();
        return out_;
    }
    const bool nodded = pitch_.Feed(rate[1 - vertical_axis], now_ms);
    const bool shook = yaw_.Feed(rate[vertical_axis], now_ms);
    if (now_ms < block_until_ms_) {
        if (nodded) pitch_.Reset();
        if (shook) yaw_.Reset();
        return out_;
    }
    if (nodded && pitch_.peak >= kSwingDominance * yaw_.peak) {
        out_.nod = true;
    } else if (shook && yaw_.peak >= kSwingDominance * pitch_.peak) {
        out_.shake = true;
    } else {
        // Both axes moved about equally (a wild shake): not a clean gesture.
        if (nodded) pitch_.Reset();
        if (shook) yaw_.Reset();
    }
    if (out_.nod || out_.shake) {
        block_until_ms_ = now_ms + kGestureCooldownMs;
        pitch_.Reset();
        yaw_.Reset();
    }
    return out_;
}
