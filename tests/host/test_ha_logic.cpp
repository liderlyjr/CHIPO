// Home Assistant helpers: request building, name slugs, spoken commands (no network, no LVGL).
#include <cstdio>

#include "chipo_ha_logic.h"

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void Expect(const char* text, HaIntentKind k, const char* entity) {
    const HaIntent i = ParseHaIntent(text);
    CHECK(i.kind == k && i.entity_id == entity, "\"%s\" -> %d \"%s\" (want %d \"%s\")", text, static_cast<int>(i.kind), i.entity_id.c_str(), static_cast<int>(k), entity);
}

int main() {
    std::printf("== slugs and ids\n");
    CHECK(HaSlug("Salón de abajo") == "salon_de_abajo", "slug 1 (%s)", HaSlug("Salón de abajo").c_str());
    CHECK(HaSlug("  Cocina!! ") == "cocina", "slug 2");
    CHECK(HaSlug("") == "", "slug empty");
    CHECK(HaValidEntityId("scene.cine") && HaValidEntityId("light.salon_2"), "valid ids");
    CHECK(!HaValidEntityId("scene") && !HaValidEntityId(".x") && !HaValidEntityId("a.") && !HaValidEntityId("a.b.c") &&
          !HaValidEntityId("Scene.x") && !HaValidEntityId("light.sal\"on") && !HaValidEntityId("light.a b"), "invalid ids");

    std::printf("== base url\n");
    std::string u;
    CHECK(HaNormalizeBaseUrl(" http://homeassistant.local:8123/ ", &u) && u == "http://homeassistant.local:8123", "trim (%s)", u.c_str());
    CHECK(HaNormalizeBaseUrl("https://casa.example.com", &u) && u == "https://casa.example.com", "https");
    CHECK(!HaNormalizeBaseUrl("ftp://x", &u) && !HaNormalizeBaseUrl("homeassistant.local", &u) && !HaNormalizeBaseUrl("http://", &u) &&
          !HaNormalizeBaseUrl("http://a b", &u) && !HaNormalizeBaseUrl("http://a\"b", &u), "rejected urls");

    std::printf("== requests\n");
    HaConfig cfg;
    CHECK(!cfg.usable(), "empty config is off");
    cfg.base_url = "http://ha.local:8123/";
    CHECK(!cfg.usable(), "no token: still off");
    cfg.token = "x";
    CHECK(cfg.usable(), "address + token: usable");
    HaRequest r;
    CHECK(HaBuildServiceCall(cfg, "scene", "turn_on", "scene.cine", &r), "scene call");
    CHECK(r.url == "http://ha.local:8123/api/services/scene/turn_on" && r.body == "{\"entity_id\":\"scene.cine\"}", "scene request (%s %s)", r.url.c_str(), r.body.c_str());
    CHECK(HaBuildServiceCall(cfg, "light", "turn_off", "all", &r) && r.body == "{\"entity_id\":\"all\"}", "all lights");
    CHECK(!HaBuildServiceCall(cfg, "scene", "turn_on", "scene.x\"}", &r), "entity injection refused");
    CHECK(!HaBuildServiceCall(cfg, "scene/../x", "turn_on", "scene.x", &r), "path injection refused");
    CHECK(!HaBuildServiceCall(cfg, "scene", "", "scene.x", &r), "empty service refused");
    HaConfig bad = cfg; bad.base_url = "nonsense";
    CHECK(!HaBuildServiceCall(bad, "scene", "turn_on", "scene.x", &r), "bad address refused");

    std::printf("== spoken commands\n");
    Expect("activa la escena cine", HaIntentKind::Scene, "scene.cine");
    Expect("Chipo, pon la escena de cena", HaIntentKind::Scene, "scene.cena");
    Expect("enciende la luz del salón", HaIntentKind::LightOn, "light.salon");
    Expect("Chipo, enciende la luz de la cocina", HaIntentKind::LightOn, "light.cocina");
    Expect("apaga la luz del salón de abajo", HaIntentKind::LightOff, "light.salon_de_abajo");
    Expect("apaga todas las luces", HaIntentKind::AllLightsOff, "all");
    Expect("apaga las luces", HaIntentKind::AllLightsOff, "all");
    Expect("qué tiempo hace", HaIntentKind::None, "");
    Expect("enciende la tele", HaIntentKind::None, "");
    Expect("la luz", HaIntentKind::None, "");
    Expect("enciende la luz", HaIntentKind::None, "");
    Expect("cuéntame por qué se apaga la luz del salón cuando llueve mucho en invierno", HaIntentKind::None, "");
    Expect("", HaIntentKind::None, "");
    Expect(nullptr, HaIntentKind::None, "");

    if (g_fail == 0) std::printf("ALL HA LOGIC TESTS PASSED\n");
    return g_fail == 0 ? 0 : 1;
}
