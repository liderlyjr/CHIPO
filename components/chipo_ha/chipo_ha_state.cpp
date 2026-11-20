#include "chipo_ha_state.h"

#include <cstring>

#include "chipo_drive_voice.h"  // DriveNormalizeText

std::vector<HaSensor> HaParseSensors(const std::string& spec) {
    std::vector<HaSensor> out;
    size_t pos = 0;
    while (pos <= spec.size() && out.size() < 12) {
        size_t end = spec.find(';', pos);
        if (end == std::string::npos) end = spec.size();
        const std::string item = spec.substr(pos, end - pos);
        pos = end + 1;
        const size_t eq = item.find('=');
        if (eq == std::string::npos) continue;
        std::string alias = item.substr(0, eq);
        std::string entity = item.substr(eq + 1);
        auto trim = [](std::string& v) {
            while (!v.empty() && v.front() == ' ') v.erase(v.begin());
            while (!v.empty() && v.back() == ' ') v.pop_back();
        };
        trim(alias);
        trim(entity);
        if (alias.empty() || alias.size() > 40 || !HaValidEntityId(entity)) continue;
        out.push_back({alias, entity});
    }
    return out;
}

bool HaBuildStateGet(const HaConfig& cfg, const std::string& entity_id, HaRequest* out) {
    std::string base;
    if (!HaNormalizeBaseUrl(cfg.base_url, &base)) return false;
    if (!HaValidEntityId(entity_id)) return false;
    if (out) {
        out->url = base + "/api/states/" + entity_id;
        out->body.clear();
    }
    return true;
}

namespace {
struct Json {
    const std::string& s;
    size_t i = 0;
    explicit Json(const std::string& text) : s(text) {}
    void Ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i; }
    bool Eof() const { return i >= s.size(); }
    // A JSON string starting at s[i] == '"'. Handles \" \\ \/ \n \t and \uXXXX (BMP -> UTF-8).
    bool Str(std::string* out) {
        if (Eof() || s[i] != '"') return false;
        ++i;
        std::string r;
        while (!Eof() && s[i] != '"') {
            char c = s[i++];
            if (c != '\\') { r.push_back(c); continue; }
            if (Eof()) return false;
            c = s[i++];
            switch (c) {
            case 'n': r.push_back('\n'); break;
            case 't': r.push_back('\t'); break;
            case 'u': {
                if (i + 4 > s.size()) return false;
                unsigned cp = 0;
                for (int k = 0; k < 4; ++k) {
                    const char h = s[i++];
                    cp <<= 4;
                    if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
                    else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                    else return false;
                }
                if (cp < 0x80) r.push_back(static_cast<char>(cp));
                else if (cp < 0x800) { r.push_back(static_cast<char>(0xC0 | (cp >> 6))); r.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
                else { r.push_back(static_cast<char>(0xE0 | (cp >> 12))); r.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); r.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
                break;
            }
            default: r.push_back(c); break;  // \" \\ \/ and anything else
            }
        }
        if (Eof()) return false;  // cut short inside the string
        ++i;                       // closing quote
        if (out) *out = r;
        return true;
    }
    // Skips any value (string, number, literal, object, array).
    bool Skip(int depth = 0) {
        Ws();
        if (Eof() || depth > 8) return false;
        if (s[i] == '"') return Str(nullptr);
        if (s[i] == '{' || s[i] == '[') {
            const char close = s[i] == '{' ? '}' : ']';
            ++i;
            Ws();
            if (!Eof() && s[i] == close) { ++i; return true; }
            while (true) {
                Ws();
                if (close == '}') {
                    if (!Str(nullptr)) return false;
                    Ws();
                    if (Eof() || s[i] != ':') return false;
                    ++i;
                }
                if (!Skip(depth + 1)) return false;
                Ws();
                if (Eof()) return false;
                if (s[i] == ',') { ++i; continue; }
                if (s[i] == close) { ++i; return true; }
                return false;
            }
        }
        while (!Eof() && s[i] != ',' && s[i] != '}' && s[i] != ']' && s[i] != ' ' && s[i] != '\n') ++i;
        return true;
    }
};
}  // namespace

