#pragma once

/**
 * CHIPO tunables. Every value can be overridden with a compiler definition
 * (e.g. -DCHIPO_TILT_MIRROR_X=1) without touching this file.
 */

// Use the on-board QMI8658 accelerometer so the face reacts when CHIPO is tilted or dropped.
#ifndef CHIPO_ENABLE_IMU
#define CHIPO_ENABLE_IMU 1
#endif

// Also read the QMI8658 gyroscope (spin, nod "yes", shake "no"). If it does not answer, CHIPO keeps
// working with the accelerometer only.
#ifndef CHIPO_ENABLE_GYRO
#define CHIPO_ENABLE_GYRO 1
#endif

// Now and then, while idle, CHIPO winks or smiles by itself. Can also be switched at run time
// with ChipoFace::SetIdleQuirks().
#ifndef CHIPO_IDLE_QUIRKS
#define CHIPO_IDLE_QUIRKS 1
#endif

// During this time before falling asleep CHIPO looks sleepy and yawns. 0 disables it.
#ifndef CHIPO_TIRED_BEFORE_SLEEP_MS
#define CHIPO_TIRED_BEFORE_SLEEP_MS 20000
#endif

// The IMU sits on the board's main I2C bus, already opened by the XiaoZhi board code
// (Waveshare ESP32-S3 Touch AMOLED 2.16: I2C port 0, SDA 15, SCL 14 - XiaoZhi's board code and Waveshare's
// docs. The QMI8658 address is not documented for this board: 0x6B is tried first, then 0x6A - the two
// addresses the chip can have).
#ifndef CHIPO_IMU_I2C_PORT
#define CHIPO_IMU_I2C_PORT 0
#endif
#ifndef CHIPO_IMU_ADDR_PRIMARY
#define CHIPO_IMU_ADDR_PRIMARY 0x6B
#endif
#ifndef CHIPO_IMU_ADDR_FALLBACK
#define CHIPO_IMU_ADDR_FALLBACK 0x6A
#endif

// Waveshare does not document how the sensor axes line up with the screen. The code finds
// the vertical axis by itself when CHIPO stands still. Only left/right can be mirrored:
// if CHIPO "falls" towards the wrong side, set this to 1.
#ifndef CHIPO_TILT_MIRROR_X
#define CHIPO_TILT_MIRROR_X 0
#endif

// Tilt (degrees from the calibrated upright pose) at which CHIPO panics and slips off the screen
// towards the side it is leaning to. Below it the face only slides a little.
#ifndef CHIPO_FALL_ANGLE_DEG
#define CHIPO_FALL_ANGLE_DEG 45
#endif

// Desk clock: CHIPO lying on its side for this long turns into a big clock (see BUDDY). The side is the one
// that ends up DOWN, seen from the front: -1 = left (USB-C edge up when the port is on the right), +1 = right,
// 0 = off. If the real board turns out mirrored, flip the sign here (or CHIPO_TILT_MIRROR_X).
#ifndef CHIPO_DESK_CLOCK_SIDE
#define CHIPO_DESK_CLOCK_SIDE -1
#endif
#ifndef CHIPO_DESK_CLOCK_AFTER_MS
#define CHIPO_DESK_CLOCK_AFTER_MS 10000
#endif
// "Uy, me mareé": how long the dizzy face lasts when CHIPO is stood up again after being a clock.
#ifndef CHIPO_DESK_CLOCK_DIZZY_MS
#define CHIPO_DESK_CLOCK_DIZZY_MS 2800
#endif

// Idle (no voice, no touch) for this long and CHIPO falls asleep. 0 disables sleeping.
#ifndef CHIPO_SLEEP_AFTER_MS
#define CHIPO_SLEEP_AFTER_MS 120000
#endif

// Give up on "Thinking" if the server never answers.
#ifndef CHIPO_THINKING_TIMEOUT_MS
#define CHIPO_THINKING_TIMEOUT_MS 25000
#endif

// Log the tilt reading once per second (useful to check axis direction on the real board).
#ifndef CHIPO_DEBUG_TILT_LOG
#define CHIPO_DEBUG_TILT_LOG 1
#endif

// Sampling period of the IMU while the car mode (CHIPO DRIVE) is on; 40 ms (25 Hz) otherwise.
#ifndef CHIPO_DRIVE_SAMPLE_MS
#define CHIPO_DRIVE_SAMPLE_MS 20
#endif
