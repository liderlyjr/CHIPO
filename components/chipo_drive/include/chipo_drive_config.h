#pragma once

/**
 * CHIPO DRIVE tunables. Every value can be overridden with a compiler definition.
 * Thresholds are for the NORMAL sensitivity; SUAVE multiplies them by ~1.35, INTENSA by ~0.7.
 */

// Master switch. 0 builds the face and everything else without the car mode.
#ifndef CHIPO_ENABLE_DRIVE
#define CHIPO_ENABLE_DRIVE 1
#endif

// Which way is "forward" along the screen normal? 0 = learn it (the first push after standing
// still decides, so start the mode while parked), +1 / -1 = fixed (sensor +Z / -Z points forward).
#ifndef CHIPO_DRIVE_FORWARD_SIGN
#define CHIPO_DRIVE_FORWARD_SIGN 0
#endif

// Mirror the lean direction (same meaning as CHIPO_TILT_MIRROR_X).
#ifndef CHIPO_DRIVE_MIRROR_X
#define CHIPO_DRIVE_MIRROR_X 0
#endif

// (The IMU sampling period for the car mode, CHIPO_DRIVE_SAMPLE_MS, lives in chipo_config.h.)

// Log one line per second with the motion values (to record a real trip and tune the thresholds).
#ifndef CHIPO_DRIVE_DEBUG_LOG
#define CHIPO_DRIVE_DEBUG_LOG 1
#endif
