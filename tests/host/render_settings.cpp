// Developer tool (not a test): renders the CHIPO SETTINGS menu and battery indicator to PPM. Usage: render_settings <out_dir>
#include <cstdio>
#include <string>
#include <vector>

#include "chipo_settings.h"

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
    ChipoSettings s;
    ChipoSettings::Hooks h;
    h.get_volume = [] { return 70; };
    h.get_brightness = [] { return 80; };
    h.battery = [](int* l, bool* c, bool* dch) { *l = 82; *c = true; *dch = false; return true; };
    h.wifi = [](WifiView* w) { w->connected = true; w->ssid = "MiCasa"; w->rssi_dbm = -55; return true; };
    h.clock = [] { return g_now; };
    s.Start(lv_screen_active(), std::move(h), "0.6.0");
    g_now += 6000; s.Step(g_now); lv_refr_now(nullptr); Save(out + "/settings_indicator.ppm");
    s.Open(); lv_refr_now(nullptr); Save(out + "/settings_menu.ppm");
    return 0;
}
