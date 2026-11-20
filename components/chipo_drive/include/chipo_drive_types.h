#pragma once

#include <cstdint>

enum class DriveSensitivity : uint8_t { Soft = 0, Normal = 1, Intense = 2 };

// Threshold multiplier: higher means harder to trigger.
inline float DriveThresholdScale(DriveSensitivity s) {
    return s == DriveSensitivity::Soft ? 1.35f : s == DriveSensitivity::Intense ? 0.7f : 1.0f;
}
// Chance multiplier for spoken comments.
inline float DriveSpeechScale(DriveSensitivity s) {
    return s == DriveSensitivity::Soft ? 0.6f : s == DriveSensitivity::Intense ? 1.4f : 1.0f;
}
// Minimum silence between two spoken comments.
inline uint32_t DriveSpeechGapMs(DriveSensitivity s) {
    return s == DriveSensitivity::Soft ? 45000u : s == DriveSensitivity::Intense ? 16000u : 28000u;
}

enum class DriveEventKind : uint8_t { Accelerate, Brake, Curve, Bump, Abrupt, Calm };
