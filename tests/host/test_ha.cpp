// ChipoHa controller on a real LVGL timer: unconfigured = off, configured = async call + result text.
#include <cstdio>
#include <string>
#include <vector>

#include "chipo_ha.h"

static int g_failures = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_failures; std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static uint32_t g_now = 1000;
static uint32_t FakeTick() { return g_now; }
static uint8_t g_buf[368 * 40 * 4];
static void Flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

struct Rig {
    ChipoHa ha;
    std::vector<std::string> clips, notes, urls, bodies, tokens;
    bool post_ok = true;
    bool get_ok = true;
    std::string get_body = "{\"state\":\"21.5\",\"attributes\":{\"unit_of_measurement\":\"\\u00b0C\"}}";
    std::vector<std::string> get_urls;
    std::vector<std::function<void()>> pending;  // deferred jobs: the worker "runs" when the test says so

    ChipoHa::Hooks Hooks() {
        ChipoHa::Hooks h;
        h.play_clip = [this](const char* id) { clips.push_back(id); return true; };
        h.notify = [this](const char* t) { notes.push_back(t); };
        h.post = [this](const HaRequest& r, const std::string& token) { urls.push_back(r.url); bodies.push_back(r.body); tokens.push_back(token); return post_ok; };
        h.get = [this](const HaRequest& r, const std::string& token, std::string* body) { get_urls.push_back(r.url); tokens.push_back(token); *body = get_body; return get_ok; };
        h.run_async = [this](std::function<void()> job) { pending.push_back(std::move(job)); };
        return h;
    }
    void RunWorker() { auto jobs = std::move(pending); pending.clear(); for (auto& j : jobs) j(); }
    void Tick() { g_now += 300; lv_timer_handler(); }
};

