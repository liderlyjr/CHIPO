#include "chipo_weather_view.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t kWhite = 0xFFFFFF;
constexpr uint32_t kSun = 0xFFC83D;
constexpr uint32_t kSunRay = 0xFFD76A;

lv_obj_t* Shape(lv_obj_t* parent, int x, int y, int w, int h, int radius, uint32_t color, lv_opa_t opa = LV_OPA_COVER) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, std::max(w, 1), std::max(h, 1));
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, opa, 0);
    lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    return o;
}

lv_obj_t* Circle(lv_obj_t* parent, int cx, int cy, int d, uint32_t color, lv_opa_t opa = LV_OPA_COVER) {
    return Shape(parent, cx - d / 2, cy - d / 2, d, d, LV_RADIUS_CIRCLE, color, opa);
}

void Glow(lv_obj_t* o, int width, uint32_t color, lv_opa_t opa) {
    lv_obj_set_style_shadow_width(o, width, 0);
    lv_obj_set_style_shadow_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_shadow_opa(o, opa, 0);
}

lv_obj_t* Label(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t color, lv_opa_t opa = LV_OPA_COVER) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_opa(l, opa, 0);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}

// ---- icons: everything is expressed in a 100x100 box scaled to `size` -------------------------

struct Box {
    lv_obj_t* parent;
    int size;
    int U(int v) const { return v * size / 100; }
};

void DrawSun(const Box& b, int cx, int cy, int scale_pct) {
    const int core = b.U(44 * scale_pct / 100);
    const int ray_r = b.U(34 * scale_pct / 100);
    const int ray_d = std::max(2, b.U(8 * scale_pct / 100));
    static const int kDx[8] = {0, 71, 100, 71, 0, -71, -100, -71};  // cos * 100
    static const int kDy[8] = {-100, -71, 0, 71, 100, 71, 0, -71};  // sin * 100
    for (int i = 0; i < 8; ++i) {
        Circle(b.parent, b.U(cx) + ray_r * kDx[i] / 100, b.U(cy) + ray_r * kDy[i] / 100, ray_d, kSunRay);
    }
    Glow(Circle(b.parent, b.U(cx), b.U(cy), core, kSun), b.U(16), 0xFFB300, LV_OPA_60);
}

void DrawMoon(const Box& b, int cx, int cy, int scale_pct) {
    const int d = b.U(50 * scale_pct / 100);
    Glow(Circle(b.parent, b.U(cx), b.U(cy), d, 0xE9EEFF), b.U(12), 0xAFC2FF, LV_OPA_40);
    Circle(b.parent, b.U(cx) - d / 5, b.U(cy) - d / 6, d / 5, 0xC5CCE6);
    Circle(b.parent, b.U(cx) + d / 5, b.U(cy) + d / 8, d / 6, 0xC5CCE6);
    Circle(b.parent, b.U(cx) - d / 12, b.U(cy) + d / 4, d / 8, 0xC5CCE6);
}

// A cloud whose base pill starts at (left, top) and is `w` wide (box units).
void DrawCloud(const Box& b, int left, int top, int w, uint32_t color) {
    const int base_h = w * 42 / 100;
    Shape(b.parent, b.U(left), b.U(top), b.U(w), b.U(base_h), LV_RADIUS_CIRCLE, color);
    const int a = w * 46 / 100, bb = w * 56 / 100;
    Circle(b.parent, b.U(left + w * 12 / 100) + b.U(a) / 2, b.U(top - w * 20 / 100) + b.U(a) / 2, b.U(a), color);
    Circle(b.parent, b.U(left + w * 34 / 100) + b.U(bb) / 2, b.U(top - w * 34 / 100) + b.U(bb) / 2, b.U(bb), color);
}

void DrawBolt(const Box& b, int cx, int cy) {
    // Two slanted slabs that read as a lightning bolt at small sizes.
    auto slab = [&](int x, int y, int w, int h, int angle_deci) {
        lv_obj_t* o = Shape(b.parent, b.U(x) - b.U(w) / 2, b.U(y) - b.U(h) / 2, b.U(w), b.U(h), b.U(2), 0xFFD23F);
        lv_obj_set_style_transform_pivot_x(o, b.U(w) / 2, 0);
        lv_obj_set_style_transform_pivot_y(o, b.U(h) / 2, 0);
        lv_obj_set_style_transform_rotation(o, angle_deci, 0);
        Glow(o, b.U(6), 0xFFB300, LV_OPA_50);
    };
    slab(cx + 4, cy - 6, 11, 24, 280);
    slab(cx - 4, cy + 12, 11, 22, 280);
}

