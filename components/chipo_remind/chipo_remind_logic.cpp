#include "chipo_remind_logic.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "chipo_buddy_voice.h"
#include "chipo_drive_voice.h"  // DriveNormalizeText

// ---- store ---------------------------------------------------------------------------------------------
bool RemindStore::Add(const Reminder& r) {
    for (auto& x : items_) {
        if (x.hour == r.hour && x.minute == r.minute && x.text == r.text) { x.daily = r.daily; return true; }
    }
    if (items_.size() >= kMax) return false;
    items_.push_back(r);
    return true;
}

size_t RemindStore::RemoveAt(int hour, int minute) {
    const size_t before = items_.size();
    items_.erase(std::remove_if(items_.begin(), items_.end(), [&](const Reminder& r) { return r.hour == hour && r.minute == minute; }), items_.end());
    return before - items_.size();
}

std::string RemindStore::Serialize() const {
    std::string out;
    for (const auto& r : items_) {
        char head[16];
        std::snprintf(head, sizeof(head), "%02d%02d,%d,", r.hour, r.minute, r.daily ? 1 : 0);
        out += head;
        for (char c : r.text) out.push_back(c == '\n' || c == '\r' ? ' ' : c);
        out.push_back('\n');
    }
    return out;
}

void RemindStore::Load(const std::string& data) {
    items_.clear();
    size_t pos = 0;
    while (pos < data.size() && items_.size() < kMax) {
        size_t end = data.find('\n', pos);
        if (end == std::string::npos) end = data.size();
        const std::string line = data.substr(pos, end - pos);
        pos = end + 1;
        if (line.size() < 7 || line[4] != ',' || line[6] != ',') continue;
        auto digit = [&](size_t i) { return line[i] >= '0' && line[i] <= '9'; };
        if (!digit(0) || !digit(1) || !digit(2) || !digit(3) || (line[5] != '0' && line[5] != '1')) continue;
        Reminder r;
        r.hour = (line[0] - '0') * 10 + (line[1] - '0');
        r.minute = (line[2] - '0') * 10 + (line[3] - '0');
        if (r.hour > 23 || r.minute > 59) continue;
        r.daily = line[5] == '1';
        r.text = line.substr(7, 60);
        items_.push_back(r);
    }
}

std::vector<Reminder> RemindStore::Due(bool valid, int hour, int minute) {
    std::vector<Reminder> fired;
    if (!valid) return fired;
    const int now = hour * 60 + minute;
    int from = now;
    if (last_minute_ >= 0 && last_minute_ != now) {
        const int gap = (now - last_minute_ + 1440) % 1440;
        from = gap <= 5 ? (last_minute_ + 1) % 1440 : now;  // short catch-up only, never a flood after a clock jump
    } else if (last_minute_ == now) {
        return fired;
    }
    last_minute_ = now;
    for (int m = from;; m = (m + 1) % 1440) {
        for (size_t i = 0; i < items_.size();) {
            if (items_[i].hour * 60 + items_[i].minute == m) {
                fired.push_back(items_[i]);
                if (!items_[i].daily) { items_.erase(items_.begin() + static_cast<long>(i)); continue; }
            }
            ++i;
        }
        if (m == now) break;
    }
    return fired;
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
}  // namespace

