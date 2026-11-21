// CHIPO TOOLS on a real LVGL build: info strip, pomodoro ring, level, 8-ball, canvas, subtitles. Simulated time.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "chipo_tools.h"

static int g_failures = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_failures; std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static uint32_t g_now = 1000;
static uint32_t FakeTick() { return g_now; }
static uint8_t g_buf[480 * 40 * 4];
static void Flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

struct Rig {
    ChipoTools tools;
    std::vector<std::string> clips, notes, emotions, saved;
    bool have_time = true, have_temp = true, calm = true, other = false, pomo = true, clips_exist = true, accents = false;
    int hour = 9, minute = 41, temp = 21;
    float pomo_left = 0.6f;
    bool pomo_work = true;
    int shakes_forwarded = 0, prefetches = 0;
    bool Start() {
        ChipoTools::Hooks h;
        h.wall_time = [this](int* hh, int* mm) { if (!have_time) return false; *hh = hour; *mm = minute; return true; };
        h.temperature = [this](int* t) { if (!have_temp) return false; *t = temp; return true; };
        h.refresh_weather = [this] { ++prefetches; };
        h.pomodoro = [this](float* f, bool* w) { if (!pomo) return false; *f = pomo_left; *w = pomo_work; return true; };
        h.face_calm = [this] { return calm; };
        h.other_screen = [this] { return other; };
        h.emotion = [this](const char* e, uint32_t) { emotions.push_back(e); };
        h.play_clip = [this](const char* id) { if (!clips_exist) return false; clips.push_back(id); return true; };
        h.notify = [this](const char* t) { notes.push_back(t); };
        h.shake_other = [this](uint32_t) { ++shakes_forwarded; return true; };
        if (accents) h.text_font = [] { return &lv_font_montserrat_24; };
        h.load_flag = [](const char*, bool def) { return def; };
        h.save_flag = [this](const char* k, bool v) { saved.push_back(std::string(k) + (v ? "=1" : "=0")); };
        h.clock = [] { return g_now; };
        return tools.Start(lv_screen_active(), std::move(h));
    }
    void Run(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 50) { g_now += 50; tools.Step(g_now); lv_timer_handler(); } }
    bool Played(const char* id) const { for (auto& c : clips) if (c == id) return true; return false; }
    // Feeds an accelerometer reading for `ms` at 50 Hz.
    void Accel(float x, float y, float z, uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += 20) { g_now += 20; ImuSample s; s.x = x; s.y = y; s.z = z; tools.OnSample(s, g_now); }
    }
    void ShakeIt() {
        for (int i = 0; i < 14; ++i) { g_now += 100; ImuSample s; s.x = 0; s.y = 0; s.z = (i % 2) ? 0.1f : 2.3f; tools.OnSample(s, g_now); }
    }
};

static bool ValidUtf8(const char* s) {
    const auto* p = reinterpret_cast<const unsigned char*>(s);
    while (*p) {
        int n = *p < 0x80 ? 1 : (*p >> 5) == 6 ? 2 : (*p >> 4) == 14 ? 3 : (*p >> 3) == 30 ? 4 : 0;
        if (!n) return false;
        for (int i = 1; i < n; ++i) if ((p[i] & 0xC0) != 0x80) return false;
        p += n;
    }
    return true;
}

