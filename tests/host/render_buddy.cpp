// Developer tool (not a test): renders the BUDDY clock screens to PPM. Usage: render_buddy <out_dir>
#include <cstdio>
#include <string>
#include <vector>

#include "chipo_buddy_view.h"

static uint32_t FakeTick() { return 1000; }
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
    ChipoBuddyView v;
    v.Create(lv_screen_active(), &lv_font_montserrat_24);
    auto shot = [&](const char* name, BuddyScreen s) { v.Show(s); lv_refr_now(nullptr); Save(out + "/" + name + ".ppm"); };
    BuddyScreen s;
    s.caption = "CONCENTRACION 2/4"; s.footer = "minutos : segundos"; s.color = 0xFF9A2E;
    s.digits[0] = '1'; s.digits[1] = '7'; s.digits[2] = '4'; s.digits[3] = '2';
    shot("buddy_timer", s);
    s = BuddyScreen{}; s.caption = "DESPIERTA"; s.footer = "Toca para posponer 9 min"; s.color = 0xFF4A3D;
    s.digits[0] = '0'; s.digits[1] = '7'; s.digits[2] = '3'; s.digits[3] = '0';
    shot("buddy_alarm", s);
    s = BuddyScreen{}; s.caption = ""; s.footer = "alarma 07:30"; s.color = 0xC8641A; s.brightness = 70;
    s.digits[0] = '2'; s.digits[1] = '3'; s.digits[2] = '0'; s.digits[3] = '5';
    shot("buddy_night", s);
    v.SetRotation(1);
    s = BuddyScreen{}; s.brightness = 230;
    s.digits[0] = '0'; s.digits[1] = '9'; s.digits[2] = '4'; s.digits[3] = '1';
    shot("buddy_desk", s);
    return 0;
}
