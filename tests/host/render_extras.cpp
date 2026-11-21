// Developer tool (not a test): renders the new animations and screens to PPM. Usage: render_extras <out_dir> [frames_dir]
// With frames_dir it also saves a numbered frame sequence of the dance and of the three scenes (for GIFs).
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "chipo_face.h"
#include "chipo_scenes.h"

static uint32_t g_now = 1000;
static uint32_t FakeTick() { return g_now; }
static std::vector<uint8_t> g_fb(480 * 480 * 2);
static void Flush(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
    const int w = a->x2 - a->x1 + 1;
    for (int y = a->y1; y <= a->y2; ++y)
        for (int x = a->x1; x <= a->x2; ++x) {
            const uint8_t* s = px + ((y - a->y1) * w + (x - a->x1)) * 2;
            g_fb[(y * 480 + x) * 2] = s[0];
            g_fb[(y * 480 + x) * 2 + 1] = s[1];
        }
    lv_display_flush_ready(d);
}
static void Save(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "wb");
    std::fprintf(f, "P6\n480 480\n255\n");
    for (int i = 0; i < 480 * 480; ++i) {
        const uint16_t v = g_fb[i * 2] | (g_fb[i * 2 + 1] << 8);
        const uint8_t rgb[3] = {static_cast<uint8_t>(((v >> 11) & 31) * 255 / 31), static_cast<uint8_t>(((v >> 5) & 63) * 255 / 63), static_cast<uint8_t>((v & 31) * 255 / 31)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
}
static std::string g_out, g_frames;
static void Run(uint32_t ms, const std::function<void()>& each = nullptr) {
    for (uint32_t t = 0; t < ms; t += 40) { g_now += 40; if (each) each(); lv_timer_handler(); }
}
static void Shot(const char* name) { lv_refr_now(nullptr); Save(g_out + "/" + name + ".ppm"); }
static int g_frame = 0;
static void Frame(const char* seq) {
    if (g_frames.empty()) return;
    lv_refr_now(nullptr);
    char name[64];
    std::snprintf(name, sizeof name, "/%s_%03d.ppm", seq, g_frame++);
    Save(g_frames + name);
}

int main(int argc, char** argv) {
    g_out = argc > 1 ? argv[1] : ".";
    g_frames = argc > 2 ? argv[2] : "";
    lv_init();
    lv_tick_set_cb(FakeTick);
    static std::vector<uint8_t> buf(480 * 480 * 2);
    lv_display_t* d = lv_display_create(480, 480);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, buf.data(), nullptr, buf.size(), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(d, Flush);
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);

    auto fresh = [&](auto&& body) {
        auto face = std::make_unique<ChipoFace>();
        face->SetIdleQuirks(false);
        face->Start(lv_screen_active());
        Run(1500);
        body(*face);
        face->Stop();
    };
    // A: finger gaze, hearts, sparkles, slow blink, yawn
    fresh([&](ChipoFace& f) { Run(600, [&] { f.LookAt(0.9f, -0.5f); }); Shot("a_gaze"); });
    fresh([&](ChipoFace& f) { f.Celebrate(ChipoFace::Burst::Hearts, 7); f.ShowEmotion(ChipoFace::Emotion::Love, 4000); Run(900); Shot("a_hearts"); });
    fresh([&](ChipoFace& f) { f.Celebrate(ChipoFace::Burst::Sparkles, 8); f.ShowEmotion(ChipoFace::Emotion::Laugh, 4000); Run(420); Shot("a_sparkles"); });
    fresh([&](ChipoFace& f) { f.SlowBlink(); Run(560); Shot("a_slow_blink"); });
    fresh([&](ChipoFace& f) { f.ShowEmotion(ChipoFace::Emotion::Tired, 9000); Run(900); Shot("a_yawn"); });
    // B: tears, tantrum, sleep, costumes
    fresh([&](ChipoFace& f) { f.Cry(6000); Run(1900); Shot("b_tears"); });
    fresh([&](ChipoFace& f) { f.SoreLoser(); Run(600); Shot("b_tantrum"); });
    fresh([&](ChipoFace& f) { f.SetState(ChipoFace::State::Idle); Run(131000); Run(4200); Shot("b_sleep"); });
    fresh([&](ChipoFace& f) { f.SetCostume(Costume::SantaHat); Run(1500); Shot("b_santa"); });
    fresh([&](ChipoFace& f) { f.SetCostume(Costume::Sunglasses); Run(2000); Shot("b_sunglasses"); });
    fresh([&](ChipoFace& f) { f.SetCostume(Costume::Mustache); Run(1500); Shot("b_moustache"); });
    // C: dance to a fake beat
    g_frame = 0;
    fresh([&](ChipoFace& f) {
        float level = 0;
        f.SetLevelSource([&] { return level; });
        f.SetDance(true);
        uint32_t t = 0;
        Run(2400, [&] {
            t += 40;
            const float ph = std::fmod(static_cast<float>(t), 500.0f) / 500.0f;
            level = ph < 0.3f ? 0.9f - ph : 0.12f;
            if (t % 80 == 0 && !g_frames.empty()) { lv_timer_handler(); Frame("dance"); }
        });
        Shot("c_dance");
    });
    // Scenes
    ChipoScenes scenes;
    static float phase = 0;
    ChipoScenes::Hooks h;
    h.audio = [](float* b, float* level) {
        for (int i = 0; i < 8; ++i) {
            const float v = 0.5f + 0.5f * std::sin(phase * 3.0f + static_cast<float>(i) * 0.9f);
            b[i] = (0.85f - 0.07f * static_cast<float>(i)) * (0.35f + 0.65f * v * v);
        }
        *level = 0.6f;
        return true;
    };
    h.notify = [](const char*) {};
    scenes.Start(lv_screen_active(), std::move(h));
    const struct { SceneKind k; const char* n; } sc[] = {{SceneKind::Visualizer, "visualizer"}, {SceneKind::Aquarium, "aquarium"}, {SceneKind::Lava, "lava"}};
    for (const auto& s : sc) {
        g_frame = 0;
        scenes.Show(s.k);
        uint32_t t = 0;
        Run(s.k == SceneKind::Lava ? 12000 : 6000, [&] {
            t += 40;
            phase += 0.04f;
            if (t % 120 == 0 && !g_frames.empty()) { lv_timer_handler(); Frame(s.n); }
        });
        Shot((std::string("c_") + s.n).c_str());
        scenes.Hide();
    }
    return 0;
}
