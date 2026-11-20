#pragma once

#include <string>

/**
 * Spoken commands of CHIPO DRIVE, matched on the text the server's speech recognition returns
 * (XiaoZhi "stt" message). There is no second recogniser: the wake word and the recognition are
 * the existing ones, so the car mode needs the same Internet connection as any conversation.
 *
 * Pure text logic (no ESP-IDF): unit tested on a PC.
 */
enum class DriveIntent {
    None,
    Activate,           // "Chipo, vámonos"
    Deactivate,         // "desactiva el modo conducción", "vuelve al modo escritorio", "sal del modo coche"
    SensitivitySoft,    // "sensibilidad suave"
    SensitivityNormal,  // "sensibilidad normal"
    SensitivityIntense  // "sensibilidad intensa"
};

// Lower case, accents removed (ñ -> n), everything that is not a letter or digit becomes a space.
std::string DriveNormalizeText(const char* text);

DriveIntent ParseDriveIntent(const char* text);
