#pragma once

#include <cstdint>

/**
 * Pure maths for "is CHIPO leaning / falling / lying down?". No ESP-IDF, no LVGL: it is unit
 * tested on a PC (tests/host).
 *
 * Input: accelerometer samples in g. At rest the accelerometer reads +1 g pointing UP.
 *
 * The sensor axes are not documented relative to the screen, so the first time CHIPO stands
 * still and roughly upright (1.2 s) that pose becomes "home": the in-plane axis carrying
 * gravity is the screen's vertical, the other in-plane axis is horizontal, the third is the
 * screen normal. Everything afterwards is measured relative to that pose.
 */
struct ImuSample {
    float x = 0, y = 0, z = 0;     // accelerometer, g
    float gx = 0, gy = 0, gz = 0;  // gyroscope, degrees per second (only meaningful if has_gyro)
    bool has_gyro = false;
    uint32_t t_ms = 0;             // when the sensor task read it (0: unknown, use the caller's clock)
};

struct TiltOutput {
    bool calibrated = false;
    float angle_deg = 0;   // angle between the current "up" and the calibrated one
    float push_x = 0;      // -1..1, positive: gravity pulls towards the screen's right side
    float lean_z = 0;      // 0..1, tilt towards or away from the viewer (sign dropped on purpose)
    bool startle = false;  // true for one update when a sudden jerk or free fall is detected
    bool lying = false;    // tilted far over for a while (lying down / upside down)
};

class ChipoTilt {
public:
    explicit ChipoTilt(bool mirror_x = false) : mirror_x_(mirror_x) {}

    void Reset();
    const TiltOutput& Update(const ImuSample& sample, uint32_t now_ms);
    const TiltOutput& output() const { return out_; }
    int vertical_axis() const { return v_axis_; }  // 0 = sensor X, 1 = sensor Y

private:
    bool mirror_x_;
    TiltOutput out_;

    bool have_filtered_ = false;
    float f_[3] = {0, 0, 0};    // low-passed sample
    float u0_[3] = {0, 0, 0};   // calibrated "up" (unit vector)
    int v_axis_ = 1;
    uint32_t last_ms_ = 0;
    bool have_last_ = false;
    uint32_t stable_ms_ = 0;
    uint32_t over_ms_ = 0;
    uint32_t startle_block_until_ = 0;
};
