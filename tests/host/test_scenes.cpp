// ChipoScenes on a real LVGL build: voice, screens, audio-driven bars, auto-off, touch.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "chipo_scenes.h"

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++g_failures; std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static uint32_t g_now = 1000;
static uint32_t FakeTick() { return g_now; }
static uint8_t g_buf[480 * 40 * 4];
static void Flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

struct Rig {
    ChipoScenes s;
    std::vector<std::string> notes, clips;
    bool dancing = false;
    bool audio_on = false;
    float band[8] = {};
    int dance_calls = 0;
    bool Start() {
        ChipoScenes::Hooks h;
        h.audio = [this](float* b, float* level) {
            if (!audio_on) return false;
            for (int i = 0; i < 8; ++i) b[i] = band[i];
            *level = 0.5f;
            return true;
        };
        h.notify = [this](const char* t) { notes.push_back(t); };
        h.play_clip = [this](const char* id) { clips.push_back(id); return true; };
        h.dance = [this](bool on) { dancing = on; ++dance_calls; };
        h.dancing = [this] { return dancing; };
        h.clock = [] { return g_now; };
        return s.Start(lv_screen_active(), std::move(h));
    }
    void Run(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 40) { g_now += 40; s.Step(g_now); } lv_timer_handler(); }
};

int main() {
    lv_init();
    lv_tick_set_cb(FakeTick);
    lv_display_t* d = lv_display_create(480, 480);
    lv_display_set_buffers(d, g_buf, nullptr, sizeof(g_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, Flush);

    std::printf("== visualizer\n");
    {
        Rig r; CHECK(r.Start(), "start");
        CHECK(!r.s.screen_shown(), "hidden at first");
        CHECK(r.s.OnVoiceText("pon el visualizador") && r.s.shown() == SceneKind::Visualizer, "opens by voice");
        CHECK(!r.clips.empty() && !r.notes.empty(), "sound and text");
        r.Run(1000);
        const int idle_h = r.s.Debug().bars_px[0];
        CHECK(idle_h >= 4 && idle_h < 60, "idle bars are low (%d)", idle_h);
        r.audio_on = true;
        r.band[0] = 1.0f; r.band[3] = 0.5f;
        r.Run(600);
        const auto dbg = r.s.Debug();
        CHECK(dbg.bars_px[0] > 240 && dbg.bars_px[3] > 100 && dbg.bars_px[7] < 60, "bars follow the sound (%d %d %d)", dbg.bars_px[0], dbg.bars_px[3], dbg.bars_px[7]);
        CHECK(dbg.visible_objects == 16, "8 bars and 8 caps (%d)", dbg.visible_objects);
        r.s.OnTouch();
        CHECK(!r.s.screen_shown(), "a touch closes it");
        CHECK(r.s.Debug().visible_objects == 0, "nothing left visible");
    }
    std::printf("== aquarium and lava\n");
    {
        Rig r; r.Start();
        CHECK(r.s.OnVoiceText("muéstrame el acuario") && r.s.shown() == SceneKind::Aquarium, "aquarium");
        r.Run(5000);
        CHECK(r.s.Debug().visible_objects == 4 * 3 + 6 + 3, "fish, bubbles and plants (%d)", r.s.Debug().visible_objects);
        CHECK(r.s.OnVoiceText("enciende la lámpara de lava") && r.s.shown() == SceneKind::Lava, "switching screens");
        r.Run(5000);
        CHECK(r.s.Debug().visible_objects == 6, "six blobs (%d)", r.s.Debug().visible_objects);
        CHECK(r.s.OnVoiceText("quita eso") && !r.s.screen_shown(), "closed by voice");
        CHECK(!r.s.OnVoiceText("quita eso"), "nothing to close");
        CHECK(!r.s.OnVoiceText("qué tiempo hace"), "other sentences are not ours");
    }
    std::printf("== auto off and dance\n");
    {
        Rig r; r.Start();
        r.s.OnVoiceText("pon el acuario");
        r.Run(9u * 60u * 1000u);
        CHECK(r.s.screen_shown(), "still there after 9 min");
        r.Run(70u * 1000u);
        CHECK(!r.s.screen_shown(), "gone after 10 min");
        CHECK(r.s.OnVoiceText("ponte a bailar") && r.dancing, "dance on");
        CHECK(r.s.OnVoiceText("para de bailar") && !r.dancing, "dance off");
        CHECK(r.dance_calls == 2, "two calls");
        CHECK(!r.s.OnVoiceText("sabes bailar"), "a question goes to the assistant");
        r.s.Stop();
        CHECK(!r.s.OnVoiceText("pon el acuario"), "stopped: nothing");
    }
    if (g_failures == 0) std::printf("ALL SCENES TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