lv_obj_t* MakeIcon(lv_obj_t* parent, WeatherKind kind, bool is_day, int x, int y, int size) {
    lv_obj_t* c = Shape(parent, x, y, size, size, 0, 0, LV_OPA_TRANSP);
    lv_obj_add_flag(c, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    const Box b{c, size};
    switch (kind) {
    case WeatherKind::Clear:
        if (is_day) DrawSun(b, 50, 50, 100);
        else DrawMoon(b, 50, 50, 100);
        break;
    case WeatherKind::PartlyCloudy:
        if (is_day) DrawSun(b, 34, 34, 78);
        else DrawMoon(b, 34, 34, 70);
        DrawCloud(b, 24, 56, 68, 0xF2F5FA);
        break;
    case WeatherKind::Cloudy:
        DrawCloud(b, 40, 40, 50, 0xAEB9C7);
        DrawCloud(b, 12, 52, 72, 0xF2F5FA);
        break;
    case WeatherKind::Fog:
        DrawCloud(b, 16, 30, 68, 0xDCE2EA);
        Shape(b.parent, b.U(14), b.U(66), b.U(72), b.U(7), LV_RADIUS_CIRCLE, 0xE9EDF3);
        Shape(b.parent, b.U(24), b.U(78), b.U(62), b.U(7), LV_RADIUS_CIRCLE, 0xE9EDF3);
        Shape(b.parent, b.U(18), b.U(90), b.U(54), b.U(7), LV_RADIUS_CIRCLE, 0xE9EDF3);
        break;
    case WeatherKind::Rain:
        DrawCloud(b, 15, 36, 70, 0xD3DAE6);
        for (int i = 0; i < 3; ++i) {
            Shape(b.parent, b.U(28 + i * 18), b.U(72 + (i == 1 ? 6 : 0)), std::max(2, b.U(6)), b.U(16),
                  LV_RADIUS_CIRCLE, 0x6EC6FF);
        }
        break;
    case WeatherKind::Snow:
        DrawCloud(b, 15, 36, 70, 0xD3DAE6);
        for (int i = 0; i < 3; ++i) {
            Circle(b.parent, b.U(31 + i * 19), b.U(78 + (i == 1 ? 9 : 0)), std::max(3, b.U(9)), kWhite);
        }
        break;
    case WeatherKind::Thunder:
        DrawCloud(b, 15, 30, 70, 0xB0B9C8);
        DrawBolt(b, 50, 72);
        break;
    }
    return c;
}

// ---- seven-segment digits ---------------------------------------------------------------------

constexpr uint8_t kSeg[10] = {0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110,
                              0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111};
// bit 0..6 = a(top) b(top right) c(bottom right) d(bottom) e(bottom left) f(top left) g(middle)

struct Digits {
    int h, w, t, gap, spacing;
    explicit Digits(int height) : h(height), w(height * 52 / 100), t(std::max(3, height * 13 / 100)),
                                   gap(std::max(1, height * 3 / 100)), spacing(std::max(4, height * 14 / 100)) {}
};

// 'glyph' is a digit 0-9, '-' or ' '. Returns the width used.
void DrawGlyph(lv_obj_t* parent, const Digits& g, int x, int y, char glyph, uint32_t color) {
    uint8_t bits = 0;
    if (glyph >= '0' && glyph <= '9') bits = kSeg[glyph - '0'];
    else if (glyph == '-') bits = 0b1000000;
    const int r = LV_RADIUS_CIRCLE;
    const int hx = x + g.t / 2 + g.gap, hw = g.w - g.t - 2 * g.gap;
    const int up_y = y + g.t / 2 + g.gap, up_h = g.h / 2 - g.gap - up_y + y;
    const int lo_y = y + g.h / 2 + g.gap, lo_h = y + g.h - g.t / 2 - g.gap - lo_y;
    auto seg = [&](bool on, int sx, int sy, int sw, int sh) {
        if (on) Glow(Shape(parent, sx, sy, sw, sh, r, color), g.t * 2, color, LV_OPA_20);
    };
    seg(bits & 1, hx, y, hw, g.t);                                // a
    seg(bits & 2, x + g.w - g.t, up_y, g.t, up_h);               // b
    seg(bits & 4, x + g.w - g.t, lo_y, g.t, lo_h);               // c
    seg(bits & 8, hx, y + g.h - g.t, hw, g.t);                   // d
    seg(bits & 16, x, lo_y, g.t, lo_h);                          // e
    seg(bits & 32, x, up_y, g.t, up_h);                          // f
    seg(bits & 64, hx, y + (g.h - g.t) / 2, hw, g.t);            // g
}

int DigitsWidth(const Digits& g, int count) { return count * g.w + std::max(0, count - 1) * g.spacing; }

void DrawNumber(lv_obj_t* parent, const Digits& g, int x, int y, const char* text, uint32_t color) {
    const int n = static_cast<int>(std::strlen(text));
    for (int i = 0; i < n; ++i) DrawGlyph(parent, g, x + i * (g.w + g.spacing), y, text[i], color);
}

struct Sky {
    uint32_t top, bottom;
};

Sky SkyFor(WeatherKind kind, bool is_day, bool placeholder) {
    if (placeholder) return {0x1A2230, 0x384659};
    switch (kind) {
    case WeatherKind::Clear: return is_day ? Sky{0x1E6FD9, 0x62B6F4} : Sky{0x070B24, 0x1B2A5C};
    case WeatherKind::PartlyCloudy: return is_day ? Sky{0x3A78C4, 0x86AED6} : Sky{0x0E1530, 0x2C3C66};
    case WeatherKind::Cloudy: return is_day ? Sky{0x56677B, 0x8A9AAC} : Sky{0x1A2230, 0x384659};
    case WeatherKind::Fog: return is_day ? Sky{0x6B7684, 0x9EA8B3} : Sky{0x262D38, 0x4A5461};
    case WeatherKind::Rain: return is_day ? Sky{0x2C3E55, 0x58728C} : Sky{0x111A28, 0x2C3E55};
    case WeatherKind::Snow: return is_day ? Sky{0x5B7189, 0xA2B8CC} : Sky{0x1F2B3A, 0x4A6075};
    case WeatherKind::Thunder: return Sky{0x1F1B33, 0x3E3860};
    }
    return {0x1A2230, 0x384659};
}

void Fmt(char* out, size_t n, int v) { std::snprintf(out, n, "%d", v); }

}  // namespace