bool HaParseState(const std::string& json, HaState* out) {
    Json p(json);
    p.Ws();
    if (p.Eof() || json[p.i] != '{') return false;
    ++p.i;
    HaState st;
    bool have_state = false;
    while (true) {
        p.Ws();
        if (p.Eof() || json[p.i] == '}') break;
        std::string key;
        if (!p.Str(&key)) break;
        p.Ws();
        if (p.Eof() || json[p.i] != ':') break;
        ++p.i;
        p.Ws();
        if (key == "state" && !p.Eof() && json[p.i] == '"') {
            if (!p.Str(&st.state)) break;
            have_state = true;
        } else if (key == "attributes" && !p.Eof() && json[p.i] == '{') {
            ++p.i;
            while (true) {
                p.Ws();
                if (p.Eof() || json[p.i] == '}') { if (!p.Eof()) ++p.i; break; }
                std::string akey;
                if (!p.Str(&akey)) break;
                p.Ws();
                if (p.Eof() || json[p.i] != ':') break;
                ++p.i;
                p.Ws();
                if ((akey == "unit_of_measurement" || akey == "friendly_name") && !p.Eof() && json[p.i] == '"') {
                    if (!p.Str(akey == "unit_of_measurement" ? &st.unit : &st.friendly)) break;
                } else if (!p.Skip()) {
                    break;
                }
                p.Ws();
                if (!p.Eof() && json[p.i] == ',') ++p.i;
            }
        } else if (!p.Skip()) {
            break;
        }
        p.Ws();
        if (!p.Eof() && json[p.i] == ',') ++p.i;
    }
    if (!have_state || st.state.empty()) return false;
    if (out) *out = st;
    return true;
}

std::string HaSpeakState(const std::string& alias, const HaState& s) {
    std::string name = alias;
    if (!name.empty() && name[0] >= 'a' && name[0] <= 'z') name[0] = static_cast<char>(name[0] - 'a' + 'A');
    std::string value = s.state;
    struct Map { const char* from; const char* to; };
    static const Map kWords[] = {{"on", "encendido"}, {"off", "apagado"}, {"open", "abierto"}, {"closed", "cerrado"},
                                 {"home", "en casa"}, {"not_home", "fuera de casa"}, {"unavailable", "no disponible"},
                                 {"unknown", "desconocido"}, {"locked", "cerrado con llave"}, {"unlocked", "sin llave"}};
    bool word = false;
    for (const Map& m : kWords) {
        if (value == m.from) { value = m.to; word = true; break; }
    }
    if (!word && !s.unit.empty()) {
        if (s.unit == "\xC2\xB0" "C") value += " grados";
        else if (s.unit == "\xC2\xB0" "F") value += " grados F";
        else value += (s.unit == "%" ? "%" : " " + s.unit);
    }
    return name + ": " + value;
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
bool HasTok(const Tokens& t, const std::string& w) {
    for (const auto& x : t) if (x == w) return true;
    return false;
}
}  // namespace

bool ParseHaSensorQuery(const char* text, const std::vector<HaSensor>& sensors, HaSensor* match) {
    if (sensors.empty()) return false;
    const Tokens t = Split(DriveNormalizeText(text));
    if (t.empty() || t.size() > 10) return false;
    static const char* kAsk[] = {"que", "cual", "cuanto", "cuanta", "cuantos", "cuantas", "dime", "dame", "como", "hay",
                                 "esta", "estan", "consulta", "mide", "marca", "tenemos", "tengo", "cuales"};
    bool asking = false;
    for (const char* a : kAsk) asking = asking || HasTok(t, a);
    if (!asking) return false;
    // Control phrases belong to the lights/scenes parser.
    for (const char* c : {"enciende", "apaga", "activa", "prende", "pon", "ponme"}) {
        if (!t.empty() && t[0] == c) return false;
    }
    const HaSensor* best = nullptr;
    size_t best_words = 0;
    for (const HaSensor& s : sensors) {
        const Tokens words = Split(DriveNormalizeText(s.alias.c_str()));
        if (words.empty()) continue;
        bool all = true;
        for (const auto& w : words) all = all && HasTok(t, w);
        if (all && words.size() > best_words) { best = &s; best_words = words.size(); }
    }
    if (!best) return false;
    if (match) *match = *best;
    return true;
}
