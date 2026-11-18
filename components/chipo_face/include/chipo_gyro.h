#pragma once

#include <cstdint>

#include "chipo_tilt.h"

/**
 * Pure maths for gestures made with the gyroscope. No ESP-IDF, no LVGL: unit tested on a PC.
 *
 * Only things that do not depend on which way the sensor axes point are used, because Waveshare
 * does not document that (see chipo_tilt.h):
 *  - spin: CHIPO turns like a steering wheel (rotation around the screen normal, sensor Z).
 *  - nod: back and forth rotation around the screen's horizontal axis ("yes").
 *  - shake: back and forth rotation around the screen's vertical axis ("no").
 * Nod and shake need to know which in-plane sensor axis is vertical, which ChipoTilt works out
 * when CHIPO first stands still and upright, so they only work after that.
 * The direction of a rotation is never used, only its size and how often it reverses.
 */
struct GyroOutput {
    bool available = false;  // at least one sample with gyroscope data was seen
    float rate_dps = 0;      // smoothed size of the rotation, any axis
    bool spinning = false;   // true while the spin lasts
    uint32_t spin_ms = 0;    // how long the current (or last) spin has lasted
    bool spin_ended = false; // true for one update when a spin stops; spin_ms holds its length
    bool nod = false;        // true for one update when a "yes" gesture is recognised
    bool shake = false;      // true for one update when a "no" gesture is recognised
};

class ChipoGyro {
public:
    void Reset();
    // vertical_axis: 0 = sensor X, 1 = sensor Y; only used when `calibrated` is true.
    const GyroOutput& Update(const ImuSample& sample, bool calibrated, int vertical_axis,
                             uint32_t now_ms);
    const GyroOutput& output() const { return out_; }

private:
    // Counts quick reversals of a rotation around one axis.
    struct Swing {
        int last_sign = 0;
        uint32_t last_ms = 0;
        int reversals = 0;
        float peak = 0;
        void Reset() { *this = Swing(); }
        // Returns true when enough reversals have happened.
        bool Feed(float rate_dps, uint32_t now_ms);
    };

    GyroOutput out_;
    float smooth_rate_ = 0;
    float smooth_spin_ = 0;
    bool have_last_ = false;
    uint32_t last_ms_ = 0;
    uint32_t spin_fast_ms_ = 0;  // time above the spin threshold
    uint32_t spin_slow_ms_ = 0;  // time below the "stopped" threshold
    uint32_t block_until_ms_ = 0;
    Swing pitch_;  // around the horizontal in-plane axis (nod)
    Swing yaw_;    // around the vertical in-plane axis (shake)
};
