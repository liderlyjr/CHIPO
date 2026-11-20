#pragma once

#include <string>

/**
 * Home Assistant REST helpers (pure logic, no ESP-IDF: unit tested on a PC).
 *
 * Only documented Home Assistant REST API calls are used:
 *   POST <base>/api/services/<domain>/<service>   Authorization: Bearer <long-lived token>
 *   body {"entity_id": "<entity>"}
 * The address and token are never in the source code: they come from the device's NVS
 * (namespace "chipo_ha", see docs/CHIPO_HOME_ASSISTANT.md). Without them everything here stays off.
 */
struct HaConfig {
    std::string base_url;       // "http://homeassistant.local:8123" (no trailing slash)
    std::string token;          // long-lived access token
    std::string scene_night;    // entity id, e.g. "scene.buenas_noches" ("" = none)
    std::string scene_morning;  // e.g. "scene.buenos_dias"
    std::string sensors;        // readable sensors by spoken name: "temperatura salon=sensor.salon_temp;humedad=sensor.hum"
    std::string media_player;   // entity id of the music player, e.g. "media_player.salon" ("" = none)
    bool usable() const;        // address valid and token present
};

// "scene.cine", "light.salon": lower case letters, digits and '_' on both sides of a single '.'.
bool HaValidEntityId(const std::string& id);
// "Salón de abajo" -> "salon_de_abajo"
std::string HaSlug(const std::string& text);
// Trims spaces and trailing '/', accepts only http:// or https:// with a host.
bool HaNormalizeBaseUrl(const std::string& in, std::string* out);

struct HaRequest {
    std::string url;
    std::string body;
};
// domain/service are fixed identifiers ("scene"/"turn_on", "light"/"turn_off").
bool HaBuildServiceCall(const HaConfig& cfg, const std::string& domain, const std::string& service,
                        const std::string& entity_id, HaRequest* out);

enum class HaIntentKind { None, Scene, LightOn, LightOff, AllLightsOff };
struct HaIntent {
    HaIntentKind kind = HaIntentKind::None;
    std::string entity_id;  // "scene.cine", "light.salon"
    std::string spoken;     // the name as said ("cine", "salón"), for the on-screen text
};
// "activa la escena cine", "enciende la luz del salón", "apaga todas las luces"
HaIntent ParseHaIntent(const char* text);
