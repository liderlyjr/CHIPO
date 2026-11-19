// Developer tool (not a test): renders the weather screen for several conditions to PPM. Usage: render_weather <out_dir>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "chipo_weather_view.h"

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
    ChipoWeatherView view;
    view.Create(lv_screen_active(), WeatherFonts{&lv_font_montserrat_20, &lv_font_montserrat_24});
    struct Case { const char* name; WeatherKind kind; bool day; int temp; };
    const Case cases[] = {{"clear_day", WeatherKind::Clear, true, 27}, {"partly_day", WeatherKind::PartlyCloudy, true, 21},
                          {"rain", WeatherKind::Rain, true, 14}, {"thunder", WeatherKind::Thunder, false, 18},
                          {"snow_night", WeatherKind::Snow, false, -2}, {"clear_night", WeatherKind::Clear, false, 12}};
    for (const auto& c : cases) {
        WeatherData w;
        w.valid = true;
        std::strcpy(w.city, "Valladolid");
        w.temp = c.temp; w.feels = c.temp - 1; w.hi = c.temp + 3; w.lo = c.temp - 6;
        w.kind = c.kind; w.is_day = c.day;
        w.hour_count = kWeatherHours;
        for (int i = 0; i < kWeatherHours; ++i) {
            std::snprintf(w.hours[i].label, sizeof w.hours[i].label, i ? "%dh" : "Ahora", 17 + 2 * i);
            w.hours[i].temp = static_cast<int16_t>(c.temp - i);
            w.hours[i].kind = i % 2 ? WeatherKind::Cloudy : c.kind;
            w.hours[i].is_day = c.day;
        }
        w.day_count = kWeatherDays;
        const char* names[] = {"Vie", "Sab", "Dom"};
        const WeatherKind ks[] = {WeatherKind::Clear, WeatherKind::Rain, WeatherKind::PartlyCloudy};
        for (int i = 0; i < kWeatherDays; ++i) {
            std::strcpy(w.days[i].name, names[i]);
            w.days[i].hi = static_cast<int16_t>(c.temp + 3 - i); w.days[i].lo = static_cast<int16_t>(c.temp - 6); w.days[i].kind = ks[i];
        }
        view.Render(w);
        view.Show();
        g_now += 2000;
        lv_timer_handler();
        lv_refr_now(nullptr);
        Save(out + "/" + c.name + ".ppm");
    }
    return 0;
}
