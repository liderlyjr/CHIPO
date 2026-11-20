#pragma once

/**
 * Spoken commands of CHIPO FUN (colour lamp, dice, coin). Pure text logic, no ESP-IDF: unit tested on a PC.
 */
enum class LampColor { Warm, Cool, Green, Pink, Rainbow };

enum class FunIntent {
    None,
    Lamp,      // "luz cálida", "lámpara", "luz azul", "arcoíris"   -> color
    LampOff,   // "apaga la lámpara", "apaga la luz" (only when the lamp is on)
    Dice,      // "tira un dado", "tira dos dados"                  -> dice (1 or 2)
    Coin,      // "cara o cruz", "tira una moneda"
    EyeColor,  // "ojos rosas", "pon los ojos verdes"               -> rgb
    EyeReset   // "ojos normales", "ojos azules", "ojos de siempre"
};

struct FunCommand {
    FunIntent intent = FunIntent::None;
    LampColor color = LampColor::Warm;
    int dice = 1;
    unsigned rgb = 0;  // EyeColor
};

FunCommand ParseFunCommand(const char* text);
