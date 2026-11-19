#pragma once

/**
 * Spoken commands of CHIPO AMBIENT (rain, fire, soft noise). Pure text logic, no ESP-IDF: unit tested on a PC.
 */
enum class AmbientSound { Rain, Fire, Noise };

enum class AmbientIntent { None, Start, Stop };

struct AmbientCommand {
    AmbientIntent intent = AmbientIntent::None;
    AmbientSound sound = AmbientSound::Rain;
};

// "pon sonido de lluvia", "ponme la chimenea", "ruido blanco" -> Start;
// "para la lluvia", "apaga el sonido", "quita el ruido" -> Stop (only when `active`, otherwise "apaga el fuego" would be
// a sentence about the kitchen).
AmbientCommand ParseAmbientCommand(const char* text, bool active);

const char* AmbientName(AmbientSound sound);      // "rain" | "fire" | "noise": clip id is ambient_<name>_<piece>
const char* AmbientLabel(AmbientSound sound);     // "Lluvia", "Fuego", "Ruido suave"
