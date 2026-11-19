#include "chipo_pet_logic.h"

#include <algorithm>
#include <vector>

#include "chipo_drive_voice.h"  // DriveNormalizeText

// ---- mood ----------------------------------------------------------------------------------------------
void PetMood::Reset(uint32_t now_ms) {
    stored_ = kStart;
    last_attention_ms_ = now_ms;
    last_pout_ms_ = now_ms;
    pouted_ = false;
}

int PetMood::Value(uint32_t now_ms) const {
    const uint32_t idle = now_ms - last_attention_ms_;
    if (idle <= kGraceMs) return stored_;
    return std::max(0, stored_ - static_cast<int>((idle - kGraceMs) / kDecayStepMs));
}

void PetMood::Attention(uint32_t now_ms, int amount) {
    stored_ = std::min(100, Value(now_ms) + amount);
    last_attention_ms_ = now_ms;
    pouted_ = false;
}

PetLevel PetMood::LevelOf(int v) {
    if (v >= 70) return PetLevel::Happy;
    if (v >= 40) return PetLevel::Fine;
    if (v >= 15) return PetLevel::Lonely;
    return PetLevel::Neglected;
}

PetLevel PetMood::Level(uint32_t now_ms) const { return LevelOf(Value(now_ms)); }

bool PetMood::ShouldPout(uint32_t now_ms) {
    const PetLevel level = Level(now_ms);
    if (level != PetLevel::Lonely && level != PetLevel::Neglected) return false;
    if (pouted_ && now_ms - last_pout_ms_ < kPoutEveryMs) return false;
    pouted_ = true;
    last_pout_ms_ = now_ms;
    return true;
}

const char* PetMood::Describe(PetLevel level) {
    switch (level) {
        case PetLevel::Happy: return "¡Estoy muy feliz! Gracias por hacerme caso";
        case PetLevel::Fine: return "Estoy bien, aunque un poquito aburrido";
        case PetLevel::Lonely: return "Un poco solito... ¿jugamos?";
        case PetLevel::Neglected: return "Estaba triste... ¡te echaba de menos!";
    }
    return "";
}

// ---- voice ---------------------------------------------------------------------------------------------
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
int NumberOf(const std::string& w) {
    static const char* kWords[] = {"cero", "uno", "dos", "tres", "cuatro", "cinco", "seis", "siete", "ocho", "nueve", "diez"};
    for (int i = 0; i <= 10; ++i) if (w == kWords[i]) return i;
    if (w == "un") return 1;
    if (!w.empty() && w.size() <= 3 && std::all_of(w.begin(), w.end(), [](char c) { return c >= '0' && c <= '9'; })) return std::stoi(w);
    return -1;
}
}  // namespace

PetCommand ParsePetCommand(const char* text, bool game_active) {
    PetCommand cmd;
    Tokens core;
    for (const auto& w : Split(DriveNormalizeText(text)))
        if (!In(w, {"chipo", "chippo", "shipo", "chipu", "chepo", "cipo", "por", "favor", "porfa", "oye", "venga", "ya", "pues", "hey", "ey", "a", "pues"}))
            core.push_back(w);
    if (core.empty() || core.size() > 8) return cmd;

    // Leaving a game.
    if (game_active) {
        if ((Has(core, {"jugar", "juego"}) && Has(core, {"no", "dejar", "basta", "terminar", "acabar", "salir", "parar", "para"})) ||
            (core.size() <= 3 && Has(core, {"basta", "salir"})) || (core.size() <= 3 && core[0] == "terminar")) {
            cmd.intent = PetIntent::StopGame;
            return cmd;
        }
    }

    // Mood.
    const bool how = core.size() <= 5 && !Has(core, {"tiempo", "clima", "llamas", "dice", "ella", "el", "tu"}) &&
                     ((Has(core, {"como"}) && Has(core, {"estas", "andas", "sientes", "va", "encuentras"})) ||
                      (Has(core, {"tal"}) && Has(core, {"estas"})) ||
                      (Has(core, {"estas"}) && Has(core, {"bien", "triste", "contento", "feliz"})));
    if (how) { cmd.intent = PetIntent::HowAreYou; return cmd; }

    // Games.
    if (Has(core, {"piedra"}) && Has(core, {"papel"}) && core.size() <= 8) { cmd.intent = PetIntent::RpsStart; return cmd; }
    if (Has(core, {"adivina", "adivinar", "adivinanza"}) && Has(core, {"numero"})) { cmd.intent = PetIntent::GuessStart; return cmd; }
    if (Has(core, {"piensa", "pienso"}) && Has(core, {"numero"})) { cmd.intent = PetIntent::GuessStart; return cmd; }
    if (Has(core, {"juguemos", "jugamos", "jugar", "jugar"}) && Has(core, {"adivinar"})) { cmd.intent = PetIntent::GuessStart; return cmd; }

    if (game_active && core.size() <= 3) {
        for (const auto& w : core) {
            if (w == "piedra") { cmd.intent = PetIntent::RpsMove; cmd.value = 0; return cmd; }
            if (w == "papel") { cmd.intent = PetIntent::RpsMove; cmd.value = 1; return cmd; }
            if (w == "tijera" || w == "tijeras") { cmd.intent = PetIntent::RpsMove; cmd.value = 2; return cmd; }
        }
        for (const auto& w : core) {
            if (In(w, {"el", "es", "creo", "sera", "numero"})) continue;
            const int n = NumberOf(w);
            if (n >= 0) { cmd.intent = PetIntent::GuessNumber; cmd.value = (n >= 1 && n <= 10) ? n : 0; return cmd; }
        }
    }
    return cmd;
}

