#include "chipo_ambient_logic.h"

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

const char* AmbientName(AmbientSound s) {
    switch (s) {
        case AmbientSound::Rain: return "rain";
        case AmbientSound::Fire: return "fire";
        case AmbientSound::Noise: return "noise";
    }
    return "rain";
}

const char* AmbientLabel(AmbientSound s) {
    switch (s) {
        case AmbientSound::Rain: return "Lluvia";
        case AmbientSound::Fire: return "Fuego";
        case AmbientSound::Noise: return "Ruido suave";
    }
    return "";
}

AmbientCommand ParseAmbientCommand(const char* text, bool active) {
    AmbientCommand cmd;
    Tokens core;
    for (const auto& w : Split(DriveNormalizeText(text)))
        if (!In(w, {"chipo", "chippo", "shipo", "chipu", "chepo", "cipo", "por", "favor", "porfa", "oye", "venga", "ya", "pues", "hey", "ey"})) core.push_back(w);
    if (core.empty() || core.size() > 8) return cmd;
    if (Has(core, {"escena", "luz", "luces", "lampara", "temporizador", "alarma", "recuerdame", "musica"})) return cmd;  // other modules

    const bool rain = Has(core, {"lluvia"});
    const bool fire = Has(core, {"fuego", "chimenea", "hoguera"});
    const bool noise = Has(core, {"ruido"}) && Has(core, {"blanco", "rosa", "marron", "fondo", "suave"});
    const bool any_sound = Has(core, {"sonido", "sonidos", "ambiente", "ruido"});

    // ---- stop ----------------------------------------------------------------------------------
    const bool stop_verb = In(core[0], {"para", "parar", "quita", "quitar", "apaga", "apagar", "silencia", "cierra", "deten", "cancela", "termina"}) ||
                           (core.size() >= 3 && core[0] == "deja" && core[1] == "de");
    if (stop_verb) {
        if (active && (rain || fire || noise || any_sound)) { cmd.intent = AmbientIntent::Stop; return cmd; }
        return cmd;
    }

    // ---- start ---------------------------------------------------------------------------------
    const bool start_verb = In(core[0], {"pon", "ponme", "poner", "quiero", "enciende", "prende", "activa", "reproduce", "dame", "sonido", "sonidos", "ruido", "modo", "escuchar", "ambiente", "lluvia", "chimenea", "fuego", "hoguera"});
    if (!start_verb) return cmd;
    if (rain && !Has(core, {"va", "voy", "hoy", "manana", "llover"})) { cmd.intent = AmbientIntent::Start; cmd.sound = AmbientSound::Rain; return cmd; }
    if (fire && !Has(core, {"cocina", "gas", "bomberos"})) { cmd.intent = AmbientIntent::Start; cmd.sound = AmbientSound::Fire; return cmd; }
    if (noise) { cmd.intent = AmbientIntent::Start; cmd.sound = AmbientSound::Noise; return cmd; }
    return cmd;
}
