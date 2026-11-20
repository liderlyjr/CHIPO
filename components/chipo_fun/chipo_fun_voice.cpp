#include "chipo_fun_voice.h"

#include <string>
#include <vector>

#include "chipo_drive_voice.h"  // DriveNormalizeText

namespace {
using Tokens = std::vector<std::string>;

Tokens Split(const std::string& s) {
    Tokens out;
    std::string cur;
    for (char c : s) {
        if (c == ' ') { if (!cur.empty()) out.push_back(cur), cur.clear(); }
        else cur.push_back(c);
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}
bool In(const std::string& w, std::initializer_list<const char*> list) {
    for (const char* x : list) if (w == x) return true;
    return false;
}
bool Has(const Tokens& t, std::initializer_list<const char*> list) {
    for (const auto& w : t) if (In(w, list)) return true;
    return false;
}
}  // namespace

FunCommand ParseFunCommand(const char* text) {
    FunCommand cmd;
    Tokens t = Split(DriveNormalizeText(text));
    Tokens core;
    for (const auto& w : t) {
        if (In(w, {"chipo", "chippo", "shipo", "chipu", "chepo", "cipo", "por", "favor", "porfa", "oye", "venga", "ya", "pues", "hey", "ey"})) continue;
        core.push_back(w);
    }
    if (core.empty() || core.size() > 8) return cmd;

    const bool off_verb = In(core[0], {"apaga", "quita", "desactiva", "cierra"}) || (core.size() >= 2 && core[0] == "fuera");
    const bool lamp_word = Has(core, {"lampara", "luz", "luces", "arcoiris"});

    // ---- lamp off -----------------------------------------------------------------------------
    if (off_verb && Has(core, {"lampara", "luz", "luces", "arcoiris"})) {
        // "apaga la luz del salón" belongs to Home Assistant: only the bare forms are ours.
        if (Has(core, {"del", "de"}) && !Has(core, {"colores"})) return cmd;
        cmd.intent = FunIntent::LampOff;
        return cmd;
    }
    if (core.size() == 2 && core[0] == "fuera" && core[1] == "luz") {
        cmd.intent = FunIntent::LampOff;
        return cmd;
    }

    // ---- eye colour ("ojos rosas", "pon los ojos verdes", "ojos normales") ------------------------
    const bool eye_cmd = core.size() <= 2 || In(core[0], {"pon", "ponte", "ponme", "pones", "cambia", "cambiate", "quiero", "usa", "con", "tus", "los", "vuelve", "vuelvan", "deja"});
    if (eye_cmd && Has(core, {"ojos", "ojo"})) {
        struct Named { const char* word; unsigned rgb; };
        static const Named kColors[] = {
            {"rosa", 0xFF4FA8}, {"rosas", 0xFF4FA8}, {"rosado", 0xFF4FA8}, {"rojo", 0xFF3B30}, {"rojos", 0xFF3B30},
            {"verde", 0x39FF88}, {"verdes", 0x39FF88}, {"naranja", 0xFF9A3C}, {"naranjas", 0xFF9A3C},
            {"morado", 0xB66BFF}, {"morados", 0xB66BFF}, {"violeta", 0xB66BFF}, {"lila", 0xB66BFF},
            {"amarillo", 0xFFE14D}, {"amarillos", 0xFFE14D}, {"turquesa", 0x2EF2E0}, {"cian", 0x2EF2E0},
            {"blanco", 0xF2F6FF}, {"blancos", 0xF2F6FF}};
        for (const auto& w : core)
            for (const auto& c : kColors)
                if (w == c.word) { cmd.intent = FunIntent::EyeColor; cmd.rgb = c.rgb; return cmd; }
        if (Has(core, {"normales", "normal", "azules", "azul", "originales", "siempre", "defecto"})) {
            cmd.intent = FunIntent::EyeReset;
            return cmd;
        }
    }

    // ---- lamp on ------------------------------------------------------------------------------
    if (lamp_word) {
        const bool on_verb = In(core[0], {"enciende", "prende", "activa", "pon", "ponme", "modo", "quiero", "luz", "luces", "lampara", "arcoiris"});
        if (Has(core, {"arcoiris"}) || (Has(core, {"colores"}) && Has(core, {"luz", "luces", "lampara"}))) {
            cmd.intent = FunIntent::Lamp; cmd.color = LampColor::Rainbow; return cmd;
        }
        if (Has(core, {"lampara"}) && on_verb) {
            cmd.intent = FunIntent::Lamp;
            cmd.color = Has(core, {"azul", "fria", "frio"}) ? LampColor::Cool : Has(core, {"verde"}) ? LampColor::Green
                      : Has(core, {"rosa", "morada", "morado"}) ? LampColor::Pink : LampColor::Warm;
            return cmd;
        }
        if (Has(core, {"luz"}) && on_verb && !Has(core, {"del", "de", "en"})) {
            if (Has(core, {"calida", "calido", "naranja", "ambiente"})) { cmd.intent = FunIntent::Lamp; cmd.color = LampColor::Warm; return cmd; }
            if (Has(core, {"azul", "fria", "frio"})) { cmd.intent = FunIntent::Lamp; cmd.color = LampColor::Cool; return cmd; }
            if (Has(core, {"verde"})) { cmd.intent = FunIntent::Lamp; cmd.color = LampColor::Green; return cmd; }
            if (Has(core, {"rosa", "morada", "morado"})) { cmd.intent = FunIntent::Lamp; cmd.color = LampColor::Pink; return cmd; }
        }
    }

    // ---- dice and coin ------------------------------------------------------------------------
    const bool roll_verb = Has(core, {"tira", "lanza", "echa", "tirame", "lanzame", "juega", "tirar", "lanzar", "saca", "dame"});
    if (Has(core, {"dado", "dados"}) && (roll_verb || core.size() <= 3)) {
        cmd.intent = FunIntent::Dice;
        cmd.dice = Has(core, {"dos", "2", "dados"}) ? 2 : 1;
        return cmd;
    }
    if (Has(core, {"moneda"}) && roll_verb) { cmd.intent = FunIntent::Coin; return cmd; }
    if (core.size() == 3 && core[0] == "cara" && core[1] == "o" && core[2] == "cruz") { cmd.intent = FunIntent::Coin; return cmd; }
    if (core.size() <= 4 && Has(core, {"cara"}) && Has(core, {"cruz"}) && Has(core, {"o"})) { cmd.intent = FunIntent::Coin; return cmd; }
    return cmd;
}
