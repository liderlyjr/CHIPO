#pragma once

#include <string>
#include <vector>

#include "chipo_ha_logic.h"

/**
 * Reading Home Assistant sensors by voice (pure logic, no ESP-IDF, unit tested on a PC).
 *
 * Only the documented REST call GET <base>/api/states/<entity_id> is used (Authorization: Bearer <token>).
 * The sensors CHIPO may read are the ones listed in the device's NVS ("sensors" key, see docs), each with the
 * name you say aloud:  "temperatura salon=sensor.salon_temp;humedad=sensor.salon_hum".
 */
struct HaSensor {
    std::string alias;      // as configured, e.g. "temperatura salon"
    std::string entity_id;  // "sensor.salon_temp"
};

// Invalid entries are skipped; at most 12 are kept.
std::vector<HaSensor> HaParseSensors(const std::string& spec);

// GET request for one entity (url only, no body).
bool HaBuildStateGet(const HaConfig& cfg, const std::string& entity_id, HaRequest* out);

struct HaState {
    std::string state;    // "21.5", "on", "unavailable"
    std::string unit;     // "°C", "%", "" (attributes.unit_of_measurement)
    std::string friendly; // attributes.friendly_name, "" if missing
};
// Reads state / unit / name from the JSON of GET /api/states/<entity>. Tolerates a body cut short after the state.
bool HaParseState(const std::string& json, HaState* out);

// "Temperatura salon: 21.5 grados", "Puerta: abierta". `alias` is how the owner named it.
std::string HaSpeakState(const std::string& alias, const HaState& state);

// "qué temperatura hay en el salón" -> the best matching sensor (all words of its alias must be said, the longest wins).
bool ParseHaSensorQuery(const char* text, const std::vector<HaSensor>& sensors, HaSensor* match);