int main() {
    lv_init();
    lv_tick_set_cb(FakeTick);
    lv_display_t* d = lv_display_create(368, 448);
    lv_display_set_buffers(d, g_buf, nullptr, sizeof(g_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, Flush);

    std::printf("== not configured: everything off\n");
    {
        Rig r;
        CHECK(r.ha.Start(HaConfig{}, r.Hooks()), "starts");
        CHECK(!r.ha.configured(), "off");
        CHECK(!r.ha.OnVoiceText("enciende la luz del salón"), "the assistant keeps the sentence");
        r.ha.RunScene("night");
        CHECK(r.pending.empty() && r.clips.empty() && r.notes.empty(), "no network, no sound");
    }

    HaConfig cfg;
    cfg.base_url = "http://ha.local:8123";
    cfg.token = "secret-token";
    cfg.scene_night = "scene.buenas_noches";

    std::printf("== configured: light, scene, result text\n");
    {
        Rig r;
        r.ha.Start(cfg, r.Hooks());
        CHECK(r.ha.configured(), "on");
        CHECK(!r.ha.OnVoiceText("qué tiempo hace"), "ordinary speech untouched");
        CHECK(r.ha.OnVoiceText("Chipo, enciende la luz del salón"), "light command handled with a chime");
        CHECK(r.pending.size() == 1 && r.urls.empty(), "the call is queued for the worker, not made on the UI task");
        r.Tick();
        CHECK(r.notes.empty(), "no result yet");
        r.RunWorker();
        CHECK(r.urls.size() == 1 && r.urls[0] == "http://ha.local:8123/api/services/light/turn_on" && r.bodies[0] == "{\"entity_id\":\"light.salon\"}", "request (%s)", r.urls.empty() ? "-" : r.urls[0].c_str());
        CHECK(r.tokens[0] == "secret-token", "bearer token passed to the client only");
        r.Tick();
        CHECK(r.notes.size() == 1 && r.notes[0] == "Luz salon encendida", "result shown (%s)", r.notes.empty() ? "-" : r.notes[0].c_str());

        // The token never appears in any text shown.
        for (const auto& n : r.notes) CHECK(n.find("secret") == std::string::npos, "token leaked in text");

        r.post_ok = false;
        CHECK(r.ha.OnVoiceText("apaga la luz de la cocina"), "second command");
        r.RunWorker();
        r.Tick();
        CHECK(r.notes.size() == 2 && r.notes[1].find("no responde") != std::string::npos, "failure text (%s)", r.notes.back().c_str());

        r.post_ok = true;
        CHECK(r.ha.OnVoiceText("activa la escena cine"), "scene");
        CHECK(!r.ha.OnVoiceText("apaga todas las luces"), "a call in flight: the second one is refused, not queued");
        CHECK(r.notes.back() == "Home Assistant ocupado", "busy text");
        r.RunWorker();
        r.Tick();
        CHECK(r.urls.back() == "http://ha.local:8123/api/services/scene/turn_on", "scene url");

        r.ha.RunScene("night");
        r.RunWorker();
        CHECK(r.bodies.back() == "{\"entity_id\":\"scene.buenas_noches\"}", "night scene entity");
        r.Tick();
        const size_t calls = r.urls.size();
        r.ha.RunScene("morning");  // no morning scene configured
        CHECK(r.pending.empty() && r.urls.size() == calls, "no morning scene: nothing happens");
    }

    std::printf("== reading a sensor\n");
    {
        HaConfig c2 = cfg;
        c2.sensors = "temperatura salon=sensor.salon_temp;puerta=binary_sensor.puerta";
        Rig r;
        r.ha.Start(c2, r.Hooks());
        CHECK(r.ha.OnVoiceText("Chipo, ¿qué temperatura hay en el salón?"), "handled with a chime");
        CHECK(r.pending.size() == 1 && r.get_urls.empty(), "read is queued for the worker");
        r.RunWorker();
        CHECK(r.get_urls.size() == 1 && r.get_urls[0] == "http://ha.local:8123/api/states/sensor.salon_temp", "GET url (%s)", r.get_urls.empty() ? "-" : r.get_urls[0].c_str());
        r.Tick();
        CHECK(r.notes.size() == 1 && r.notes[0] == "Temperatura salon: 21.5 grados", "value shown (%s)", r.notes.empty() ? "-" : r.notes[0].c_str());
        CHECK(!r.ha.OnVoiceText("qué tiempo hace"), "weather is left to the weather module");
        CHECK(!r.ha.OnVoiceText("dime la presión"), "unknown sensor: the assistant answers");

        r.get_ok = false;
        r.ha.OnVoiceText("está abierta la puerta");
        r.RunWorker();
        r.Tick();
        CHECK(r.notes.back() == "Home Assistant: no puedo leer puerta", "failure text (%s)", r.notes.back().c_str());

        r.get_ok = true;
        r.get_body = "{\"message\":\"Entity not found.\"}";
        r.ha.OnVoiceText("está abierta la puerta");
        r.RunWorker();
        r.Tick();
        CHECK(r.notes.back() == "Home Assistant: no puedo leer puerta", "unknown entity text");
        for (const auto& n : r.notes) CHECK(n.find("secret") == std::string::npos, "token leaked in text");

        // Without a GET hook (older glue) nothing is read.
        Rig r2;
        auto h = r2.Hooks();
        h.get = nullptr;
        r2.ha.Start(c2, std::move(h));
        CHECK(!r2.ha.OnVoiceText("qué temperatura hay en el salón"), "no GET hook: not handled");
    }

    std::printf("== stopped while a call is running\n");
    {
        Rig r;
        r.ha.Start(cfg, r.Hooks());
        r.ha.OnVoiceText("enciende la luz del salón");
        r.ha.Stop();
        r.RunWorker();  // the worker finishes after Stop: must not touch anything freed (ASan)
        r.Tick();
        CHECK(!r.ha.configured(), "stopped");
    }

    if (g_failures == 0) std::printf("ALL HA TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