ChipoWeatherView::~ChipoWeatherView() { Destroy(); }

bool ChipoWeatherView::Create(lv_obj_t* parent, WeatherFonts fonts) {
    if (root_ || !parent || !fonts.small || !fonts.medium) return false;
    fonts_ = fonts;
    root_ = lv_obj_create(parent);
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_dir(root_, LV_GRAD_DIR_VER, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    // Clickable on purpose: while the weather is up it must not let touches fall through to
    // whatever is underneath. The controller closes it on any tap.
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(root_);
    width_ = lv_obj_get_width(root_);
    height_ = lv_obj_get_height(root_);

    content_ = Shape(root_, 0, 0, width_, height_, 0, 0, LV_OPA_TRANSP);
    return true;
}

void ChipoWeatherView::Destroy() {
    if (root_) lv_obj_delete(root_);
    root_ = content_ = nullptr;
}

void ChipoWeatherView::Show() {
    if (!root_) return;
    lv_obj_move_foreground(root_);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_fade_in(root_, 250, 0);
}

void ChipoWeatherView::Hide() {
    if (root_) lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

bool ChipoWeatherView::IsVisible() const { return root_ && !lv_obj_has_flag(root_, LV_OBJ_FLAG_HIDDEN); }

void ChipoWeatherView::BeginRender(WeatherKind kind, bool is_day, bool placeholder) {
    lv_obj_clean(content_);
    const Sky sky = SkyFor(kind, is_day, placeholder);
    lv_obj_set_style_bg_color(root_, lv_color_hex(sky.top), 0);
    lv_obj_set_style_bg_grad_color(root_, lv_color_hex(sky.bottom), 0);
}

// Icon on the left, temperature on the right (digits + degree ring), centred as a group.
void ChipoWeatherView::DrawHero(int y, WeatherKind kind, bool is_day, const char* temp_text) {
    const int icon = 76, gap = 14;
    const Digits g(64);
    const int n = static_cast<int>(std::strlen(temp_text));
    const int ring = g.t * 26 / 10;
    const int digits_w = DigitsWidth(g, n) + 4 + ring;
    const int total = icon + gap + digits_w;
    const int x0 = (width_ - total) / 2;
    MakeIcon(content_, kind, is_day, x0, y - 2 + (g.h - icon) / 2 + 2, icon);
    const int dx = x0 + icon + gap;
    DrawNumber(content_, g, dx, y, temp_text, kWhite);
    lv_obj_t* r = Shape(content_, dx + DigitsWidth(g, n) + 4, y + 2, ring, ring, LV_RADIUS_CIRCLE, 0, LV_OPA_TRANSP);
    lv_obj_set_style_border_width(r, std::max(2, g.t * 7 / 10), 0);
    lv_obj_set_style_border_color(r, lv_color_hex(kWhite), 0);
}

void ChipoWeatherView::RenderStatus(const char* title, const char* line) {
    if (!root_) return;
    BeginRender(WeatherKind::Cloudy, true, true);
    const int lh_m = lv_font_get_line_height(fonts_.medium);
    int y = 14;
    lv_obj_t* l = Label(content_, title ? title : "CHIPO", fonts_.medium, kWhite);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
    y += lh_m + 6;
    DrawHero(y, WeatherKind::Cloudy, true, "--");
    y += 76 + 16;
    lv_obj_t* t = Label(content_, line ? line : "", fonts_.medium, kWhite, LV_OPA_80);
    lv_obj_set_width(t, width_ - 32);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, y);
}

void ChipoWeatherView::Render(const WeatherData& d) {
    if (!root_) return;
    BeginRender(d.kind, d.is_day, false);
    const int lh_s = lv_font_get_line_height(fonts_.small);
    const int lh_m = lv_font_get_line_height(fonts_.medium);
    const int margin = 16;
    char buf[48];
    int y = 14;

    lv_obj_t* city = Label(content_, d.city[0] ? d.city : "CHIPO", fonts_.medium, kWhite);
    lv_obj_align(city, LV_ALIGN_TOP_MID, 0, y);
    y += lh_m + 6;

    Fmt(buf, sizeof buf, d.temp);
    DrawHero(y, d.kind, d.is_day, buf);
    y += 76 + 10;

    lv_obj_t* cond = Label(content_, WeatherKindText(d.kind, d.is_day), fonts_.medium, kWhite);
    lv_obj_align(cond, LV_ALIGN_TOP_MID, 0, y);
    y += lh_m;

    std::snprintf(buf, sizeof buf, "Max %d   Min %d   Sens. %d", d.hi, d.lo, d.feels);
    lv_obj_t* range = Label(content_, buf, fonts_.small, kWhite, LV_OPA_80);
    lv_obj_align(range, LV_ALIGN_TOP_MID, 0, y);
    y += lh_s + 12;

    // ---- next hours ----------------------------------------------------------------------------
    const int icon_s = 30;
    const int hour_h = 8 + lh_s + 2 + icon_s + 2 + lh_s + 8;
    lv_obj_t* hcard = Shape(content_, margin, y, width_ - 2 * margin, hour_h, 22, kWhite, LV_OPA_20);
    const int cols = std::max(1, d.hour_count);
    const int col_w = (width_ - 2 * margin) / kWeatherHours;
    const int hx0 = ((width_ - 2 * margin) - col_w * cols) / 2;
    for (int i = 0; i < d.hour_count; ++i) {
        const WeatherHour& h = d.hours[i];
        const int cx = hx0 + i * col_w + col_w / 2;
        lv_obj_t* hl = Label(hcard, h.label, fonts_.small, kWhite, LV_OPA_80);
        lv_obj_align(hl, LV_ALIGN_TOP_LEFT, 0, 8);
        lv_obj_update_layout(hl);
        lv_obj_set_x(hl, cx - lv_obj_get_width(hl) / 2);
        MakeIcon(hcard, h.kind, h.is_day, cx - icon_s / 2, 8 + lh_s + 2, icon_s);
        Fmt(buf, sizeof buf, h.temp);
        lv_obj_t* tl = Label(hcard, buf, fonts_.small, kWhite);
        lv_obj_align(tl, LV_ALIGN_TOP_LEFT, 0, 8 + lh_s + 2 + icon_s + 2);
        lv_obj_update_layout(tl);
        lv_obj_set_x(tl, cx - lv_obj_get_width(tl) / 2);
    }
    y += hour_h + 10;

    // ---- next days (as many rows as fit above the bottom margin) -------------------------------
    const int row_h = std::max(lh_s + 10, 34);
    const int room = height_ - 22 - y - 12;
    const int rows = std::min(d.day_count, std::max(0, room / row_h));
    if (rows > 0) {
        lv_obj_t* dcard = Shape(content_, margin, y, width_ - 2 * margin, rows * row_h + 12, 22, kWhite, LV_OPA_20);
        const int cw = width_ - 2 * margin;
        for (int i = 0; i < rows; ++i) {
            const WeatherDay& day = d.days[i];
            const int ry = 6 + i * row_h;
            lv_obj_t* nl = Label(dcard, day.name, fonts_.small, kWhite);
            lv_obj_align(nl, LV_ALIGN_TOP_LEFT, 16, ry + (row_h - lh_s) / 2);
            MakeIcon(dcard, day.kind, true, cw / 2 - 40, ry + (row_h - 26) / 2, 26);
            Fmt(buf, sizeof buf, day.hi);
            lv_obj_t* hi = Label(dcard, buf, fonts_.small, kWhite);
            lv_obj_set_width(hi, 44);
            lv_obj_set_style_text_align(hi, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(hi, LV_ALIGN_TOP_RIGHT, -16, ry + (row_h - lh_s) / 2);
            Fmt(buf, sizeof buf, day.lo);
            lv_obj_t* lo = Label(dcard, buf, fonts_.small, kWhite, LV_OPA_60);
            lv_obj_set_width(lo, 44);
            lv_obj_set_style_text_align(lo, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(lo, LV_ALIGN_TOP_RIGHT, -16 - 52, ry + (row_h - lh_s) / 2);
        }
    }
}
