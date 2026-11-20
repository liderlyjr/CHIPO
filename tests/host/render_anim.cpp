// Developer tool (not a test): renders CHIPO's face animations as numbered PPM frames, one folder per
// sequence, to review the motion and build GIFs. Usage: render_anim <out_dir> [width height]
// Sequences: conversation (idle -> listening -> thinking -> speaking -> idle), emotions, sleep, yawn.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "chipo_face.h"

// Fake accelerometer: gravity on +Y when CHIPO stands upright (as in test_face.cpp).
static ImuSample g_imu{0, 1, 0};
static void SetRoll(float deg) {
    const float r = deg * 3.14159265f / 180.0f;
    g_imu = {std::sin(r), std::cos(r), 0};
}

static uint32_t g_now = 1000;
static uint32_t FakeTick() { return g_now; }
static int W = 480, H = 480;
static std::vector<uint8_t> g_fb;
static void Flush(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
    const int w = a->x2 - a->x1 + 1;
    for (int y = a->y1; y <= a->y2; ++y)
        for (int x = a->x1; x <= a->x2; ++x) {
            const uint8_t* s = px + ((y - a->y1) * w + (x - a->x1)) * 2;
            g_fb[(y * W + x) * 2] = s[0];
            g_fb[(y * W + x) * 2 + 1] = s[1];
        }
    lv_display_flush_ready(d);
}
static void Save(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; ++i) {
        const uint16_t v = g_fb[i * 2] | (g_fb[i * 2 + 1] << 8);
        const uint8_t rgb[3] = {static_cast<uint8_t>(((v >> 11) & 31) * 255 / 31),
                                static_cast<uint8_t>(((v >> 5) & 63) * 255 / 63),
                                static_cast<uint8_t>((v & 31) * 255 / 31)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
}

int main(int argc, char** argv) {
    const std::string out = argc > 1 ? argv[1] : ".";
    if (argc > 3) { W = std::atoi(argv[2]); H = std::atoi(argv[3]); }
    g_fb.assign(static_cast<size_t>(W) * H * 2, 0);
    lv_init();
    lv_tick_set_cb(FakeTick);
    static std::vector<uint8_t> buf;
    buf.assign(static_cast<size_t>(W) * H * 2, 0);
    lv_display_t* d = lv_display_create(W, H);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, buf.data(), nullptr, buf.size(), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(d, Flush);
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);

    ChipoFace face;
    face.SetIdleQuirks(false);
    face.Start(lv_screen_active());
    face.SetTiltSource([](ImuSample* s) { *s = g_imu; return true; });
    for (int i = 0; i < 40; ++i) { g_now += 40; lv_timer_handler(); }

    std::string dir;
    int frame = 0;
    auto begin = [&](const char* name) {
        dir = out + "/" + name;
        mkdir(dir.c_str(), 0755);
        frame = 0;
    };
    // 80 ms per frame (12.5 fps): enough to judge the motion, small GIFs.
    auto play = [&](uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += 40) {
            g_now += 40;
            lv_timer_handler();
            if ((t / 40) % 2 == 0) {
                lv_refr_now(nullptr);
                char name[32];
                std::snprintf(name, sizeof name, "/f_%03d.ppm", frame++);
                Save(dir + name);
            }
        }
    };

    begin("conversation");
    face.SetState(ChipoFace::State::Idle);
    play(3200);
    face.SetState(ChipoFace::State::Listening);
    play(2000);
    face.SetState(ChipoFace::State::Thinking);
    play(1800);
    face.SetState(ChipoFace::State::Speaking);
    play(2600);
    face.SetState(ChipoFace::State::Idle);
    play(1200);

    begin("emotions");
    const char* names[] = {"happy", "loving", "surprised", "sad", "angry", "laughing", "winking"};
    for (const char* n : names) {
        face.ShowEmotionByName(n, 60000);
        play(1300);
    }
    face.ShowEmotion(ChipoFace::Emotion::None);
    play(600);

    begin("sleep");
    face.SetState(ChipoFace::State::Sleep);
    play(4400);
    face.SetState(ChipoFace::State::Idle);
    play(1200);

    begin("yawn");
    face.ShowEmotion(ChipoFace::Emotion::Tired, 60000);
    play(5400);
    face.ShowEmotion(ChipoFace::Emotion::None);
    play(800);

    // Tilted past 45 degrees the face slides off the side, then comes back when CHIPO stands up.
    begin("fall");
    SetRoll(0);
    play(1200);
    SetRoll(55);
    play(2400);
    std::printf("fall: face_dx %d (screen %d px wide)\n", face.Debug().face_dx, W);
    SetRoll(0);
    play(3000);
    return 0;
}
