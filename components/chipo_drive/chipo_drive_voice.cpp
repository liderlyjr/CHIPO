#include "chipo_drive_voice.h"

#include <cstring>
#include <vector>

namespace {

// UTF-8 aware: the speech recogniser returns "vámonos", "conducción", "ñ"...
void AppendNormalized(std::string& out, const unsigned char* p, size_t* consumed) {
    const unsigned char c = p[0];
    *consumed = 1;
    if (c >= 'A' && c <= 'Z') { out.push_back(static_cast<char>(c - 'A' + 'a')); return; }
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) { out.push_back(static_cast<char>(c)); return; }
    if (c == 0xC3 && p[1] != 0) {
        *consumed = 2;
        switch (p[1]) {
            case 0xA1: case 0x81: out.push_back('a'); return;  // á Á
            case 0xA9: case 0x89: out.push_back('e'); return;  // é É
            case 0xAD: case 0x8D: out.push_back('i'); return;  // í Í
            case 0xB3: case 0x93: out.push_back('o'); return;  // ó Ó
            case 0xBA: case 0x9A: case 0xBC: case 0x9C: out.push_back('u'); return;  // ú Ú ü Ü
            case 0xB1: case 0x91: out.push_back('n'); return;  // ñ Ñ
            default: out.push_back(' '); return;
        }
    }
    if (c >= 0x80) {  // any other multi-byte character (¡ ¿ quotes ...): skip the whole sequence
        size_t n = 1;
        if ((c & 0xE0) == 0xC0) n = 2;
        else if ((c & 0xF0) == 0xE0) n = 3;
        else if ((c & 0xF8) == 0xF0) n = 4;
        for (size_t i = 1; i < n; ++i) {
            if (p[i] == 0) { n = i; break; }
        }
        *consumed = n;
    }
    out.push_back(' ');
}

std::vector<std::string> Tokens(const std::string& normalized) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : normalized) {
        if (c == ' ') {
            if (!cur.empty()) out.push_back(cur), cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

bool IsChipo(const std::string& t) {
    // Spellings the recogniser may give the name (the wake word itself is "chee po").
    static const char* const kNames[] = {"chipo", "chippo", "shipo", "chipu", "chepo", "cipo", "chiipo", "chipp"};
    for (const char* n : kNames) {
        if (t == n) return true;
    }
    return false;
}

bool IsFiller(const std::string& t) {
    static const char* const kFillers[] = {"ya", "pues", "venga", "oye", "hey", "ey", "eh", "bueno", "ok", "vale", "porfa", "jefe"};
    for (const char* n : kFillers) {
        if (t == n) return true;
    }
    return false;
}

bool Has(const std::vector<std::string>& tokens, const char* word) {
    for (const auto& t : tokens) {
        if (t == word) return true;
    }
    return false;
}

bool HasPrefix(const std::vector<std::string>& tokens, const char* prefix) {
    const size_t n = std::strlen(prefix);
    for (const auto& t : tokens) {
        if (t.compare(0, n, prefix) == 0) return true;
    }
    return false;
}

}  // namespace

std::string DriveNormalizeText(const char* text) {
    std::string out;
    if (text == nullptr) return out;
    const auto* p = reinterpret_cast<const unsigned char*>(text);
    while (*p) {
        size_t used = 1;
        AppendNormalized(out, p, &used);
        p += used;
    }
    return out;
}

DriveIntent ParseDriveIntent(const char* text) {
    std::vector<std::string> tokens = Tokens(DriveNormalizeText(text));
    // "chee po" / "chi po" written as two words.
    for (size_t i = 0; i + 1 < tokens.size(); ++i) {
        if ((tokens[i] == "chee" || tokens[i] == "chi" || tokens[i] == "shee") && tokens[i + 1] == "po") {
            tokens[i] = "chipo";
            tokens.erase(tokens.begin() + static_cast<long>(i) + 1);
        }
    }
    // Commands are short. A long sentence that merely contains one of the words is not a command.
    if (tokens.empty() || tokens.size() > 12) return DriveIntent::None;

    const bool names_chipo = [&] {
        for (const auto& t : tokens) {
            if (IsChipo(t)) return true;
        }
        return false;
    }();

    // ---- leave the car mode -----------------------------------------------------------------
    const bool mode_word = Has(tokens, "modo");
    if (mode_word && HasPrefix(tokens, "desactiv")) {
        if (Has(tokens, "conduccion") || Has(tokens, "coche") || Has(tokens, "drive") || Has(tokens, "copiloto")) {
            return DriveIntent::Deactivate;
        }
    }
    if (mode_word && (Has(tokens, "escritorio") || Has(tokens, "mesa"))) {
        if (Has(tokens, "vuelve") || Has(tokens, "volver") || Has(tokens, "pasa") || Has(tokens, "pon") ||
            Has(tokens, "cambia") || HasPrefix(tokens, "activ")) {
            return DriveIntent::Deactivate;
        }
    }
    if (mode_word && (Has(tokens, "sal") || Has(tokens, "salir") || Has(tokens, "salte")) &&
        (Has(tokens, "coche") || Has(tokens, "conduccion") || Has(tokens, "drive"))) {
        return DriveIntent::Deactivate;
    }

    // ---- sensitivity ------------------------------------------------------------------------
    if (HasPrefix(tokens, "sensibilidad")) {
        if (Has(tokens, "suave") || Has(tokens, "baja")) return DriveIntent::SensitivitySoft;
        if (Has(tokens, "normal") || Has(tokens, "media")) return DriveIntent::SensitivityNormal;
        if (Has(tokens, "intensa") || Has(tokens, "alta") || Has(tokens, "maxima")) return DriveIntent::SensitivityIntense;
    }

    // ---- enter the car mode -----------------------------------------------------------------
    // "Chipo, vámonos": the wake word starts the conversation, so the text may arrive as just
    // "vámonos". Fillers around it are fine, any other word is not ("vámonos a comer").
    {
        std::vector<std::string> core;
        for (const auto& t : tokens) {
            if (!IsChipo(t) && !IsFiller(t)) core.push_back(t);
        }
        if (core.size() == 1 && core[0] == "vamonos") return DriveIntent::Activate;
    }
    if (names_chipo && Has(tokens, "drive")) return DriveIntent::Activate;
    if (mode_word && !HasPrefix(tokens, "desactiv") &&
        (Has(tokens, "conduccion") || Has(tokens, "coche")) &&
        (HasPrefix(tokens, "activ") || Has(tokens, "pon") || Has(tokens, "entra") || Has(tokens, "cambia"))) {
        return DriveIntent::Activate;
    }
    return DriveIntent::None;
}