// ---- games ---------------------------------------------------------------------------------------------
PetGames::PetGames(std::function<int(int, int)> rand) : rand_(std::move(rand)) {}

bool PetGames::Tick(uint32_t now_ms) {
    if (game_ != Game::None && now_ms - last_ms_ > kIdleEndMs) {
        game_ = Game::None;
        return true;
    }
    return false;
}

GameReply PetGames::Handle(const PetCommand& cmd, uint32_t now_ms) {
    GameReply r;
    switch (cmd.intent) {
    case PetIntent::RpsStart:
        game_ = Game::Rps;
        score_user_ = score_chipo_ = 0;
        last_ms_ = now_ms;
        r.handled = true;
        r.text = "¡Piedra, papel o tijera! Di tu jugada";
        r.emotion = "winking";
        return r;
    case PetIntent::GuessStart:
        game_ = Game::Guess;
        secret_ = rand_(1, 10);
        tries_ = 0;
        last_ms_ = now_ms;
        r.handled = true;
        r.text = "Ya pensé un número del 1 al 10. ¿Cuál es?";
        r.emotion = "cool";
        return r;
    case PetIntent::StopGame:
        if (game_ == Game::None) return r;
        game_ = Game::None;
        r.handled = true;
        r.text = "Vale, lo dejamos. ¡Ha estado genial!";
        r.emotion = "happy";
        return r;
    case PetIntent::RpsMove: {
        if (game_ != Game::Rps) return r;
        static const char* kNames[] = {"piedra", "papel", "tijera"};
        const int mine = rand_(0, 2);
        const int diff = (cmd.value - mine + 3) % 3;  // 0 draw, 1 the owner wins, 2 CHIPO wins
        last_ms_ = now_ms;
        r.handled = true;
        r.text = std::string("Yo: ") + kNames[mine] + ". ";
        if (diff == 0) { r.text += "¡Empate!"; r.emotion = "winking"; }
        else if (diff == 1) { ++score_user_; r.text += "¡Ganas tú!"; r.emotion = "surprised"; }
        else { ++score_chipo_; r.text += "¡Gano yo!"; r.emotion = "laughing"; }
        r.text += " (" + std::to_string(score_user_) + "-" + std::to_string(score_chipo_) + ")";
        r.clip = "fun_coin";
        return r;
    }
    case PetIntent::GuessNumber: {
        if (game_ != Game::Guess) return r;
        last_ms_ = now_ms;
        r.handled = true;
        if (cmd.value < 1) { r.text = "Es un número del 1 al 10"; r.emotion = "confused"; return r; }
        ++tries_;
        if (cmd.value == secret_) {
            r.text = "¡Acertaste en " + std::to_string(tries_) + (tries_ == 1 ? " intento!" : " intentos!");
            r.emotion = "surprised";
            game_ = Game::None;
        } else if (tries_ >= 5) {
            r.text = "¡Se acabaron los intentos! Era el " + std::to_string(secret_);
            r.emotion = "laughing";
            game_ = Game::None;
        } else {
            r.text = cmd.value < secret_ ? "Más alto" : "Más bajo";
            r.emotion = "cool";
        }
        return r;
    }
    default: return r;
    }
}
