// Developer tool (not a test): renders CHIPO's faces (states and emotions) to PPM. Usage: render_faces <out_dir>
#include <cstdio>
#include <string>
#include <vector>

#include "chipo_face.h"

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
int main(int argc, char** argv) {
    const std::string out = argc > 1 ? argv[1] : ".";
    lv_init();
    lv_tick_set_cb(FakeTick);
    static std::vector<uint8_t> buf(480 * 480 * 2);
    lv_display_t* d = lv_display_create(480, 480);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, buf.data(), nullptr, buf.size(), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(d, Flush);
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
    ChipoFace face;
    face.SetIdleQuirks(false);
    face.Start(lv_screen_active());
    auto run = [&](uint32_t ms) { for (uint32_t t = 0; t < ms; t += 40) { g_now += 40; lv_timer_handler(); } };
    auto shot = [&](const char* name) { lv_refr_now(nullptr); Save(out + "/" + name + ".ppm"); };
    run(1500);
    struct S { const char* n; ChipoFace::State s; const char* e; };
    const S list[] = {
        {"idle", ChipoFace::State::Idle, nullptr}, {"listening", ChipoFace::State::Listening, nullptr},
        {"thinking", ChipoFace::State::Thinking, nullptr}, {"speaking", ChipoFace::State::Speaking, nullptr},
        {"error", ChipoFace::State::Error, nullptr}, {"love", ChipoFace::State::Idle, "loving"},
        {"happy", ChipoFace::State::Idle, "happy"}, {"surprised", ChipoFace::State::Idle, "surprised"},
        {"grumpy", ChipoFace::State::Idle, "angry"}, {"sad", ChipoFace::State::Idle, "sad"},
    };
    for (const auto& it : list) {
        face.SetState(it.s);
        if (it.e) face.ShowEmotionByName(it.e, 60000);
        run(1400);
        shot(it.n);
        if (it.e) face.ShowEmotion(ChipoFace::Emotion::None);
    }
    return 0;
}
