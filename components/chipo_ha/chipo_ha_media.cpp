#include "chipo_ha_media.h"

#include <vector>

#include "chipo_drive_voice.h"  // DriveNormalizeText
#include "chipo_ha_logic.h"

bool HaValidMediaPlayer(const std::string& id) {
    return id.rfind("media_player.", 0) == 0 && HaValidEntityId(id);
}

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

HaMediaCommand ParseHaMediaCommand(const char* text) {
    HaMediaCommand cmd;
    Tokens core;
    for (const auto& w : Split(DriveNormalizeText(text)))
        if (!In(w, {"chipo", "chippo", "shipo", "chipu", "chepo", "cipo", "por", "favor", "porfa", "oye", "venga", "ya", "pues", "hey", "ey"})) core.push_back(w);
    if (core.empty() || core.size() > 8) return cmd;
    if (!Has(core, {"musica", "cancion", "tema", "reproductor", "playlist"})) return cmd;
    if (Has(core, {"escena", "luz", "luces", "alarma", "temporizador"})) return cmd;

    const bool next = Has(core, {"siguiente", "proxima", "proximo", "salta", "saltar", "pasa", "cambia"});
    const bool prev = Has(core, {"anterior", "previa", "previo", "atras", "vuelve"});
    const bool up = Has(core, {"sube", "subir", "aumenta", "mas"}) && Has(core, {"volumen", "alto", "fuerte"});
    const bool down = Has(core, {"baja", "bajar", "reduce", "menos"}) && Has(core, {"volumen", "bajo", "flojo"});
    const bool pause = Has(core, {"pausa", "pausar", "para", "parar", "deten", "quita", "apaga", "silencia", "calla"});
    const bool play = Has(core, {"pon", "ponme", "reproduce", "reproducir", "play", "continua", "sigue", "reanuda", "empieza", "enciende", "activa", "dale", "quiero", "suena"});

    if (up) { cmd.action = HaMediaAction::VolumeUp; cmd.service = "volume_up"; cmd.ok_text = "Música: más volumen"; return cmd; }
    if (down) { cmd.action = HaMediaAction::VolumeDown; cmd.service = "volume_down"; cmd.ok_text = "Música: menos volumen"; return cmd; }
    if (next && !prev) { cmd.action = HaMediaAction::Next; cmd.service = "media_next_track"; cmd.ok_text = "Siguiente canción"; return cmd; }
    if (prev) { cmd.action = HaMediaAction::Previous; cmd.service = "media_previous_track"; cmd.ok_text = "Canción anterior"; return cmd; }
    if (pause && !play) { cmd.action = HaMediaAction::Pause; cmd.service = "media_pause"; cmd.ok_text = "Música en pausa"; return cmd; }
    if (play) { cmd.action = HaMediaAction::Play; cmd.service = "media_play"; cmd.ok_text = "Música"; return cmd; }
    return cmd;
}
