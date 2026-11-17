#pragma once

#include "chipo_weather.h"
#include "lvgl.h"

/**
 * The weather screen: a full-screen LVGL panel (city, big temperature, icon, next hours, next
 * days) drawn only with LVGL shapes and two text fonts. No images, no font files, no network.
 * All methods must be called from the LVGL task (lock held).
 *
 * Text is ASCII only: the Montserrat fonts have no accents. The big temperature is drawn as
 * seven-segment digits so it does not depend on a large font being compiled in.
 */
struct WeatherFonts {
    const lv_font_t* small;   // hours, days, temperatures (about 20 px)
    const lv_font_t* medium;  // city and condition (about 24 px)
};

class ChipoWeatherView {
public:
    ChipoWeatherView() = default;
    ~ChipoWeatherView();
    ChipoWeatherView(const ChipoWeatherView&) = delete;
    ChipoWeatherView& operator=(const ChipoWeatherView&) = delete;

    bool Create(lv_obj_t* parent, WeatherFonts fonts);
    void Destroy();

    void Render(const WeatherData& data);
    // Placeholder screen: "--" instead of a temperature plus one line of text.
    void RenderStatus(const char* title, const char* line);

    void Show();
    void Hide();
    bool IsVisible() const;
    lv_obj_t* root() const { return root_; }

private:
    void BeginRender(WeatherKind kind, bool is_day, bool placeholder);
    void DrawHero(int y, WeatherKind kind, bool is_day, const char* temp_text);

    lv_obj_t* root_ = nullptr;
    lv_obj_t* content_ = nullptr;
    WeatherFonts fonts_{};
    int width_ = 0, height_ = 0;
};
