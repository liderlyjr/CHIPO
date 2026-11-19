#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/**
 * Spoken commands of CHIPO BUDDY (timers, pomodoro, stopwatch, alarm, bedtime), matched on the text
 * the server's speech recognition returns. Pure text logic, no ESP-IDF: unit tested on a PC.
 */
enum class BuddyIntent {
    None,
    SetTimer,        // "temporizador de 10 minutos", "avísame en media hora"  -> seconds
    Pomodoro,        // "pomodoro", "modo concentración"
    StopwatchStart,  // "cronómetro"
    StopwatchStop,   // "para el cronómetro"
    CancelTimers,    // "cancela el temporizador"
    TimeLeft,        // "cuánto queda"
    SetAlarm,        // "despiértame a las 7 y media"  -> hour, minute
    CancelAlarm,     // "cancela la alarma"
    Bedtime,         // "buenas noches", "modo dormir"
    Wake,            // "buenos días", "sal del modo dormir"
    AskTime,         // "qué hora es"
    Dismiss          // "para", "vale", "silencio" (only meaningful while ringing)
};

struct BuddyCommand {
    BuddyIntent intent = BuddyIntent::None;
    uint32_t seconds = 0;  // SetTimer
    int hour = 0;          // SetAlarm, 0..23
    int minute = 0;        // SetAlarm, 0..59
};

BuddyCommand ParseBuddyCommand(const char* text);

// "a las 18:30", "a las seis y media", "a las 7 de la tarde" in already normalised tokens (DriveNormalizeText, split on
// spaces). On success `*end` is the index of the first token after the time itself ("de la tarde" may still follow).
bool BuddyParseClockTime(const std::vector<std::string>& tokens, int* hour, int* minute, size_t* end);