int main() {
    lv_init();
    lv_tick_set_cb(FakeTick);
    lv_rand_set_seed(77);
    lv_display_t* d = lv_display_create(480, 480);
    lv_display_set_buffers(d, g_buf, nullptr, sizeof(g_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, Flush);

    std::printf("== info strip\n");
    {
        Rig r;
        CHECK(r.Start(), "start");
        auto dbg = r.tools.Debug();
        CHECK(!dbg.strip_visible && !dbg.ring_visible && dbg.tool == ChipoTools::Tool::None, "nothing at first");
        r.Run(1500);
        dbg = r.tools.Debug();
        CHECK(dbg.strip_visible && std::strcmp(dbg.time_text, "09:41") == 0, "time shown: '%s'", dbg.time_text);
        CHECK(dbg.temp_visible && dbg.temp_c == 21, "temperature shown");
        r.minute = 5; r.hour = 23;
        r.Run(700);
        CHECK(std::strcmp(r.tools.Debug().time_text, "23:05") == 0, "time follows the clock: '%s'", r.tools.Debug().time_text);
        r.calm = false;
        r.Run(1500);
        CHECK(!r.tools.Debug().strip_visible && !r.tools.Debug().temp_visible, "hidden while CHIPO is busy");
        r.calm = true;
        r.Run(1500);
        CHECK(r.tools.Debug().strip_visible, "back when calm");
        r.other = true;
        r.Run(1500);
        CHECK(!r.tools.Debug().strip_visible, "hidden under another screen");
        r.other = false;
        r.Run(1500);
        r.have_time = false;
        r.Run(1500);
        CHECK(!r.tools.Debug().strip_visible && r.tools.Debug().time_text[0] == '\0', "no clock -> no time is invented");
        r.have_time = true;
        r.have_temp = false;
        r.Run(1500);
        CHECK(r.tools.Debug().strip_visible && !r.tools.Debug().temp_visible, "time without temperature when no weather");
        r.Run(20000);
        CHECK(r.prefetches == 1, "the weather is fetched quietly once after start-up (%d)", r.prefetches);
        CHECK(r.tools.OnVoiceText("oculta la información") && r.saved.back() == "tools_info=0", "voice hides the info and saves it");
        r.have_temp = true;
        r.Run(1500);
        CHECK(!r.tools.Debug().strip_visible, "info off");
        CHECK(r.tools.OnVoiceText("muestra la información") && r.saved.back() == "tools_info=1", "voice shows it again");
        r.Run(1500);
        CHECK(r.tools.Debug().strip_visible, "info on again");
        r.tools.Stop();
    }

    std::printf("== pomodoro ring\n");
    {
        Rig r;
        r.Start();
        r.Run(1500);
        auto dbg = r.tools.Debug();
        CHECK(dbg.ring_visible && dbg.ring_value == 600 && dbg.ring_work, "ring at 60%% work: %d", dbg.ring_value);
        r.pomo_left = 0.25f; r.pomo_work = false;
        r.Run(1200);
        dbg = r.tools.Debug();
        CHECK(dbg.ring_value == 250 && !dbg.ring_work, "break ring at 25%%: %d", dbg.ring_value);
        r.other = true;
        r.Run(1500);
        CHECK(!r.tools.Debug().ring_visible, "ring hidden under another screen");
        r.other = false;
        r.pomo = false;
        r.Run(1500);
        CHECK(!r.tools.Debug().ring_visible, "no pomodoro -> no ring");
        r.tools.Stop();
    }

    std::printf("== level\n");
    {
        Rig r;
        r.Start();
        CHECK(r.tools.OnVoiceText("pon el nivel") && r.tools.tool() == ChipoTools::Tool::Level && r.tools.screen_shown(), "level opens by voice");
        r.Accel(0, 1, 0, 400);  // standing up
        r.Run(200);
        CHECK(!r.tools.Debug().level_flat, "standing up is not flat");
        const float s = std::sin(4.0f * 0.0174533f), c = std::cos(4.0f * 0.0174533f);
        r.Accel(s, 0, c, 800);
        r.Run(600);
        auto dbg = r.tools.Debug();
        CHECK(dbg.level_flat && !dbg.level_ok && dbg.bubble_x > 20 && std::fabs(dbg.bubble_y) < 2 && std::fabs(dbg.angle_deg - 4.0f) < 0.3f,
              "4 degrees: bubble %.1f,%.1f angle %.2f", dbg.bubble_x, dbg.bubble_y, dbg.angle_deg);
        r.Accel(0.002f, -0.003f, 1.0f, 1200);
        r.Run(800);
        dbg = r.tools.Debug();
        CHECK(dbg.level_ok && std::fabs(dbg.bubble_x) < 3 && std::fabs(dbg.bubble_y) < 3, "level: %.1f,%.1f ok=%d", dbg.bubble_x, dbg.bubble_y, dbg.level_ok);
        CHECK(r.tools.OnVoiceText("cierra") && r.tools.tool() == ChipoTools::Tool::None && !r.tools.screen_shown(), "voice closes it");
        CHECK(!r.tools.OnVoiceText("cuál es el nivel de batería"), "battery level is not ours");
        r.tools.Stop();
    }

    std::printf("== magic 8-ball and shaking\n");
    {
        Rig r;
        r.Start();
        r.ShakeIt();
        CHECK(r.shakes_forwarded == 1, "a shake on the face goes to the dice/coin (%d)", r.shakes_forwarded);
        CHECK(r.tools.OnVoiceText("bola mágica") && r.tools.tool() == ChipoTools::Tool::Ball, "8-ball opens");
        CHECK(r.tools.Debug().ball_phase == 0, "waiting for the question");
        r.Accel(0, 0, 1, 3000);  // quiet: the cooldown of the earlier shake is over
        r.ShakeIt();
        auto dbg = r.tools.Debug();
        CHECK(dbg.ball_phase == 1 && r.shakes_forwarded == 1, "a shake asks the ball and is not forwarded (phase %d)", dbg.ball_phase);
        r.Run(1300);
        dbg = r.tools.Debug();
        CHECK(dbg.ball_phase == 2 && dbg.ball_index >= 0 && dbg.ball_index < EightBallCount(), "answer shown: %d", dbg.ball_index);
        CHECK(!r.emotions.empty(), "the face reacts");
        const int mood = EightBallMood(dbg.ball_index);
        CHECK(r.emotions.back() == (mood > 0 ? "happy" : mood < 0 ? "sad" : "confused"), "reaction matches the answer: %s", r.emotions.back().c_str());
        const int first = dbg.ball_index;
        r.tools.AskBall(g_now);
        r.Run(1300);
        CHECK(r.tools.Debug().ball_index != first, "a second question gives another answer");
        r.tools.Close();
        CHECK(r.tools.tool() == ChipoTools::Tool::None, "closed");
        r.tools.Stop();
    }

    std::printf("== canvas\n");
    {
        Rig r;
        r.Start();
        CHECK(r.tools.OnVoiceText("vamos a dibujar") && r.tools.tool() == ChipoTools::Tool::Canvas, "canvas opens");
        r.tools.PenMove(10, 10);
        CHECK(r.tools.Debug().canvas_dots == 0, "moving without pressing draws nothing");
        r.tools.PenDown(100, 100);
        r.tools.PenMove(200, 140);
        r.tools.PenUp();
        const long dots = r.tools.Debug().canvas_dots;
        CHECK(dots > 500, "a stroke draws pixels: %ld", dots);
        r.tools.PenDown(-50, 700);  // off screen: must not crash or write
        r.tools.PenMove(900, -20);
        r.tools.PenUp();
        r.Run(300);
        CHECK(r.tools.OnVoiceText("borra") && r.tools.Debug().canvas_dots == 0, "voice clears it");
        CHECK(r.tools.OnVoiceText("cierra") && r.tools.tool() == ChipoTools::Tool::None, "voice closes it");
        r.tools.PenDown(5, 5);
        CHECK(r.tools.Debug().canvas_dots == 0, "no pen when closed");
        r.tools.OnVoiceText("pizarra");
        r.tools.Close();
        r.tools.OnVoiceText("lienzo");
        r.tools.Close();
        r.tools.Stop();
    }

    std::printf("== hardware key cycles the tools\n");
    {
        Rig r;
        r.Start();
        r.tools.OnKey();
        CHECK(r.tools.tool() == ChipoTools::Tool::Level, "key 1: level");
        r.tools.OnKey();
        CHECK(r.tools.tool() == ChipoTools::Tool::Ball, "key 2: 8-ball");
        r.tools.OnKey();
        CHECK(r.tools.tool() == ChipoTools::Tool::Canvas, "key 3: canvas");
        r.tools.OnKey();
        CHECK(r.tools.tool() == ChipoTools::Tool::None, "key 4: closed");
        r.tools.Stop();
    }

    std::printf("== subtitles\n");
    {
        Rig r;
        r.Start();
        r.tools.ShowSubtitle("Hola, ¿cómo estás?");
        CHECK(!r.tools.Debug().subtitle_visible, "off by default");
        CHECK(r.tools.OnVoiceText("activa los subtítulos") && r.tools.subtitles_enabled() && r.saved.back() == "tools_subs=1", "voice turns them on");
        r.tools.ShowSubtitle("Hola, ¿cómo estás? Mañana habrá sol");
        auto dbg = r.tools.Debug();
        CHECK(dbg.subtitle_visible && std::strcmp(dbg.subtitle_text, "Hola, como estas? Manana habra sol") == 0, "accents folded without a font: '%s'", dbg.subtitle_text);
        r.Run(2000);
        CHECK(r.tools.Debug().subtitle_visible, "still there");
        r.Run(5000);
        CHECK(!r.tools.Debug().subtitle_visible, "fades away");
        std::string longtext;
        for (int i = 0; i < 60; ++i) longtext += "año ";
        r.tools.ShowSubtitle(longtext.c_str());
        CHECK(ValidUtf8(r.tools.Debug().subtitle_text), "long text cut on a character boundary");
        CHECK(r.tools.OnVoiceText("quita los subtítulos") && !r.tools.subtitles_enabled(), "voice turns them off");
        r.Run(1500);
        CHECK(!r.tools.Debug().subtitle_visible, "hidden when switched off");
        r.tools.Stop();
        Rig a;
        a.accents = true;
        a.Start();
        a.tools.SetSubtitlesEnabled(true);
        a.tools.ShowSubtitle("¿Qué tal, señor?");
        CHECK(std::strcmp(a.tools.Debug().subtitle_text, "¿Qué tal, señor?") == 0, "accents kept with a font that has them");
        a.tools.Stop();
    }

    std::printf("== voice that must pass through\n");
    {
        Rig r;
        r.Start();
        CHECK(!r.tools.OnVoiceText("qué hora es") && !r.tools.OnVoiceText("qué tiempo hace") && !r.tools.OnVoiceText("") && !r.tools.OnVoiceText(nullptr), "not ours");
        r.clips_exist = false;
        CHECK(!r.tools.OnVoiceText("pon el nivel"), "without the sound the server still answers");
        CHECK(r.tools.tool() == ChipoTools::Tool::Level, "but the level opened");
        r.tools.Stop();
        r.tools.Stop();  // twice is fine
    }

    if (g_failures == 0) std::printf("ALL TOOLS TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
