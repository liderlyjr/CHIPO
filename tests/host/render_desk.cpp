// Developer tool (not a test): renders the "desk clock" sequence (tip over, fall, clock, stand up, dizzy)
// as numbered PPM frames plus meta.txt (frame, device roll in degrees, duration in ms, phase).
// Usage: render_desk <out_dir>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "chipo_buddy.h"
#include "chipo_face.h"

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

static float g_roll = 0;
static bool g_new = false;

int main(int argc, char** argv) {
    const std::string out = argc > 1 ? argv[1] : ".";
    lv_init();
    lv_tick_set_cb(FakeTick);
    static std::vector<uint8_t> buf(368 * 448 * 2);
    lv_display_t* d = lv_display_create(368, 448);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, buf.data(), nullptr, buf.size(), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(d, Flush);
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);

    ChipoFace face;
    face.SetIdleQuirks(false);
    face.SetTiltSource([](ImuSample* s) {
        if (!g_new) return false;
        g_new = false;
        const float r = g_roll * 3.14159265f / 180.0f;
        *s = ImuSample{std::sin(r), std::cos(r), 0};
        return true;
    });
    face.Start(lv_screen_active());

    ChipoBuddy buddy;
    ChipoBuddy::Hooks h;
    h.play_clip = [](const char*) { return true; };
    h.notify = [](const char*) {};
    h.clock = [] { return g_now; };
    h.wall_time = [](BuddyWallTime* o) {
        o->valid = true; o->hour = 9; o->minute = 41; o->second = static_cast<int>((g_now / 1000) % 60);
        return true;
    };
    buddy.Start(lv_screen_active(), std::move(h));
    face.SetDeskClockObserver([&](bool on, int side) { buddy.SetDeskClock(on, side); });

    FILE* meta = std::fopen((out + "/meta.txt").c_str(), "w");
    int n = 0;
    auto shot = [&](uint32_t ms, uint32_t skip, const char* phase, float roll_from, float roll_to) {
        const uint32_t steps = ms / 40;
        for (uint32_t i = 0; i < steps; ++i) {
            g_roll = roll_from + (roll_to - roll_from) * static_cast<float>(i + 1) / static_cast<float>(steps);
            g_now += 40; g_new = true;
            face.Step(g_now); buddy.Step(g_now); lv_timer_handler();
            if (i % skip != skip - 1 && i + 1 != steps) continue;
            lv_refr_now(nullptr);
            char name[32]; std::snprintf(name, sizeof name, "/f_%03d.ppm", n);
            Save(out + name);
            std::fprintf(meta, "%d %.1f %u %s\n", n, g_roll, 40 * skip, phase);
            ++n;
        }
    };
    shot(1800, 1, "upright", 0, 0);
    shot(700, 1, "tipping", 0, 90);
    shot(2500, 2, "lying", 90, 90);
    shot(8000, 20, "waiting", 90, 90);   // up to ~10 s lying; the clock appears inside this stretch
    shot(2500, 5, "clock", 90, 90);
    shot(900, 1, "standing", 90, 0);
    shot(3200, 2, "dizzy", 0, 0);
    shot(1000, 5, "back", 0, 0);
    std::fclose(meta);
    return 0;
}
