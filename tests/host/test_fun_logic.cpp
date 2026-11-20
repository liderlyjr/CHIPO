#include <cstdio>

#include "chipo_fun_voice.h"

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void Lamp(const char* t, LampColor c) {
    const FunCommand r = ParseFunCommand(t);
    CHECK(r.intent == FunIntent::Lamp && r.color == c, "\"%s\" -> %d/%d", t, static_cast<int>(r.intent), static_cast<int>(r.color));
}
static void Is(const char* t, FunIntent i, int dice = 1) {
    const FunCommand r = ParseFunCommand(t);
    CHECK(r.intent == i && (i != FunIntent::Dice || r.dice == dice), "\"%s\" -> %d (dice %d), want %d", t, static_cast<int>(r.intent), r.dice, static_cast<int>(i));
}

int main() {
    Lamp("enciende la lámpara", LampColor::Warm);
    Lamp("Chipo, modo lámpara", LampColor::Warm);
    Lamp("pon luz cálida", LampColor::Warm);
    Lamp("luz azul", LampColor::Cool);
    Lamp("enciende la luz verde", LampColor::Green);
    Lamp("pon la luz rosa", LampColor::Pink);
    Lamp("arcoíris", LampColor::Rainbow);
    Lamp("pon luces de colores", LampColor::Rainbow);
    Lamp("enciende la lámpara azul", LampColor::Cool);
    Is("apaga la lámpara", FunIntent::LampOff);
    Is("apaga la luz", FunIntent::LampOff);
    Is("fuera luz", FunIntent::LampOff);
    Is("apaga las luces de colores", FunIntent::LampOff);
    Is("tira un dado", FunIntent::Dice, 1);
    Is("Chipo, lanza dos dados", FunIntent::Dice, 2);
    Is("un dado", FunIntent::Dice, 1);
    Is("cara o cruz", FunIntent::Coin);
    Is("Chipo, tira una moneda", FunIntent::Coin);

    // Eye colour.
    {
        FunCommand c = ParseFunCommand("Chipo, pon los ojos rosas");
        if (c.intent != FunIntent::EyeColor || c.rgb != 0xFF4FA8) { std::printf("FAIL eyes pink\n"); ++g_fail; }
        c = ParseFunCommand("ojos verdes");
        if (c.intent != FunIntent::EyeColor || c.rgb != 0x39FF88) { std::printf("FAIL eyes green\n"); ++g_fail; }
        c = ParseFunCommand("cambia los ojos a morado");
        if (c.intent != FunIntent::EyeColor || c.rgb != 0xB66BFF) { std::printf("FAIL eyes purple\n"); ++g_fail; }
    }
    Is("ojos normales", FunIntent::EyeReset);
    Is("Chipo, vuelve a los ojos azules", FunIntent::EyeReset);
    Is("pon los ojos de siempre", FunIntent::EyeReset);
    Is("mi hermana tiene los ojos verdes y es muy guapa", FunIntent::None);
    Is("tengo ojos verdes", FunIntent::None);
    Is("me duelen los ojos", FunIntent::None);

    // Home Assistant's phrases and ordinary speech are not ours.
    Is("enciende la luz del salón", FunIntent::None);
    Is("apaga la luz del salón", FunIntent::None);
    Is("apaga la luz de la cocina", FunIntent::None);
    Is("enciende la luz de la cocina", FunIntent::None);
    Is("qué tiempo hace", FunIntent::None);
    Is("me han dado una moneda de oro en el mercado de la ciudad vieja", FunIntent::None);
    Is("la moneda", FunIntent::None);
    Is("dime qué es un dado de rol y para qué sirve en los juegos", FunIntent::None);
    Is("", FunIntent::None);
    Is(nullptr, FunIntent::None);
    if (g_fail == 0) std::printf("ALL FUN LOGIC TESTS PASSED\n");
    return g_fail == 0 ? 0 : 1;
}
