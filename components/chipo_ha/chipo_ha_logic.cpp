#include "chipo_ha_logic.h"

#include <cstring>
#include <vector>

#include "chipo_drive_voice.h"  // DriveNormalizeText

bool HaConfig::usable() const {
    std::string norm;
    return !token.empty() && HaNormalizeBaseUrl(base_url, &norm);
}

bool HaValidEntityId(const std::string& id) {
    const size_t dot = id.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= id.size() || id.size() > 80) return false;
    if (id.find('.', dot + 1) != std::string::npos) return false;
    for (char c : id) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
        if (!ok) return false;
    }
    return true;
}

std::string HaSlug(const std::string& text) {
    const std::string norm = DriveNormalizeText(text.c_str());
    std::string out;
    bool pending = false;
    for (char c : norm) {
        if (c == ' ') {
            pending = !out.empty();
        } else {
            if (pending) out.push_back('_');
            pending = false;
            out.push_back(c);
        }
    }
    return out;
}

bool HaNormalizeBaseUrl(const std::string& in, std::string* out) {
    size_t b = 0, e = in.size();
    while (b < e && in[b] == ' ') ++b;
    while (e > b && (in[e - 1] == ' ' || in[e - 1] == '/')) --e;
    std::string u = in.substr(b, e - b);
    size_t host_at = 0;
    if (u.compare(0, 7, "http://") == 0) host_at = 7;
    else if (u.compare(0, 8, "https://") == 0) host_at = 8;
    else return false;
    if (u.size() <= host_at || u.size() > 120) return false;
    for (size_t i = host_at; i < u.size(); ++i) {
        const char c = u[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == ':' || c == '/' || c == '_';
        if (!ok) return false;
    }
    if (out) *out = u;
    return true;
}

bool HaBuildServiceCall(const HaConfig& cfg, const std::string& domain, const std::string& service,
                        const std::string& entity_id, HaRequest* out) {
    std::string base;
    if (!HaNormalizeBaseUrl(cfg.base_url, &base)) return false;
    auto ident = [](const std::string& s) {
        if (s.empty() || s.size() > 32) return false;
        for (char c : s) {
            if (!((c >= 'a' && c <= 'z') || c == '_')) return false;
        }
        return true;
    };
    if (!ident(domain) || !ident(service)) return false;
    if (entity_id != "all" && !HaValidEntityId(entity_id)) return false;  // "all" is a Home Assistant keyword
    if (out) {
        out->url = base + "/api/services/" + domain + "/" + service;
        out->body = "{\"entity_id\":\"" + entity_id + "\"}";  // validated characters only: nothing to escape
    }
    return true;
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
std::string Join(const Tokens& t, size_t from) {
    std::string s;
    for (size_t i = from; i < t.size(); ++i) {
        if (!s.empty()) s.push_back(' ');
        s += t[i];
    }
    return s;
}
}  // namespace

HaIntent ParseHaIntent(const char* text) {
    HaIntent none;
    Tokens t = Split(DriveNormalizeText(text));
    if (t.empty() || t.size() > 10) return none;
    // Drop the name and fillers.
    Tokens core;
    for (const auto& w : t) {
        if (In(w, {"chipo", "chippo", "shipo", "chipu", "chepo", "cipo", "por", "favor", "porfa", "oye", "venga", "ya", "pues"})) continue;
        core.push_back(w);
    }
    if (core.size() < 2) return none;

    const bool on = In(core[0], {"enciende", "prende", "activa", "pon", "ponme"});
    const bool off = In(core[0], {"apaga", "desactiva", "quita"});

    // "activa la escena cine" / "pon la escena cine"
    for (size_t i = 1; i < core.size(); ++i) {
        if (core[i] == "escena" && on && i + 1 < core.size()) {
            size_t from = i + 1;
            if (core[from] == "de" && from + 1 < core.size()) ++from;
            const std::string spoken = Join(core, from);
            const std::string slug = HaSlug(spoken);
            if (slug.empty()) return none;
            HaIntent r;
            r.kind = HaIntentKind::Scene;
            r.entity_id = "scene." + slug;
            r.spoken = spoken;
            return HaValidEntityId(r.entity_id) ? r : none;
        }
    }
    // "apaga todas las luces"
    if (off && core.size() <= 4 && (In(core.back(), {"luces"})) && In(core[1], {"todas", "las"})) {
        HaIntent r;
        r.kind = HaIntentKind::AllLightsOff;
        r.entity_id = "all";
        r.spoken = "todas las luces";
        return r;
    }
    // "enciende la luz del salón" / "apaga la luz de la cocina" / "enciende la luz del salón de abajo"
    if (on || off) {
        for (size_t i = 1; i < core.size(); ++i) {
            if (core[i] == "luz" && i + 1 < core.size()) {
                size_t from = i + 1;
                if (In(core[from], {"del", "de", "la", "el"})) {
                    ++from;
                    if (from < core.size() && In(core[from], {"la", "el"})) ++from;  // "de la cocina"
                }
                if (from >= core.size()) return none;
                const std::string spoken = Join(core, from);
                const std::string slug = HaSlug(spoken);
                HaIntent r;
                r.kind = on ? HaIntentKind::LightOn : HaIntentKind::LightOff;
                r.entity_id = "light." + slug;
                r.spoken = spoken;
                return HaValidEntityId(r.entity_id) ? r : none;
            }
        }
    }
    return none;
}
