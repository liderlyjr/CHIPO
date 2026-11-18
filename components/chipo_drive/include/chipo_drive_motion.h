#pragma once

#include <cstdint>

#include "chipo_drive_types.h"
#include "chipo_tilt.h"  // ImuSample

/**
 * Turns accelerometer + gyroscope samples of a CHIPO standing on a car dashboard into driving
 * events: acceleration, braking, curves, bumps and abrupt movements. No ESP-IDF, no LVGL: it is
 * unit tested on a PC (tests/host/test_drive_motion.cpp).
 *
 * What it measures is the movement of CHIPO, not of the car. It never reports a speed and never
 * detects accidents.
 *
 * How the two sensors work together
 *  - Accelerometer: gravity reference (captured while CHIPO stands still on its mount), then the
 *    "linear" acceleration = reading - reference, split into vertical (along gravity), forward and
 *    lateral components.
 *  - Gyroscope: (1) while the device is turning or being shaken the gravity reference is NOT
 *    updated, so a long curve does not get absorbed as "gravity"; (2) a curve needs a sustained
 *    rotation about the gravity axis (yaw) that goes together with lateral acceleration
 *    (centripetal), while picking CHIPO up or twisting it by hand rotates about other axes or
 *    gives no lateral acceleration; (3) it separates "device moved" from "road moved".
 *
 * Mounting assumption: upright on the dashboard with the screen towards the driver. Then the screen
 * horizontal is the car's lateral axis and the screen normal is the car's longitudinal axis. Which
 * way along the normal is "forward" is learned from the first push after standing still.
 */
struct DriveOutput {
    bool mounted = false;       // gravity reference known and gravity still close to it
    bool forward_known = false; // acceleration / braking can be told apart
    bool in_curve = false;
    float lean = 0;             // -1..1, face push (positive: to the screen's right), only in curves
    float yaw_dps = 0;          // debug: rotation about gravity (smoothed)
    float lateral_g = 0;        // debug: lateral acceleration (smoothed)
    float forward_g = 0;        // debug: forward acceleration (smoothed, + = accelerating)
    float vertical_g = 0;       // debug: last vertical deviation
    // One-step events (true for the update in which they happen).
    bool accelerate = false;
    bool brake = false;
    bool curve_start = false;
    bool bump = false;
    bool abrupt = false;
    float intensity = 0;        // 0..1 for the event of this update
    int curve_side = 0;         // -1 / +1: which way the face pushes at the start of the curve
};

class DriveMotion {
public:
    explicit DriveMotion(bool mirror_x = false) : mirror_x_(mirror_x) {}

    void SetSensitivity(DriveSensitivity s) { sens_ = s; }
    DriveSensitivity sensitivity() const { return sens_; }
    // +1 / -1: the sensor +Z / -Z axis points forward; 0: learn it.
    void SetForwardSign(int sign);
    // New session: forget the mounting (keeps the sensitivity).
    void Reset();

    const DriveOutput& Update(const ImuSample& sample, uint32_t now_ms);
    const DriveOutput& output() const { return out_; }

private:
    void ComputeAxes();
    void Capture(const float a[3]);

    bool mirror_x_;
    DriveSensitivity sens_ = DriveSensitivity::Normal;
    DriveOutput out_;

    uint32_t last_ms_ = 0;
    bool have_last_ = false;

    float a_f_[3] = {0, 0, 0};     // fast low-pass (about 100 ms): vibration removed
    float a_slow_[3] = {0, 0, 0};  // slow low-pass (about 300 ms): for "is it still"
    bool have_filters_ = false;
    uint32_t still_ms_ = 0;

    // Mount reference.
    bool have_ref_ = false;
    float ref_[3] = {0, 0, 0};
    float g_hat_[3] = {0, 0, 0};   // unit vector of the reference (up)
    float lat_[3] = {0, 0, 0};     // unit lateral axis (screen horizontal)
    float fwd0_[3] = {0, 0, 0};    // unit longitudinal axis (screen normal), sign not applied
    int h_axis_ = 0;
    uint32_t off_mount_ms_ = 0;

    // Forward direction.
    int forced_sign_ = 0;
    int fwd_votes_ = 0;
    bool start_armed_ = false;
    uint32_t start_push_ms_ = 0;
    uint32_t start_window_ms_ = 0;

    // Smoothed signals.
    float yaw_f_ = 0, tilt_rate_f_ = 0, tilt_slow_ = 0, lat_f_ = 0, fwd_f_ = 0, hmag_f_ = 0;
    int bump_hits_ = 0;
    float lean_ = 0;

    // Event state machines.
    uint32_t accel_ms_ = 0, brake_ms_ = 0, curve_ms_ = 0, curve_off_ms_ = 0;
    bool accelerating_ = false, braking_ = false;
    uint32_t accel_block_until_ = 0, brake_block_until_ = 0, curve_block_until_ = 0;
    uint32_t bump_block_until_ = 0, abrupt_block_until_ = 0, quiet_long_until_ = 0;
    int curve_relation_votes_ = 0;  // sign of yaw * lateral accel seen in accepted curves
};
