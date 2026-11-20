#include <cstdio>
#include <cstring>

#include "chipo_ha_state.h"

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); ++g_fail; } } while (0)

int main() {
    std::printf("== sensors list\n");
    {
        auto v = HaParseSensors("temperatura salon=sensor.salon_temp; humedad = sensor.salon_hum ;malo=Sensor.X;sin_igual;=sensor.a;puerta=binary_sensor.puerta");
        CHECK(v.size() == 3, "3 valid entries (%zu)", v.size());
        CHECK(v.size() == 3 && v[0].alias == "temperatura salon" && v[0].entity_id == "sensor.salon_temp", "first");
        CHECK(v.size() == 3 && v[1].alias == "humedad" && v[1].entity_id == "sensor.salon_hum", "trimmed");
        CHECK(HaParseSensors("").empty(), "empty");
        std::string many;
        for (int i = 0; i < 30; ++i) many += "s" + std::to_string(i) + "=sensor.s" + std::to_string(i) + ";";
        CHECK(HaParseSensors(many).size() == 12, "capped at 12");
    }
    std::printf("== request\n");
    {
        HaConfig cfg; cfg.base_url = "http://homeassistant.local:8123/"; cfg.token = "x";
        HaRequest r;
        CHECK(HaBuildStateGet(cfg, "sensor.salon_temp", &r) && r.url == "http://homeassistant.local:8123/api/states/sensor.salon_temp" && r.body.empty(), "url (%s)", r.url.c_str());
        CHECK(!HaBuildStateGet(cfg, "sensor.salon temp", &r), "bad entity refused");
        CHECK(!HaBuildStateGet(cfg, "../api/config", &r), "path tricks refused");
        cfg.base_url = "ftp://x";
        CHECK(!HaBuildStateGet(cfg, "sensor.a", &r), "bad address refused");
    }
    std::printf("== state JSON\n");
    {
        HaState s;
        const char* j = "{\"entity_id\":\"sensor.salon_temp\",\"state\":\"21.5\",\"attributes\":{\"state_class\":\"measurement\",\"unit_of_measurement\":\"\\u00b0C\",\"friendly_name\":\"Salón temperatura\",\"extra\":{\"state\":\"nope\",\"list\":[1,2,{\"a\":\"b\"}]}},\"last_changed\":\"2026-11-18T10:00:00+00:00\"}";
        CHECK(HaParseState(j, &s), "parses");
        CHECK(s.state == "21.5", "state (%s)", s.state.c_str());
        CHECK(s.unit == "\xC2\xB0" "C", "unit with unicode escape (%s)", s.unit.c_str());
        CHECK(s.friendly == "Salón temperatura", "friendly name (%s)", s.friendly.c_str());
        CHECK(HaSpeakState("temperatura salon", s) == "Temperatura salon: 21.5 grados", "spoken (%s)", HaSpeakState("temperatura salon", s).c_str());

        CHECK(HaParseState("{\"state\":\"on\",\"attributes\":{}}", &s) && HaSpeakState("puerta", s) == "Puerta: encendido", "on");
        CHECK(HaParseState("{\"state\":\"unavailable\"}", &s) && HaSpeakState("luz", s) == "Luz: no disponible", "unavailable");
        CHECK(HaParseState("{\"state\":\"45\",\"attributes\":{\"unit_of_measurement\":\"%\"}}", &s) && HaSpeakState("humedad", s) == "Humedad: 45%", "percent");
        CHECK(HaParseState("{\"state\":\"1013\",\"attributes\":{\"unit_of_measurement\":\"hPa\"}}", &s) && HaSpeakState("presion", s) == "Presion: 1013 hPa", "other unit");
        // A body cut short in the middle of the attributes still gives the state.
        CHECK(HaParseState("{\"entity_id\":\"sensor.a\",\"state\":\"7\",\"attributes\":{\"unit_of_measurement\":\"W\",\"big\":[1,2,3,", &s) && s.state == "7" && s.unit == "W", "truncated");
        CHECK(!HaParseState("", &s), "empty");
        CHECK(!HaParseState("not json", &s), "garbage");
        CHECK(!HaParseState("{\"message\":\"Entity not found.\"}", &s), "error body has no state");
        CHECK(!HaParseState("{\"state\":\"", &s), "cut inside the state");
        CHECK(!HaParseState("[1,2,3]", &s), "array");
    }
    std::printf("== voice\n");
    {
        auto sensors = HaParseSensors("temperatura salon=sensor.salon_temp;temperatura=sensor.casa_temp;humedad=sensor.hum;puerta=binary_sensor.puerta");
        HaSensor m;
        CHECK(ParseHaSensorQuery("Chipo, ¿qué temperatura hay en el salón?", sensors, &m) && m.entity_id == "sensor.salon_temp", "longest alias wins");
        CHECK(ParseHaSensorQuery("qué temperatura hace", sensors, &m) && m.entity_id == "sensor.casa_temp", "generic temperature");
        CHECK(ParseHaSensorQuery("cuánta humedad hay", sensors, &m) && m.entity_id == "sensor.hum", "humidity");
        CHECK(ParseHaSensorQuery("¿está abierta la puerta?", sensors, &m) && m.entity_id == "binary_sensor.puerta", "door");
        CHECK(!ParseHaSensorQuery("la temperatura", sensors, &m), "not a question");
        CHECK(!ParseHaSensorQuery("enciende la luz de la puerta", sensors, &m), "control phrases are not queries");
        CHECK(!ParseHaSensorQuery("qué tiempo hace hoy", sensors, &m), "weather is not ours");
        CHECK(!ParseHaSensorQuery("dime la presión", sensors, &m), "unknown sensor");
        CHECK(!ParseHaSensorQuery("qué temperatura hay", {}, &m), "no sensors configured");
        CHECK(!ParseHaSensorQuery(nullptr, sensors, &m), "null");
    }
    if (g_fail == 0) std::printf("ALL HA STATE TESTS PASSED\n");
    return g_fail == 0 ? 0 : 1;
}
