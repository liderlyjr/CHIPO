#include <cstdio>
#include <string>
#include <vector>

#include "chipo_ambient.h"

static uint32_t g_now = 1000;
static uint32_t FakeTick() { return g_now; }
static uint8_t g_buf[368 * 40 * 2];
static void Flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); ++g_fail; } } while (0)

static AmbientCommand P(const char* t, bool active = false) { return ParseAmbientCommand(t, active); }

struct Rig {
    ChipoAmbient amb;
    std::vector<std::string> clips, notes;
    std::vector<uint32_t> push_times;
    bool exists = true;
    bool busy = false;
    bool Start() {
        ChipoAmbient::Hooks h;
        h.clip_exists = [this](const char*) { return exists; };
        h.play_clip = [this](const char* id) {
            if (!exists) return false;
            clips.push_back(id);
            if (std::string(id).rfind("ambient_", 0) == 0) push_times.push_back(g_now);
            return true;
        };
        h.notify = [this](const char* t) { notes.push_back(t); };
        h.assistant_busy = [this] { return busy; };
        h.clock = [] { return g_now; };
        return amb.Start(std::move(h));
    }
    void Advance(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 50) { g_now += 50; amb.Step(g_now); } }
    int Pieces() const { return static_cast<int>(push_times.size()); }
};

int main() {
    lv_init();
    lv_tick_set_cb(FakeTick);
    lv_display_t* d = lv_display_create(368, 448);
    lv_display_set_buffers(d, g_buf, nullptr, sizeof(g_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, Flush);

    std::printf("== voice\n");
    CHECK(P("pon sonido de lluvia").intent == AmbientIntent::Start && P("pon sonido de lluvia").sound == AmbientSound::Rain, "rain");
    CHECK(P("Chipo, ponme la chimenea").sound == AmbientSound::Fire && P("ponme la chimenea").intent == AmbientIntent::Start, "fireplace");
    CHECK(P("ruido blanco").intent == AmbientIntent::Start && P("ruido blanco").sound == AmbientSound::Noise, "white noise");
    CHECK(P("quiero escuchar la lluvia").intent == AmbientIntent::Start, "listen to rain");
    CHECK(P("va a llover mañana").intent == AmbientIntent::None, "weather talk");
    CHECK(P("hoy hay lluvia").intent == AmbientIntent::None, "not a command");
    CHECK(P("qué tiempo hace").intent == AmbientIntent::None, "weather");
    CHECK(P("activa la escena chimenea").intent == AmbientIntent::None, "Home Assistant scene is not ours");
    CHECK(P("pon música").intent == AmbientIntent::None, "music is not ours");
    CHECK(P("apaga el fuego de la cocina", false).intent == AmbientIntent::None, "not playing: nothing to stop");
    CHECK(P("para la lluvia", true).intent == AmbientIntent::Stop, "stop rain");
    CHECK(P("apaga el sonido", true).intent == AmbientIntent::Stop, "stop sound");
    CHECK(P("quita el ruido", true).intent == AmbientIntent::Stop, "stop noise");
    CHECK(P("para la lluvia", false).intent == AmbientIntent::None, "stop with nothing playing");
    CHECK(P("apaga la luz", true).intent == AmbientIntent::None, "lights are not ours");
    CHECK(P("", true).intent == AmbientIntent::None && P(nullptr, true).intent == AmbientIntent::None, "empty");

    std::printf("== playing in pieces\n");
    {
        Rig r;
        r.Start();
        CHECK(r.amb.OnVoiceText("pon sonido de lluvia") && r.clips.back() == "buddy_chime" && r.notes.back() == "Lluvia", "started with a chime");
        CHECK(r.amb.playing() && r.Pieces() == 0, "nothing queued before the chime is over");
        r.Advance(1300);
        CHECK(r.Pieces() == 1 && r.clips.back() == "ambient_rain_0", "first piece (%s)", r.clips.back().c_str());
        r.Advance(800);
        CHECK(r.Pieces() == 2 && r.clips.back() == "ambient_rain_1", "second piece comes early (%d)", r.Pieces());
        r.Advance(60 * 1000);
        CHECK(r.Pieces() >= 60 && r.Pieces() <= 66, "about one piece per second (%d in 62 s)", r.Pieces());
        // Steady cadence: gaps between pieces after the first two are 960 ms +- one tick.
        int bad = 0;
        for (size_t i = 3; i < r.push_times.size(); ++i) {
            const uint32_t gap = r.push_times[i] - r.push_times[i - 1];
            if (gap < 900 || gap > 1020) ++bad;
        }
        CHECK(bad <= 1, "regular cadence (%d irregular gaps)", bad);
        // Cycles through all 9 pieces in order.
        bool order = true;
        int expect = 0;
        for (const auto& c : r.clips) {
            if (c.rfind("ambient_rain_", 0) != 0) continue;
            if (std::stoi(c.substr(13)) != expect) order = false;
            expect = (expect + 1) % 9;
        }
        CHECK(order, "pieces in loop order");
    }
    std::printf("== pauses for the assistant, stops\n");
    {
        Rig r;
        r.Start();
        r.amb.OnVoiceText("pon la chimenea");
        r.Advance(5000);
        const int before = r.Pieces();
        r.busy = true;
        r.Advance(10000);
        CHECK(r.Pieces() == before, "nothing queued while the assistant listens/speaks (%d vs %d)", r.Pieces(), before);
        r.busy = false;
        r.Advance(2000);
        CHECK(r.Pieces() > before && r.Pieces() <= before + 3, "resumes afterwards without a burst (%d -> %d)", before, r.Pieces());
        CHECK(r.amb.OnVoiceText("para el fuego") && !r.amb.playing(), "stopped by voice");
        const int stopped = r.Pieces();
        r.Advance(5000);
        CHECK(r.Pieces() == stopped, "silent after stop");
    }
    std::printf("== switches itself off\n");
    {
        Rig r;
        r.Start();
        r.amb.OnVoiceText("ruido blanco");
        r.Advance(29u * 60u * 1000u);
        CHECK(r.amb.playing(), "still on at 29 min");
        r.Advance(90u * 1000u);
        CHECK(!r.amb.playing() && r.notes.back().find("30 min") != std::string::npos, "off after 30 min (%s)", r.notes.back().c_str());
        r.amb.OnVoiceText("pon sonido de lluvia");
        CHECK(r.amb.playing(), "can start again");
    }
    std::printf("== missing clips / late timer / stopped\n");
    {
        Rig r;
        r.exists = false;
        r.Start();
        CHECK(!r.amb.OnVoiceText("pon sonido de lluvia") && !r.amb.playing() && r.notes.back().find("No tengo") != std::string::npos, "no sound in the firmware: text, assistant answers");

        Rig s;
        s.Start();
        s.amb.OnVoiceText("pon sonido de lluvia");
        s.Advance(3000);
        const int n = s.Pieces();
        g_now += 20000;  // the UI task was blocked for 20 s
        s.amb.Step(g_now);
        s.Advance(1100);
        CHECK(s.Pieces() <= n + 3, "no burst of pieces after a long stall (%d -> %d)", n, s.Pieces());
        s.amb.Stop();
        CHECK(!s.amb.OnVoiceText("pon sonido de lluvia"), "stopped: nothing");
    }
    if (g_fail == 0) std::printf("ALL AMBIENT TESTS PASSED\n");
    return g_fail == 0 ? 0 : 1;
}
