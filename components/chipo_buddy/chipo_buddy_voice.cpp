#include "chipo_buddy_voice.h"

#include <cstdlib>
#include <string>
#include <vector>

#include "chipo_drive_voice.h"  // DriveNormalizeText: lower case, no accents

namespace {

using Tokens = std::vector<std::string>;

Tokens Split(const std::string& s) {
    Tokens out;
    std::string cur;
    for (char c : s) {
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
    static const char* const kNames[] = {"chipo", "chippo", "shipo", "chipu", "chepo", "cipo", "chiipo", "chipp"};
    for (const char* n : kNames) {
        if (t == n) return true;
    }
    return false;
}

bool IsFiller(const std::string& t) {
    static const char* const kFillers[] = {"ya", "pues", "venga", "oye", "hey", "ey", "eh", "bueno", "ok", "porfa", "jefe", "por", "favor"};
    for (const char* n : kFillers) {
        if (t == n) return true;
    }
    return false;
}

bool Has(const Tokens& t, const char* w) {
    for (const auto& x : t) {
        if (x == w) return true;
    }
    return false;
}

bool HasPrefix(const Tokens& t, const char* prefix) {
    const std::string p(prefix);
    for (const auto& x : t) {
        if (x.compare(0, p.size(), p) == 0) return true;
    }
    return false;
}

// Spanish number words 0..99 ("diez", "veintiuno", "treinta y cinco"). Returns tokens consumed (0: none).
int WordNumber(const Tokens& t, size_t i, int* value) {
    struct W { const char* w; int v; };
    static const W kUnits[] = {{"cero", 0}, {"un", 1}, {"uno", 1}, {"una", 1}, {"dos", 2}, {"tres", 3}, {"cuatro", 4}, {"cinco", 5},
                               {"seis", 6}, {"siete", 7}, {"ocho", 8}, {"nueve", 9}, {"diez", 10}, {"once", 11}, {"doce", 12},
                               {"trece", 13}, {"catorce", 14}, {"quince", 15}, {"dieciseis", 16}, {"diecisiete", 17},
                               {"dieciocho", 18}, {"diecinueve", 19}, {"veinte", 20}, {"veintiuno", 21}, {"veintiun", 21},
                               {"veintidos", 22}, {"veintitres", 23}, {"veinticuatro", 24}, {"veinticinco", 25},
                               {"veintiseis", 26}, {"veintisiete", 27}, {"veintiocho", 28}, {"veintinueve", 29}};
    static const W kTens[] = {{"treinta", 30}, {"cuarenta", 40}, {"cincuenta", 50}, {"sesenta", 60}, {"setenta", 70},
                              {"ochenta", 80}, {"noventa", 90}};
    const std::string& w = t[i];
    if (!w.empty() && w.find_first_not_of("0123456789") == std::string::npos && w.size() <= 4) {
        *value = std::atoi(w.c_str());
        return 1;
    }
    for (const W& u : kUnits) {
        if (w == u.w) { *value = u.v; return 1; }
    }
    for (const W& d : kTens) {
        if (w == d.w) {
            *value = d.v;
            if (i + 2 < t.size() && t[i + 1] == "y") {
                for (const W& u : kUnits) {
                    if (t[i + 2] == u.w && u.v >= 1 && u.v <= 9) { *value += u.v; return 3; }
                }
            }
            return 1;
        }
    }
    return 0;
}

enum class Unit { None, Hour, Minute, Second };

Unit UnitOf(const std::string& w) {
    if (w == "hora" || w == "horas") return Unit::Hour;
    if (w == "minuto" || w == "minutos" || w == "min") return Unit::Minute;
    if (w == "segundo" || w == "segundos" || w == "seg") return Unit::Second;
    return Unit::None;
}

// Sum of every "<number> <unit>" in the sentence, plus "media hora" / "cuarto de hora" / "y media".
uint32_t ParseDuration(const Tokens& t) {
    uint32_t total = 0;
    for (size_t i = 0; i < t.size(); ++i) {
        int v = 0;
        const int used = WordNumber(t, i, &v);
        if (used > 0 && i + used < t.size()) {
            const Unit u = UnitOf(t[i + used]);
            if (u == Unit::Hour) total += static_cast<uint32_t>(v) * 3600u;
            else if (u == Unit::Minute) total += static_cast<uint32_t>(v) * 60u;
            else if (u == Unit::Second) total += static_cast<uint32_t>(v);
            if (u != Unit::None) {
                i += used;  // skip the unit too
                // "una hora y media" / "una hora y cuarto"
                if (u == Unit::Hour && i + 2 < t.size() && t[i + 1] == "y") {
                    if (t[i + 2] == "media") total += 1800, i += 2;
                    else if (t[i + 2] == "cuarto") total += 900, i += 2;
                }
                continue;
            }
        }
    }
    for (size_t i = 0; i + 1 < t.size(); ++i) {
        if (t[i] == "media" && t[i + 1] == "hora") total += 1800;
        if (t[i] == "cuarto" && i + 2 < t.size() && t[i + 1] == "de" && t[i + 2] == "hora") total += 900;
    }
    // A bare number after the trigger ("temporizador 10") means minutes.
    if (total == 0) {
        for (size_t i = 0; i < t.size(); ++i) {
            int v = 0;
            const int used = WordNumber(t, i, &v);
            if (used > 0 && i + used == t.size() && v > 0) return static_cast<uint32_t>(v) * 60u;
        }
    }
    return total;
}

bool ParseAlarmTime(const Tokens& t, int* hour, int* minute, size_t* end = nullptr) {
    for (size_t i = 0; i + 1 < t.size(); ++i) {
        if (t[i] != "las" && t[i] != "la") continue;
        int h = 0;
        const int used = WordNumber(t, i + 1, &h);
        if (used == 0 || h > 24) continue;
        size_t j = i + 1 + used;
        int m = 0;
        if (j < t.size()) {
            int mv = 0;
            const int mu = WordNumber(t, j, &mv);
            if (t[j] == "y" && j + 1 < t.size()) {
                if (t[j + 1] == "media") { m = 30; j += 2; }
                else if (t[j + 1] == "cuarto") { m = 15; j += 2; }
                else {
                    const int mu2 = WordNumber(t, j + 1, &mv);
                    if (mu2 > 0 && mv < 60) { m = mv; j += 1 + mu2; }
                }
            } else if (t[j] == "menos" && j + 1 < t.size() && t[j + 1] == "cuarto") {
                m = 45;
                h = (h + 23) % 24;
                j += 2;
            } else if (mu > 0 && mv < 60) {  // "7 45" (from "7:45")
                m = mv;
                j += mu;
            }
        }
        bool pm = false, am = false;
        for (size_t k = j; k < t.size(); ++k) {
            if (t[k] == "tarde" || t[k] == "noche") pm = true;
            if (t[k] == "manana" || t[k] == "madrugada") am = true;
        }
        const bool night = [&] { for (size_t k = j; k < t.size(); ++k) if (t[k] == "noche") return true; return false; }();
        if (night && h == 12) h = 0;  // "las doce de la noche" is midnight
        else if (pm && h < 12) h += 12;
        if (am && h == 12) h = 0;
        if (h == 24) h = 0;
        *hour = h;
        *minute = m;
        if (end) *end = j;
        return true;
    }
    return false;
}

}  // namespace

bool BuddyParseClockTime(const std::vector<std::string>& tokens, int* hour, int* minute, size_t* end) {
    return ParseAlarmTime(tokens, hour, minute, end);
}

BuddyCommand ParseBuddyCommand(const char* text) {
    BuddyCommand cmd;
    Tokens t = Split(DriveNormalizeText(text));
    if (t.empty() || t.size() > 16) return cmd;

    Tokens core;
    for (const auto& x : t) {
        if (!IsChipo(x) && !IsFiller(x)) core.push_back(x);
    }
    if (core.empty()) return cmd;

    auto only = [&](std::initializer_list<const char*> words) {
        if (core.size() != words.size()) return false;
        size_t i = 0;
        for (const char* w : words) {
            if (core[i++] != w) return false;
        }
        return true;
    };

    // ---- single phrases ---------------------------------------------------------------------
    if (only({"buenas", "noches"}) || only({"a", "dormir"}) || only({"modo", "dormir"}) || only({"modo", "mesita"}) ||
        only({"activa", "modo", "dormir"}) || only({"pon", "modo", "dormir"}) || only({"modo", "noche"})) {
        cmd.intent = BuddyIntent::Bedtime;
        return cmd;
    }
    if (only({"buenos", "dias"}) || only({"sal", "del", "modo", "dormir"}) || only({"salir", "del", "modo", "dormir"}) ||
        only({"desactiva", "modo", "dormir"}) || only({"desactiva", "el", "modo", "dormir"}) || only({"despierta"}) || only({"despiertate"})) {
        cmd.intent = BuddyIntent::Wake;
        return cmd;
    }
    if (only({"para"}) || only({"vale"}) || only({"silencio"}) || only({"callate"}) || only({"ya", "esta"}) || only({"apagar"}) ||
        only({"apaga"}) || only({"detente"}) || only({"basta"}) || only({"ya"}) || only({"stop"})) {
        cmd.intent = BuddyIntent::Dismiss;
        return cmd;
    }
    if (only({"que", "hora", "es"}) || only({"dime", "la", "hora"}) || only({"que", "hora", "tenemos"}) || only({"hora"})) {
        cmd.intent = BuddyIntent::AskTime;
        return cmd;
    }
    if (only({"cuanto", "queda"}) || only({"cuanto", "falta"}) || only({"cuanto", "tiempo", "queda"}) ||
        only({"cuanto", "tiempo", "falta"}) || only({"que", "tiempo", "queda"})) {
        cmd.intent = BuddyIntent::TimeLeft;
        return cmd;
    }

    const bool alarm = HasPrefix(core, "alarma") || Has(core, "despiertame");
    const bool cancel = HasPrefix(core, "cancel") || HasPrefix(core, "quita") || HasPrefix(core, "borra") || HasPrefix(core, "elimina") ||
                        Has(core, "apaga") || HasPrefix(core, "desactiv");

    // ---- alarm ------------------------------------------------------------------------------
    if (alarm && cancel) {
        cmd.intent = BuddyIntent::CancelAlarm;
        return cmd;
    }
    if (alarm) {
        int h = 0, m = 0;
        if (ParseAlarmTime(core, &h, &m)) {
            cmd.intent = BuddyIntent::SetAlarm;
            cmd.hour = h;
            cmd.minute = m;
            return cmd;
        }
        // "despiértame en 20 minutos" falls through to the timer.
    }

    // ---- stopwatch --------------------------------------------------------------------------
    if (HasPrefix(core, "cronometro") && core.size() <= 6 && !Has(core, "que") && !Has(core, "como")) {
        const bool stop = Has(core, "para") || HasPrefix(core, "deten") || HasPrefix(core, "pausa") || HasPrefix(core, "termina") || cancel;
        cmd.intent = stop ? BuddyIntent::StopwatchStop : BuddyIntent::StopwatchStart;
        return cmd;
    }

    // ---- pomodoro ---------------------------------------------------------------------------
    if (HasPrefix(core, "pomodoro") || only({"modo", "concentracion"}) || only({"pon", "modo", "concentracion"}) ||
        only({"activa", "modo", "concentracion"}) || only({"modo", "estudio"})) {
        if (cancel || Has(core, "para") || Has(core, "deten")) cmd.intent = BuddyIntent::CancelTimers;
        else cmd.intent = BuddyIntent::Pomodoro;
        return cmd;
    }

    // ---- timer ------------------------------------------------------------------------------
    const bool timer_word = HasPrefix(core, "temporizador") || HasPrefix(core, "temporiza") || Has(core, "avisame") ||
                            Has(core, "recuerdame") || (Has(core, "cuenta") && Has(core, "atras")) || (alarm && Has(core, "en")) ||
                            HasPrefix(core, "tempo");
    if (timer_word) {
        if (cancel || Has(core, "para") || HasPrefix(core, "deten")) {
            cmd.intent = BuddyIntent::CancelTimers;
            return cmd;
        }
        const uint32_t secs = ParseDuration(core);
        if (secs > 0 && secs <= 24u * 3600u) {
            cmd.intent = BuddyIntent::SetTimer;
            cmd.seconds = secs;
        }
    }
    return cmd;
}
