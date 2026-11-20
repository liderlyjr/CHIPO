// Developer tool (not a test): renders CHIPO TOOLS screens to PPM next to the face. Usage: render_tools <out_dir>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "chipo_face.h"
#include "chipo_tools.h"

static uint32_t g_now = 1000;
static uint32_t FakeTick() { return g_now; }
static std::vector<uint8_t> g_fb(368 * 448 * 2);
static void Flush(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
    const int w = a->x2 - a->x1 + 1;
    for (int y = a->y1; y <= a->y2; ++y)
        for (int x = a->x1; x <= a->x2; ++x) {
            const uint8_t* s = px + ((y - a->y1) * w + (x - a->x1)) * 2;
            g_fb[(y * 368 + x) * 2] = s[0];
            g_fb[(y * 368 + x) * 2 + 1] = s[1];
        }
    lv_display_flush_ready(d);
}
static void Save(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "wb");
    std::fprintf(f, "P6\n368 448\n255\n");
    for (int i = 0; i < 368 * 448; ++i) {
        const uint16_t v = g_fb[i * 2] | (g_fb[i * 2 + 1] << 8);
        const uint8_t rgb[3] = {static_cast<uint8_t>(((v >> 11) & 31) * 255 / 31), static_cast<uint8_t>(((v >> 5) & 63) * 255 / 63), static_cast<uint8_t>((v & 31) * 255 / 31)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
}
static std::string g_out;
static ChipoFace g_face;
static ChipoTools g_tools;
static void Run(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 40) { g_now += 40; g_face.Step(g_now); g_tools.Step(g_now); lv_timer_handler(); }
}
static void Shot(const char* name) { lv_refr_now(nullptr); Save(g_out + "/" + name + ".ppm"); }
static void Accel(float x, float y, float z) {
    for (int i = 0; i < 40; ++i) { g_now += 20; ImuSample s; s.x = x; s.y = y; s.z = z; g_tools.OnSample(s, g_now); }
}

int main(int argc, char** argv) {
    g_out = argc > 1 ? argv[1] : ".";
    lv_init();
    lv_tick_set_cb(FakeTick);
    static std::vector<uint8_t> buf(368 * 448 * 2);
    lv_display_t* d = lv_display_create(368, 448);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, buf.data(), nullptr, buf.size(), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(d, Flush);
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);

    g_face.SetIdleQuirks(false);
    g_face.Start(lv_screen_active());

    bool pomodoro = false;
    ChipoTools::Hooks h;
    h.wall_time = [](int* hh, int* mm) { *hh = 9; *mm = 41; return true; };
    h.temperature = [](int* t) { *t = 21; return true; };
    h.pomodoro = [&](float* f, bool* w) { if (!pomodoro) return false; *f = 0.62f; *w = true; return true; };
    h.face_calm = [] { return g_face.GetState() == ChipoFace::State::Idle; };
    h.emotion = [](const char* e, uint32_t ms) { g_face.ShowEmotionByName(e, ms); };
    h.play_clip = [](const char*) { return true; };
    h.notify = [](const char*) {};
    h.load_flag = [](const char*, bool def) { return def; };
    h.save_flag = [](const char*, bool) {};
    h.clock = [] { return g_now; };
    g_tools.Start(lv_screen_active(), std::move(h));

    Run(2500);
    Shot("tools_info");
    pomodoro = true;
    Run(2500);
    Shot("tools_pomodoro");
    pomodoro = false;

    // Subtitles under a speaking face.
    g_tools.SetSubtitlesEnabled(true);
    g_face.SetState(ChipoFace::State::Speaking);
    g_tools.ShowSubtitle("Hola, ¿qué tal? Hoy hará sol y unos veintiún grados en Madrid.");
    Run(1500);
    Shot("tools_subtitles");
    g_tools.SetSubtitlesEnabled(false);
    g_face.SetState(ChipoFace::State::Idle);
    Run(3000);

    // Level: tilted 4 degrees, then flat.
    g_tools.OnVoiceText("pon el nivel");
    const float s = std::sin(4.0f * 0.0174533f), c = std::cos(4.0f * 0.0174533f);
    Accel(s * 0.8f, -s * 0.6f, c);
    Run(1200);
    Shot("tools_level_tilt");
    Accel(0.001f, 0.0f, 1.0f);
    Accel(0.001f, 0.0f, 1.0f);
    Run(1200);
    Shot("tools_level_ok");
    Accel(0, 1, 0);
    Accel(0, 1, 0);
    Run(500);
    Shot("tools_level_upright");
    g_tools.Close();

    // 8-ball.
    g_tools.OnVoiceText("bola mágica");
    Run(600);
    Shot("tools_ball_wait");
    g_tools.AskBall(g_now);
    Run(500);
    Shot("tools_ball_thinking");
    Run(1500);
    Shot("tools_ball_answer");
    g_tools.Close();

    // Canvas.
    g_tools.OnVoiceText("vamos a dibujar");
    Run(300);
    const int W = 368, H = 448;
    auto stroke = [&](const std::vector<std::pair<int, int>>& pts) {
        g_tools.PenDown(pts[0].first, pts[0].second);
        for (size_t i = 1; i < pts.size(); ++i) g_tools.PenMove(pts[i].first, pts[i].second);
        g_tools.PenUp();
    };
    std::vector<std::pair<int, int>> spiral, wave, smile;
    for (int i = 0; i < 160; ++i) { const float a = i * 0.16f, r = 12 + i * 0.9f; spiral.push_back({W / 2 + int(std::cos(a) * r), H / 2 - 20 + int(std::sin(a) * r)}); }
    for (int i = 0; i <= 60; ++i) wave.push_back({40 + i * (W - 80) / 60, 110 + int(std::sin(i * 0.3f) * 22)});
    for (int i = 0; i <= 30; ++i) { const float a = 0.3f + i * (2.54f / 30); smile.push_back({W / 2 + int(std::cos(a) * 70), H - 150 + int(std::sin(a) * 40)}); }
    stroke(spiral);
    stroke(wave);
    stroke(smile);
    stroke({{60, 330}, {120, 380}, {200, 340}, {300, 390}});
    Run(300);
    Shot("tools_canvas");
    g_tools.Close();
    g_tools.Stop();
    return 0;
}
