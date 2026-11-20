#pragma once

#include <cstddef>
#include <string>
#include <vector>

/**
 * CHIPO REMIND: spoken reminders at a time of day that survive a reboot ("recuérdame a las 18:30 sacar al perro").
 * Pure logic, no ESP-IDF: unit tested on a PC.
 *
 * What it can do: a reminder at a clock time, once (the next time that hour comes round) or every day.
 * What it cannot do: dates ("mañana", "el viernes") - it says so instead of guessing; a reminder whose time passes while
 * CHIPO is switched off is simply missed (there is no battery-backed clock).
 */
struct Reminder {
    int hour = 0, minute = 0;
    bool daily = false;
    std::string text;  // what to be reminded of (lower case, no accents: that is how the speech text is normalised)
};

class RemindStore {
public:
    static constexpr size_t kMax = 8;

    bool Add(const Reminder& r);                    // same time + text replaces; false if full
    size_t RemoveAt(int hour, int minute);          // returns how many were removed
    void Clear() { items_.clear(); }
    const std::vector<Reminder>& items() const { return items_; }

    // "HHMM,D,text" per line; at most ~560 bytes. Load skips malformed lines.
    std::string Serialize() const;
    void Load(const std::string& data);

    // Reminders that fall in the minutes since the last call (up to 5 min of catch-up). One-shot ones are removed.
    // Call about once a second with the real wall clock; with valid == false nothing happens.
    std::vector<Reminder> Due(bool valid, int hour, int minute);

private:
    std::vector<Reminder> items_;
    int last_minute_ = -1;  // minute of the day of the last check
};

enum class RemindIntent { None, Add, AddNeedsDate, List, ClearAll, ClearAt, Dismiss };

struct RemindCommand {
    RemindIntent intent = RemindIntent::None;
    Reminder reminder;  // Add
    int hour = 0, minute = 0;  // ClearAt
};

RemindCommand ParseRemindCommand(const char* text, bool ringing);

// "18:30" ; "Sacar al perro" ; the sentences CHIPO shows.
std::string RemindClock(int hour, int minute);
std::string RemindCapitalise(const std::string& text);
std::string RemindConfirmation(const Reminder& r);
std::string RemindListText(const std::vector<Reminder>& items);