RemindCommand ParseRemindCommand(const char* text, bool ringing) {
    RemindCommand cmd;
    Tokens core;
    for (const auto& w : Split(DriveNormalizeText(text)))
        if (!In(w, {"chipo", "chippo", "shipo", "chipu", "chepo", "cipo", "porfa", "favor", "oye", "venga", "pues", "hey", "ey"})) core.push_back(w);
    if (core.empty() || core.size() > 16) return cmd;

    if (ringing && core.size() <= 3 && Has(core, {"vale", "para", "silencio", "gracias", "ok", "entendido", "basta", "ya", "callate"})) {
        cmd.intent = RemindIntent::Dismiss;
        return cmd;
    }

    const bool plural = Has(core, {"recordatorios"});
    const bool singular = Has(core, {"recordatorio"});
    const bool erase = Has(core, {"borra", "cancela", "elimina", "quita", "olvida", "olvidate", "anula", "borrar", "cancelar"});
    if (plural || singular) {
        int h = 0, m = 0;
        size_t end = 0;
        if (erase) {
            if (singular && BuddyParseClockTime(core, &h, &m, &end)) { cmd.intent = RemindIntent::ClearAt; cmd.hour = h; cmd.minute = m; return cmd; }
            if (plural || Has(core, {"todos"})) { cmd.intent = RemindIntent::ClearAll; return cmd; }
            return cmd;
        }
        if (Has(core, {"que", "cuales", "dime", "lista", "mis", "tengo", "hay", "muestrame", "cuantos"}) && core.size() <= 8) {
            cmd.intent = RemindIntent::List;
            return cmd;
        }
        return cmd;
    }

    if (!Has(core, {"recuerdame", "recuerda", "avisame", "recordarme"})) return cmd;
    int h = 0, m = 0;
    size_t end = 0;
    if (!BuddyParseClockTime(core, &h, &m, &end)) return cmd;

    // Dates are not supported: "mañana a las 8" before the time (after it, "de la mañana" is just am).
    size_t first_las = 0;
    while (first_las < core.size() && core[first_las] != "las" && core[first_las] != "la") ++first_las;
    for (size_t i = 0; i < first_las; ++i) {
        if (In(core[i], {"manana", "pasado", "lunes", "martes", "miercoles", "jueves", "viernes", "sabado", "domingo"})) {
            cmd.intent = RemindIntent::AddNeedsDate;
            return cmd;
        }
    }

    Reminder r;
    r.hour = h;
    r.minute = m;
    // "every day" can be said before or after the time; it never belongs to the label.
    auto daily_at = [&](size_t i) -> size_t {  // tokens used by a "daily" phrase starting at i (0: none)
        if (core[i] == "todos" && i + 2 < core.size() && core[i + 1] == "los" && (core[i + 2] == "dias" || core[i + 2] == "dia")) return 3;
        if (core[i] == "cada" && i + 1 < core.size() && core[i + 1] == "dia") return 2;
        if (core[i] == "diariamente") return 1;
        return 0;
    };
    for (size_t i = 0; i < core.size(); ++i) if (daily_at(i)) r.daily = true;
    Tokens label;
    for (size_t i = end; i < core.size();) {
        const size_t used = daily_at(i);
        if (used) { i += used; continue; }
        label.push_back(core[i++]);
    }
    // Leading glue: "de la tarde", "por la noche", "de la manana", "que", "para", "a"...
    while (!label.empty() && In(label.front(), {"de", "la", "por", "en", "tarde", "noche", "manana", "madrugada", "mediodia", "del", "a", "que", "para", "punto", "y", "pm", "am", "horas", "hora"}))
        label.erase(label.begin());
    for (const auto& w : label) {
        if (!r.text.empty()) r.text += ' ';
        r.text += w;
    }
    if (r.text.size() > 60) r.text.resize(60);
    if (r.text.empty()) r.text = "recordatorio";
    cmd.intent = RemindIntent::Add;
    cmd.reminder = r;
    return cmd;
}

// ---- texts ---------------------------------------------------------------------------------------------
std::string RemindClock(int hour, int minute) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
    return buf;
}

std::string RemindCapitalise(const std::string& text) {
    std::string out = text;
    if (!out.empty() && out[0] >= 'a' && out[0] <= 'z') out[0] = static_cast<char>(out[0] - 'a' + 'A');
    return out;
}

std::string RemindConfirmation(const Reminder& r) {
    return std::string("Te lo recordaré ") + (r.daily ? "todos los días a las " : "a las ") + RemindClock(r.hour, r.minute) +
           ": " + r.text;
}

std::string RemindListText(const std::vector<Reminder>& items) {
    if (items.empty()) return "No tienes recordatorios";
    std::string out = items.size() == 1 ? "1 recordatorio: " : std::to_string(items.size()) + " recordatorios: ";
    for (size_t i = 0; i < items.size() && i < 3; ++i) {
        if (i) out += "; ";
        out += RemindClock(items[i].hour, items[i].minute) + " " + items[i].text;
    }
    if (items.size() > 3) out += "...";
    return out;
}
