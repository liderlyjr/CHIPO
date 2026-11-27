#include "chipo_tools.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

#ifndef CHIPO_LEVEL_MIRROR_X
#define CHIPO_LEVEL_MIRROR_X 0  // the sign of the sensor axes is not documented: flip if the bubble runs the wrong way
#endif
#ifndef CHIPO_LEVEL_MIRROR_Y
#define CHIPO_LEVEL_MIRROR_Y 0
#endif

namespace {
constexpr uint32_t kTickMs = 50;
constexpr uint32_t kPrefetchEveryMs = 30u * 60u * 1000u;
constexpr uint32_t kWeatherMaxAgeMs = 3u * 60u * 60u * 1000u;
constexpr int kStripOpa = 150;   // the strip is dim on purpose: AMOLED burn-in and it must not compete with the face
constexpr int kRingOpa = 190;
constexpr uint32_t kLilac = 0xC9A6FF, kPaleLilac = 0xE2C8FF, kPink = 0xFF6FB5, kCyan = 0x5CF0FF;
constexpr uint32_t kAmber = 0xFFB347, kGreen = 0x7CFF9E;
const uint32_t kPens[] = {0xC9A6FF, 0xFF6FB5, 0x5CF0FF, 0xFFD2B0, 0x9CFFB0, 0xFFFFFF};
constexpr int kPenCount = static_cast<int>(sizeof(kPens) / sizeof(kPens[0]));
constexpr int kPenRadius = 4;

lv_obj_t* Plain(lv_obj_t* parent, int w, int h) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

lv_obj_t* Label(lv_obj_t* parent, const lv_font_t* font, uint32_t rgb) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(rgb), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}

lv_obj_t* Circle(lv_obj_t* parent, int d, uint32_t fill, int fill_opa, uint32_t border, int border_w, int border_opa) {
    lv_obj_t* o = Plain(parent, d, d);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(fill), 0);
    lv_obj_set_style_bg_opa(o, static_cast<lv_opa_t>(fill_opa), 0);
    lv_obj_set_style_border_color(o, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(o, border_w, 0);
    lv_obj_set_style_border_opa(o, static_cast<lv_opa_t>(border_opa), 0);
    return o;
}

void Show(lv_obj_t* o, bool on) {
    if (!o) return;
    if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

// Moves `value` towards `target` by at most `step`.
float Approach(float value, float target, float step) {
    if (value < target) return std::min(value + step, target);
    return std::max(value - step, target);
}

// The drawing canvas is a whole screen of RGB565 (460 KB at 480 x 480): it lives in PSRAM and only while it is open.
uint16_t* AllocPixels(size_t count) {
    const size_t bytes = count * sizeof(uint16_t);
#ifdef ESP_PLATFORM
    void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    return static_cast<uint16_t*>(p);
#else
    return static_cast<uint16_t*>(std::malloc(bytes));
#endif
}
void FreePixels(uint16_t* p) {
#ifdef ESP_PLATFORM
    heap_caps_free(p);
#else
    std::free(p);
#endif
}

// Cuts a UTF-8 string so it fits in `size` bytes (with the terminator) without splitting a character.
void CopyUtf8(char* dst, size_t size, const char* src) {
    if (size == 0) return;
    size_t n = std::min(std::strlen(src), size - 1);
    while (n > 0 && (static_cast<unsigned char>(src[n]) & 0xC0) == 0x80) --n;  // never stop in the middle of a character
    std::memcpy(dst, src, n);
    dst[n] = '\0';
}
}  // namespace

ChipoTools::ChipoTools() = default;
ChipoTools::~ChipoTools() { Stop(); }

uint32_t ChipoTools::Now() const { return hooks_.clock ? hooks_.clock() : lv_tick_get(); }

bool ChipoTools::Start(lv_obj_t* parent, Hooks hooks) {
    if (running_) return true;
    if (!parent) return false;
    hooks_ = std::move(hooks);
    parent_ = parent;
    int w = lv_obj_get_width(parent), h = lv_obj_get_height(parent);
    if (w <= 0) w = 480;
    if (h <= 0) h = 480;
    w_ = w;
    h_ = h;
#if LV_FONT_MONTSERRAT_32
    font_big_ = &lv_font_montserrat_32;
#elif LV_FONT_MONTSERRAT_24
    font_big_ = &lv_font_montserrat_24;
#else
    font_big_ = LV_FONT_DEFAULT;
#endif
#if LV_FONT_MONTSERRAT_24
    font_mid_ = &lv_font_montserrat_24;
#else
    font_mid_ = LV_FONT_DEFAULT;
#endif
    font_text_ = hooks_.text_font ? hooks_.text_font() : nullptr;
    text_has_accents_ = font_text_ != nullptr;
    if (!font_text_) font_text_ = font_mid_;
    if (hooks_.load_flag) {
        info_on_ = hooks_.load_flag("tools_info", true);
        subs_on_ = hooks_.load_flag("tools_subs", false);
    }
    Build(w, h);
    if (!root_) return false;
    timer_ = lv_timer_create(TimerCallback, kTickMs, this);
    if (timer_ == nullptr) {
        // Without the tick the clock, the pomodoro ring and the tools would never update. Remove what Build()
        // made and drop the hooks, so Start() can be called again later.
        Stop();
        hooks_ = Hooks{};
        parent_ = nullptr;
        return false;
    }
    next_prefetch_ms_ = Now() + 20000;  // first silent weather download a little after start-up (Wi-Fi needs a moment)
    running_ = true;
    return true;
}

void ChipoTools::Stop() {
    if (!running_ && !root_) return;
    if (timer_) lv_timer_delete(timer_);
    timer_ = nullptr;
    Close();
    for (lv_obj_t** o : {&time_label_, &temp_row_, &ring_, &sub_, &root_}) {
        if (*o) lv_obj_delete(*o);
        *o = nullptr;
    }
    running_ = false;
}

void ChipoTools::Build(int w, int h) {
    lv_obj_t* parent = parent_;
    const int side = std::min(w, h);

    // Pomodoro ring along the edge of the screen.
    ring_ = lv_arc_create(parent);
    lv_obj_set_size(ring_, side - 14, side - 14);
    lv_obj_align(ring_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(ring_, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(ring_, 270);
    lv_arc_set_bg_angles(ring_, 0, 360);
    lv_arc_set_range(ring_, 0, 1000);
    lv_arc_set_value(ring_, 0);
    lv_obj_set_style_arc_width(ring_, 7, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(ring_, LV_OPA_10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring_, lv_color_hex(kPaleLilac), LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring_, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(ring_, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ring_, lv_color_hex(kAmber), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ring_, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(ring_, 0, LV_PART_KNOB);
    lv_obj_add_flag(ring_, LV_OBJ_FLAG_HIDDEN);

    // Info strip: time on top, temperature below.
    time_label_ = Label(parent, font_big_, kPaleLilac);
    lv_label_set_text(time_label_, "");
    lv_obj_align(time_label_, LV_ALIGN_TOP_MID, 0, std::max(18, side / 22));
    lv_obj_add_flag(time_label_, LV_OBJ_FLAG_HIDDEN);

    temp_row_ = Plain(parent, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(temp_row_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temp_row_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(temp_row_, 3, 0);
    temp_num_ = Label(temp_row_, font_big_, kPaleLilac);
    temp_deg_ = Circle(temp_row_, 9, 0x000000, LV_OPA_TRANSP, kPaleLilac, 2, LV_OPA_COVER);
    temp_unit_ = Label(temp_row_, font_big_, kPaleLilac);
    lv_label_set_text(temp_unit_, "C");
    lv_obj_set_style_margin_top(temp_deg_, 4, 0);
    lv_obj_align(temp_row_, LV_ALIGN_BOTTOM_MID, 0, -std::max(18, side / 22));
    lv_obj_add_flag(temp_row_, LV_OBJ_FLAG_HIDDEN);

    // Subtitles.
    sub_ = Label(parent, font_text_, kPaleLilac);
    lv_obj_set_width(sub_, w - 72);
    lv_label_set_long_mode(sub_, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(sub_, lv_font_get_line_height(font_text_) * 3 + 6);
    lv_obj_align(sub_, LV_ALIGN_BOTTOM_MID, 0, -std::max(26, side / 16));
    lv_obj_add_flag(sub_, LV_OBJ_FLAG_HIDDEN);

    // Overlay for the level, motion tools and DrawPanel.
    root_ = Plain(parent, w, h);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_style_bg_color(root_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(root_, RootEvent, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(root_, RootEvent, LV_EVENT_LONG_PRESSED, this);

    caption_ = Label(root_, font_mid_, kPaleLilac);
    lv_obj_align(caption_, LV_ALIGN_TOP_MID, 0, std::max(18, side / 22));
    hint_ = Label(root_, font_mid_, kLilac);
    lv_obj_set_style_opa(hint_, LV_OPA_60, 0);
    lv_obj_align(hint_, LV_ALIGN_BOTTOM_MID, 0, -std::max(18, side / 22));

    // Level.
    const int d = side - 120;
    lvl_ = Plain(root_, w, h);
    lvl_ring_ = Circle(lvl_, d, 0x000000, LV_OPA_TRANSP, kLilac, 3, LV_OPA_70);
    lv_obj_align(lvl_ring_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t* inner = Circle(lvl_, d / 2, 0x000000, LV_OPA_TRANSP, kLilac, 2, LV_OPA_30);
    lv_obj_align(inner, LV_ALIGN_CENTER, 0, 0);
    for (int i = 0; i < 2; ++i) {
        lvl_cross_[i] = Plain(lvl_, i == 0 ? d : 2, i == 0 ? 2 : d);
        lv_obj_set_style_bg_color(lvl_cross_[i], lv_color_hex(kLilac), 0);
        lv_obj_set_style_bg_opa(lvl_cross_[i], LV_OPA_30, 0);
        lv_obj_align(lvl_cross_[i], LV_ALIGN_CENTER, 0, 0);
    }
    lvl_target_ = Circle(lvl_, side / 9, 0x000000, LV_OPA_TRANSP, kCyan, 3, LV_OPA_80);
    lv_obj_align(lvl_target_, LV_ALIGN_CENTER, 0, 0);
    lvl_bubble_ = Circle(lvl_, side / 8, kLilac, LV_OPA_80, kPaleLilac, 3, LV_OPA_COVER);
    lv_obj_align(lvl_bubble_, LV_ALIGN_CENTER, 0, 0);
    lvl_angle_ = Label(lvl_, font_big_, kPaleLilac);
    lv_obj_align(lvl_angle_, LV_ALIGN_CENTER, 0, d / 2 - 52);
    Show(lvl_, false);

    // Gravity Ball: a ball follows gravity on the QMI8658 accelerometer.
    gravity_ball_ = Plain(root_, w, h);
    gravity_ball_track_ = Circle(gravity_ball_, side - 104, 0x000000, LV_OPA_TRANSP, kLilac, 3, LV_OPA_70);
    lv_obj_align(gravity_ball_track_, LV_ALIGN_CENTER, 0, 0);
    gravity_ball_dot_ = Circle(gravity_ball_, side / 9, 0xFF7048, LV_OPA_COVER, 0xFFE3D8, 3, LV_OPA_COVER);
    lv_obj_set_style_shadow_color(gravity_ball_dot_, lv_color_hex(0xFF7048), 0);
    lv_obj_set_style_shadow_width(gravity_ball_dot_, 18, 0);
    lv_obj_set_style_shadow_opa(gravity_ball_dot_, LV_OPA_50, 0);
    lv_obj_align(gravity_ball_dot_, LV_ALIGN_CENTER, 0, 0);
    Show(gravity_ball_, false);

}

// ---- opening and closing --------------------------------------------------------------------------------------------
void ChipoTools::Open(Tool tool) {
    if (!running_ || tool == Tool::None) return;
    Close();
    tool_ = tool;
    shake_.Reset();
    Show(root_, true);
    lv_obj_move_foreground(root_);
    Show(lvl_, tool == Tool::Level);
    Show(gravity_ball_, tool == Tool::GravityBall);
    if (tool == Tool::Level) {
        lv_label_set_text(caption_, "NIVEL");
        lv_label_set_text(hint_, "Toca para calibrar · mantén CHIPO quieto");
        have_accel_ = false;
        level_zero_x_ = level_zero_y_ = 0.0f;
        level_calibrated_ = false;
        level_calibrating_ = false;
        level_calibration_count_ = 0;
        bubble_x_ = bubble_y_ = 0;
    } else if (tool == Tool::GravityBall) {
        lv_label_set_text(caption_, "GRAVITY BALL");
        lv_label_set_text(hint_, "Inclina CHIPO para mover la pelota");
        gravity_x_ = gravity_y_ = gravity_vx_ = gravity_vy_ = 0.0f;
        lv_obj_align(gravity_ball_dot_, LV_ALIGN_CENTER, 0, 0);
    } else if (tool == Tool::Horizon) {
        lv_label_set_text(caption_, "HORIZONTE ARTIFICIAL");
        lv_label_set_text(hint_, "Toca para fijar el horizonte neutro");
        horizon_px_ = AllocPixels(static_cast<size_t>(w_) * static_cast<size_t>(h_));
        if (!horizon_px_) {
            if (hooks_.notify) hooks_.notify("No hay memoria para el horizonte");
            tool_ = Tool::None; Show(root_, false); return;
        }
        horizon_canvas_ = lv_canvas_create(root_);
        if (!horizon_canvas_) {
            FreePixels(horizon_px_); horizon_px_ = nullptr;
            tool_ = Tool::None; Show(root_, false); return;
        }
        lv_canvas_set_buffer(horizon_canvas_, horizon_px_, w_, h_, LV_COLOR_FORMAT_RGB565);
        lv_obj_set_pos(horizon_canvas_, 0, 0);
        lv_obj_move_background(horizon_canvas_);
        horizon_zero_roll_ = horizon_zero_pitch_ = 0.0f;
        UpdateHorizon(Now());
        lv_obj_move_foreground(caption_);
        lv_obj_move_foreground(hint_);
    } else {
        lv_label_set_text(caption_, "DRAW PANEL");
        lv_label_set_text(hint_, "Toca para dibujar · máximo 1.000 puntos");
        canvas_px_ = AllocPixels(static_cast<size_t>(w_) * static_cast<size_t>(h_));
        if (!canvas_px_) {
            if (hooks_.notify) hooks_.notify("No hay memoria para dibujar");
            tool_ = Tool::None; Show(root_, false); return;
        }
        canvas_ = lv_canvas_create(root_);
        if (!canvas_) {
            FreePixels(canvas_px_); canvas_px_ = nullptr;
            tool_ = Tool::None; Show(root_, false); return;
        }
        lv_canvas_set_buffer(canvas_, canvas_px_, w_, h_, LV_COLOR_FORMAT_RGB565);
        lv_canvas_fill_bg(canvas_, lv_color_black(), LV_OPA_COVER);
        lv_obj_set_pos(canvas_, 0, 0);
        lv_obj_add_flag(canvas_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(canvas_, CanvasEvent, LV_EVENT_ALL, this);
        auto button = [&](const char* text) {
            lv_obj_t* b = Plain(root_, 104, 42);
            lv_obj_set_style_radius(b, 21, 0);
            lv_obj_set_style_bg_color(b, lv_color_hex(0x16092E), 0);
            lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(b, lv_color_hex(kLilac), 0);
            lv_obj_set_style_border_width(b, 2, 0);
            lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(b, ButtonEvent, LV_EVENT_CLICKED, this);
            lv_obj_t* l = Label(b, font_mid_, kPaleLilac);
            lv_label_set_text(l, text); lv_obj_center(l);
            return b;
        };
        canvas_close_ = button("Salir"); lv_obj_align(canvas_close_, LV_ALIGN_TOP_MID, 0, 8);
        canvas_clear_ = button("Borrar"); lv_obj_align(canvas_clear_, LV_ALIGN_BOTTOM_MID, 0, -8);
        dots_ = 0; draw_points_count_ = 0; pen_down_ = false; pen_color_ = 0;
        lv_obj_move_foreground(canvas_close_); lv_obj_move_foreground(canvas_clear_);
    }
}

void ChipoTools::OnKey() {
    switch (tool_) {
    case Tool::None: Open(Tool::Level); break;
    case Tool::Level: Close(); Open(Tool::GravityBall); break;
    case Tool::GravityBall: Close(); Open(Tool::Horizon); break;
    case Tool::Horizon: Close(); Open(Tool::Canvas); break;
    case Tool::Canvas: Close(); break;
    }
}

void ChipoTools::Close() {
    if (tool_ == Tool::None && !canvas_ && !horizon_canvas_) return;
    if (canvas_) {
        if (canvas_close_) lv_obj_delete(canvas_close_);
        if (canvas_clear_) lv_obj_delete(canvas_clear_);
        lv_obj_delete(canvas_);
        canvas_ = canvas_close_ = canvas_clear_ = nullptr;
    }
    if (canvas_px_) { FreePixels(canvas_px_); canvas_px_ = nullptr; }
    if (horizon_canvas_) { lv_obj_delete(horizon_canvas_); horizon_canvas_ = nullptr; }
    if (horizon_px_) { FreePixels(horizon_px_); horizon_px_ = nullptr; }
    pen_down_ = false; tool_ = Tool::None;
    Show(root_, false); Show(lvl_, false); Show(gravity_ball_, false);
}

// ---- voice, sensors ------------------------------------------------------------------------------------------------
bool ChipoTools::OnVoiceText(const char* text) {
    if (!running_) return false;
    const ToolsIntent intent = ParseToolsCommand(text, tool_ != Tool::None, tool_ == Tool::Canvas);
    switch (intent) {
        case ToolsIntent::None: return false;
        case ToolsIntent::Level: Open(Tool::Level); return Play("buddy_chime");
        case ToolsIntent::GravityBall: Open(Tool::GravityBall); return Play("buddy_chime");
        case ToolsIntent::Horizon: Open(Tool::Horizon); return Play("buddy_chime");
        case ToolsIntent::Canvas: Open(Tool::Canvas); return Play("buddy_chime");
        case ToolsIntent::ClearCanvas: ClearCanvas(); return Play("buddy_chime");
        case ToolsIntent::Close: Close(); return Play("buddy_chime");
        case ToolsIntent::SubtitlesOn:
            SetSubtitlesEnabled(true);
            if (hooks_.notify) hooks_.notify("Subtítulos activados");
            return Play("buddy_chime");
        case ToolsIntent::SubtitlesOff:
            SetSubtitlesEnabled(false);
            if (hooks_.notify) hooks_.notify("Subtítulos desactivados");
            return Play("buddy_chime");
        case ToolsIntent::InfoOn:
            SetInfoEnabled(true);
            if (hooks_.notify) hooks_.notify("Hora y tiempo en pantalla");
            return Play("buddy_chime");
        case ToolsIntent::InfoOff:
            SetInfoEnabled(false);
            if (hooks_.notify) hooks_.notify("Información oculta");
            return Play("buddy_chime");
    }
    return false;
}

void ChipoTools::SetInfoEnabled(bool on) {
    info_on_ = on;
    if (hooks_.save_flag) hooks_.save_flag("tools_info", on);
}

void ChipoTools::SetSubtitlesEnabled(bool on) {
    subs_on_ = on;
    if (!on) sub_hide_at_ = 0;
    if (hooks_.save_flag) hooks_.save_flag("tools_subs", on);
}

void ChipoTools::OnSample(const ImuSample& s, uint32_t now_ms) {
    if (!running_) return;
    if (!have_accel_) { ax_ = s.x; ay_ = s.y; az_ = s.z; have_accel_ = true; }
    else { ax_ += (s.x - ax_) * 0.25f; ay_ += (s.y - ay_) * 0.25f; az_ += (s.z - az_) * 0.25f; }
    if (tool_ == Tool::Level && level_calibrating_) {
        if (level_calibration_count_ == 0) {
            level_calibration_min_x_ = level_calibration_max_x_ = s.x;
            level_calibration_min_y_ = level_calibration_max_y_ = s.y;
        } else {
            level_calibration_min_x_ = std::min(level_calibration_min_x_, s.x);
            level_calibration_max_x_ = std::max(level_calibration_max_x_, s.x);
            level_calibration_min_y_ = std::min(level_calibration_min_y_, s.y);
            level_calibration_max_y_ = std::max(level_calibration_max_y_, s.y);
        }
        level_calibration_sum_x_ += s.x;
        level_calibration_sum_y_ += s.y;
        ++level_calibration_count_;
        if (level_calibration_count_ >= 200) {
            const float range_x = level_calibration_max_x_ - level_calibration_min_x_;
            const float range_y = level_calibration_max_y_ - level_calibration_min_y_;
            level_calibrating_ = false;
            if (range_x <= 0.1f && range_y <= 0.1f) {
                const float mx = level_calibration_sum_x_ / level_calibration_count_;
                const float my = level_calibration_sum_y_ / level_calibration_count_;
                const float mag = std::sqrt(mx * mx + my * my + az_ * az_);
                if (mag > 0.5f && mag < 1.6f) {
                    level_zero_x_ = mx / mag;
                    level_zero_y_ = my / mag;
                    level_calibrated_ = true;
                    bubble_x_ = bubble_y_ = 0.0f;
                    if (hooks_.notify) hooks_.notify("Nivel calibrado");
                } else if (hooks_.notify) hooks_.notify("Calibración no válida");
            } else if (hooks_.notify) hooks_.notify("CHIPO se movió; vuelve a calibrar");
        }
    }
    if (!shake_.Feed(s.x, s.y, s.z, now_ms)) return;
    ++shakes_;
    // `now_ms` is the sensor's clock; everything else here runs on the UI clock.
    if (tool_ == Tool::None && hooks_.shake_other) hooks_.shake_other(Now());
}


void ChipoTools::ShowSubtitle(const char* text) {
    if (!running_ || !subs_on_ || !text || !*text) return;
    std::string shown = text_has_accents_ ? std::string(text) : FoldAccents(text);
    CopyUtf8(sub_text_, sizeof sub_text_, shown.c_str());
    lv_label_set_text(sub_, sub_text_);
    sub_visible_ = true;
    sub_opa_ = 255;
    const uint32_t ms = std::min<uint32_t>(9000u, 2800u + 55u * static_cast<uint32_t>(std::strlen(sub_text_)));
    sub_hide_at_ = Now() + ms;
    Show(sub_, true);
    lv_obj_set_style_opa(sub_, LV_OPA_COVER, 0);
}

bool ChipoTools::CalibrateLevel() {
    if (tool_ != Tool::Level || !have_accel_) return false;
    const float mag = std::sqrt(ax_ * ax_ + ay_ * ay_ + az_ * az_);
    const LevelReading raw = LevelFromAccel(ax_, ay_, az_,
                                            CHIPO_LEVEL_MIRROR_X != 0, CHIPO_LEVEL_MIRROR_Y != 0);
    if (!raw.flat || mag < 0.5f || mag > 1.6f) {
        if (hooks_.notify) hooks_.notify("Ponme plano para calibrar");
        return false;
    }
    level_calibration_count_ = 0;
    level_calibration_sum_x_ = level_calibration_sum_y_ = 0.0f;
    level_calibration_min_x_ = level_calibration_min_y_ = 0.0f;
    level_calibration_max_x_ = level_calibration_max_y_ = 0.0f;
    level_calibrating_ = true;
    level_calibrated_ = false;
    if (hooks_.notify) hooks_.notify("Calibrando: mantén CHIPO quieto");
    return true;
}

// ---- canvas --------------------------------------------------------------------------------------------------------
void ChipoTools::ClearCanvas() {
    if (!canvas_) return;
    lv_canvas_fill_bg(canvas_, lv_color_black(), LV_OPA_COVER);
    dots_ = 0;
    draw_points_count_ = 0;
    pen_color_ = 0;
}

void ChipoTools::DrawDot(int cx, int cy, int r) {
    if (!canvas_px_) return;
    const uint16_t v = lv_color_to_u16(lv_color_hex(0xFF3030));
    for (int dy = -r; dy <= r; ++dy) {
        const int y = cy + dy;
        if (y < 0 || y >= h_) continue;
        for (int dx = -r; dx <= r; ++dx) {
            const int x = cx + dx;
            if (x < 0 || x >= w_ || dx * dx + dy * dy > r * r) continue;
            canvas_px_[y * w_ + x] = v;
            ++dots_;
        }
    }
}

void ChipoTools::RedrawCanvas() {
    if (!canvas_px_ || !canvas_) return;
    lv_canvas_fill_bg(canvas_, lv_color_black(), LV_OPA_COVER);
    dots_ = 0;
    for (size_t i = 0; i < draw_points_count_; ++i) {
        DrawDot(draw_points_[i].x, draw_points_[i].y, 5);
    }
    lv_area_t area{0, 0, w_ - 1, h_ - 1};
    lv_obj_invalidate_area(canvas_, &area);
}

void ChipoTools::AddDrawPoint(int x, int y) {
    if (!canvas_ || !canvas_px_) return;
    constexpr int radius = 5;  // Waveshare DrawPanel uses 10 px dots.
    x = std::clamp(x, radius, w_ - radius - 1);
    y = std::clamp(y, radius, h_ - radius - 1);
    const int dx = x - (w_ - 1) / 2;
    const int dy = y - (h_ - 1) / 2;
    const int circle_radius = std::min(w_, h_) / 2 - radius;
    if (dx * dx + dy * dy > circle_radius * circle_radius) return;

    const bool evict_oldest = draw_points_count_ == draw_points_.size();
    if (evict_oldest) {
        for (size_t i = 1; i < draw_points_count_; ++i) draw_points_[i - 1] = draw_points_[i];
        --draw_points_count_;
    }
    draw_points_[draw_points_count_++] = lv_point_t{x, y};
    if (evict_oldest) {
        RedrawCanvas();
    } else {
        DrawDot(x, y, radius);
        lv_area_t area{x - radius - 1, y - radius - 1, x + radius + 1, y + radius + 1};
        area.x1 = std::max(0, area.x1); area.y1 = std::max(0, area.y1);
        area.x2 = std::min(w_ - 1, area.x2); area.y2 = std::min(h_ - 1, area.y2);
        lv_obj_invalidate_area(canvas_, &area);
    }
}

void ChipoTools::DrawSegment(int x0, int y0, int x1, int y1) {
    if (!canvas_) return;
    const int dx = x1 - x0, dy = y1 - y0;
    const int steps = std::max(1, static_cast<int>(std::sqrt(static_cast<float>(dx * dx + dy * dy)) / 2.0f));
    for (int i = 0; i <= steps; ++i) {
        DrawDot(x0 + dx * i / steps, y0 + dy * i / steps, kPenRadius);
    }
    lv_area_t a;
    a.x1 = std::max(0, std::min(x0, x1) - kPenRadius - 1);
    a.y1 = std::max(0, std::min(y0, y1) - kPenRadius - 1);
    a.x2 = std::min(w_ - 1, std::max(x0, x1) + kPenRadius + 1);
    a.y2 = std::min(h_ - 1, std::max(y0, y1) + kPenRadius + 1);
    lv_obj_invalidate_area(canvas_, &a);
}

void ChipoTools::PenDown(int x, int y) {
    if (tool_ != Tool::Canvas) return;
    pen_down_ = true;
    pen_x_ = x;
    pen_y_ = y;
    AddDrawPoint(x, y);
}

void ChipoTools::PenMove(int x, int y) {
    if (tool_ != Tool::Canvas || !pen_down_) return;
    pen_x_ = x;
    pen_y_ = y;
    AddDrawPoint(x, y);
}

void ChipoTools::PenUp() {
    if (!pen_down_) return;
    pen_down_ = false;
    pen_color_ = 0;  // DrawPanel uses one red point colour
}

// ---- LVGL callbacks ------------------------------------------------------------------------------------------------
void ChipoTools::TimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoTools*>(lv_timer_get_user_data(timer));
    self->Step(self->Now());
}

void ChipoTools::RootEvent(lv_event_t* e) {
    auto* self = static_cast<ChipoTools*>(lv_event_get_user_data(e));
    if (!self || self->tool_ == Tool::None || self->tool_ == Tool::Canvas) return;
    const uint32_t now = self->Now();
    if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) {
        self->last_long_ms_ = now;
        self->Close();
        return;
    }
    if (now - self->last_long_ms_ < 400) return;  // the release that ends a long press is not a tap
    if (self->tool_ == Tool::Level) {
        // A tap records the current flat surface as the level's zero. A long press still exits.
        const LevelReading raw = LevelFromAccel(self->ax_, self->ay_, self->az_,
                                                CHIPO_LEVEL_MIRROR_X != 0, CHIPO_LEVEL_MIRROR_Y != 0);
        if (!self->have_accel_ || !raw.flat) {
            if (self->hooks_.notify) self->hooks_.notify("Ponme plano para calibrar");
            return;
        }
        const float mag = std::sqrt(self->ax_ * self->ax_ + self->ay_ * self->ay_ + self->az_ * self->az_);
        if (mag < 0.5f || mag > 1.6f) return;
        self->level_zero_x_ = self->ax_ / mag;
        self->level_zero_y_ = self->ay_ / mag;
        self->level_calibrated_ = true;
        self->bubble_x_ = self->bubble_y_ = 0.0f;
        if (self->hooks_.notify) self->hooks_.notify("Nivel calibrado");
    } else if (self->tool_ == Tool::Horizon && self->have_accel_) {
        const float mag = std::sqrt(self->ax_ * self->ax_ + self->ay_ * self->ay_ + self->az_ * self->az_);
        if (mag > 0.5f && mag < 1.6f) {
            self->horizon_zero_roll_ = std::atan2(self->ay_, self->az_) * 57.2957795f;
            self->horizon_zero_pitch_ = std::atan2(-self->ax_, std::sqrt(self->ay_ * self->ay_ + self->az_ * self->az_)) * 57.2957795f;
            if (self->hooks_.notify) self->hooks_.notify("Horizonte calibrado");
        }
    }
}

void ChipoTools::CanvasEvent(lv_event_t* e) {
    auto* self = static_cast<ChipoTools*>(lv_event_get_user_data(e));
    if (!self || self->tool_ != Tool::Canvas) return;
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) return;
    lv_point_t p{0, 0};
    lv_indev_t* in = lv_event_get_indev(e);
    if (in) lv_indev_get_point(in, &p);
    lv_area_t area;
    lv_obj_get_coords(self->canvas_, &area);
    const int x = p.x - area.x1;
    const int y = p.y - area.y1;
    if (code == LV_EVENT_PRESSED) self->PenDown(x, y);
    else if (code == LV_EVENT_PRESSING) self->PenMove(x, y);
    else self->PenUp();
}

void ChipoTools::ButtonEvent(lv_event_t* e) {
    auto* self = static_cast<ChipoTools*>(lv_event_get_user_data(e));
    if (!self) return;
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (target == self->canvas_close_) self->Close();
    else if (target == self->canvas_clear_) self->ClearCanvas();
}

// ---- per tick ------------------------------------------------------------------------------------------------------
void ChipoTools::SetOpa(lv_obj_t* obj, int opa) {
    if (!obj) return;
    lv_obj_set_style_opa(obj, static_cast<lv_opa_t>(std::clamp(opa, 0, 255)), 0);
    Show(obj, opa > 0);
}

void ChipoTools::Step(uint32_t now_ms) {
    if (!running_) return;
    UpdateStrip(now_ms);
    UpdateRing(now_ms);
    if (tool_ == Tool::Level) UpdateLevel(now_ms);
    else if (tool_ == Tool::GravityBall) UpdateGravityBall(now_ms);
    else if (tool_ == Tool::Horizon) UpdateHorizon(now_ms);
    UpdateSubtitle(now_ms);
}

void ChipoTools::UpdateStrip(uint32_t now_ms) {
    // Look at the world twice a second; fade every tick.
    if (now_ms - last_strip_ms_ >= 500 || last_strip_ms_ == 0) {
        last_strip_ms_ = now_ms;
        int hour = 0, minute = 0;
        const bool have_time = hooks_.wall_time && hooks_.wall_time(&hour, &minute);
        bool calm = tool_ == Tool::None && info_on_ && have_time;
        if (calm && hooks_.face_calm) calm = hooks_.face_calm();
        if (calm && hooks_.other_screen) calm = !hooks_.other_screen();
        strip_target_ = calm;
        if (have_time) {
            char text[8];
            if (FormatClock(hour, minute, text, sizeof text) && std::strcmp(text, time_text_) != 0) {
                std::strcpy(time_text_, text);
                lv_label_set_text(time_label_, time_text_);
                // Burn-in: the strip drifts a couple of pixels every few minutes.
                const int dx = ((minute / 5) % 3 - 1) * 2;
                lv_obj_align(time_label_, LV_ALIGN_TOP_MID, dx, std::max(18, std::min(w_, h_) / 22) + ((minute / 15) % 2) * 2);
            }
        } else {
            time_text_[0] = '\0';
        }
        int temp = 0;
        const bool have_temp = hooks_.temperature && hooks_.temperature(&temp);
        if (have_temp && (!temp_shown_ || temp != temp_c_)) {
            temp_c_ = temp;
            char buf[8];
            std::snprintf(buf, sizeof buf, "%d", temp);
            lv_label_set_text(temp_num_, buf);
        }
        temp_shown_ = have_temp;
        if (info_on_ && hooks_.refresh_weather && static_cast<int32_t>(now_ms - next_prefetch_ms_) >= 0) {
            next_prefetch_ms_ = now_ms + kPrefetchEveryMs;
            hooks_.refresh_weather();
        }
    }
    strip_opa_ = Approach(strip_opa_, strip_target_ ? kStripOpa : 0, 14);
    SetOpa(time_label_, time_text_[0] ? static_cast<int>(strip_opa_) : 0);
    SetOpa(temp_row_, temp_shown_ ? static_cast<int>(strip_opa_) : 0);
}

void ChipoTools::UpdateRing(uint32_t now_ms) {
    if (now_ms - last_ring_ms_ >= 500 || last_ring_ms_ == 0) {
        last_ring_ms_ = now_ms;
        float frac = 0;
        bool work = true;
        bool on = tool_ == Tool::None && hooks_.pomodoro && hooks_.pomodoro(&frac, &work);
        if (on && hooks_.other_screen) on = !hooks_.other_screen();
        ring_target_ = on;
        if (on) {
            ring_value_ = static_cast<int>(std::clamp(frac, 0.0f, 1.0f) * 1000.0f + 0.5f);
            if (work != ring_work_ || lv_arc_get_value(ring_) != ring_value_) {
                ring_work_ = work;
                lv_obj_set_style_arc_color(ring_, lv_color_hex(work ? kAmber : kGreen), LV_PART_INDICATOR);
                lv_arc_set_value(ring_, ring_value_);
            }
        }
    }
    ring_opa_ = Approach(ring_opa_, ring_target_ ? kRingOpa : 0, 16);
    SetOpa(ring_, static_cast<int>(ring_opa_));
}

void ChipoTools::UpdateLevel(uint32_t) {
    char buf[32];
    if (!have_accel_) {
        lv_label_set_text(lvl_angle_, level_calibrating_ ? "Mantén CHIPO quieto" : "");
        Show(lvl_bubble_, false);
        return;
    }
    const float mag = std::sqrt(ax_ * ax_ + ay_ * ay_ + az_ * az_);
    const float zero_x = level_calibrated_ ? level_zero_x_ * mag : 0.0f;
    const float zero_y = level_calibrated_ ? level_zero_y_ * mag : 0.0f;
    level_ = LevelFromAccel(ax_ - zero_x, ay_ - zero_y, az_,
                            CHIPO_LEVEL_MIRROR_X != 0, CHIPO_LEVEL_MIRROR_Y != 0);
    const int side = std::min(w_, h_);
    const float radius = static_cast<float>(side - 120) / 2.0f - static_cast<float>(side / 8) / 2.0f - 6.0f;
    const float tx = level_.flat ? level_.x * radius : 0.0f;
    const float ty = level_.flat ? level_.y * radius : 0.0f;
    bubble_x_ += (tx - bubble_x_) * 0.35f;
    bubble_y_ += (ty - bubble_y_) * 0.35f;
    Show(lvl_bubble_, level_.flat);
    lv_obj_align(lvl_bubble_, LV_ALIGN_CENTER, static_cast<int>(std::lround(bubble_x_)), static_cast<int>(std::lround(bubble_y_)));
    lv_obj_set_style_bg_color(lvl_bubble_, lv_color_hex(level_.level ? kGreen : kLilac), 0);
    lv_obj_set_style_border_color(lvl_target_, lv_color_hex(level_.level ? kGreen : kCyan), 0);
    if (!level_.flat) {
        lv_label_set_text(caption_, level_calibrating_ ? "CALIBRANDO..." : "Ponme plano sobre la mesa");
        lv_label_set_text(lvl_angle_, "");
    } else {
        lv_label_set_text(caption_, level_.level ? "Nivelado" : (level_calibrated_ ? "Nivel - calibrado" : "Nivel"));
        std::snprintf(buf, sizeof buf, "%.1f grados", static_cast<double>(level_.angle_deg));
        lv_label_set_text(lvl_angle_, buf);
    }
}

void ChipoTools::UpdateGravityBall(uint32_t) {
    if (!gravity_ball_dot_ || !have_accel_) return;
    const float mag = std::sqrt(ax_ * ax_ + ay_ * ay_ + az_ * az_);
    if (mag < 0.55f || mag > 1.55f) return;
    const float limit = static_cast<float>(std::min(w_, h_) - 104 - w_ / 9) / 2.0f;
    const float tx = std::clamp(ax_ / mag, -1.0f, 1.0f) * limit;
    const float ty = std::clamp(ay_ / mag, -1.0f, 1.0f) * limit;
    gravity_vx_ = (gravity_vx_ + (tx - gravity_x_) * 0.10f) * 0.82f;
    gravity_vy_ = (gravity_vy_ + (ty - gravity_y_) * 0.10f) * 0.82f;
    gravity_x_ = std::clamp(gravity_x_ + gravity_vx_, -limit, limit);
    gravity_y_ = std::clamp(gravity_y_ + gravity_vy_, -limit, limit);
    lv_obj_align(gravity_ball_dot_, LV_ALIGN_CENTER, static_cast<int>(std::lround(gravity_x_)), static_cast<int>(std::lround(gravity_y_)));
}

void ChipoTools::UpdateHorizon(uint32_t) {
    if (!horizon_px_ || !horizon_canvas_ || !have_accel_) return;
    const float mag = std::sqrt(ax_ * ax_ + ay_ * ay_ + az_ * az_);
    if (mag < 0.55f || mag > 1.55f) return;
    horizon_roll_ = std::clamp(std::atan2(ay_, az_) * 57.2957795f - horizon_zero_roll_, -89.0f, 89.0f);
    horizon_pitch_ = std::clamp(std::atan2(-ax_, std::sqrt(ay_ * ay_ + az_ * az_)) * 57.2957795f - horizon_zero_pitch_, -45.0f, 45.0f);
    const float rad = horizon_roll_ * 0.0174532925f, sn = std::sin(rad), cs = std::cos(rad);
    const float pitch_px = horizon_pitch_ * 5.0f;
    const uint16_t sky = lv_color_to_u16(lv_color_hex(0x1FA9D8));
    const uint16_t earth = lv_color_to_u16(lv_color_hex(0xBF5A06));
    const uint16_t white = lv_color_to_u16(lv_color_hex(0xFFFFFF));
    const uint16_t mark = lv_color_to_u16(lv_color_hex(0xFFF2A8));
    const int cx = w_ / 2, cy = h_ / 2;
    for (int y = 0; y < h_; ++y) {
        const float dy = static_cast<float>(y - cy);
        for (int x = 0; x < w_; ++x) {
            const float dx = static_cast<float>(x - cx);
            const float d = -dx * sn + dy * cs + pitch_px;
            uint16_t color = d >= 0.0f ? earth : sky;
            if (std::fabs(d) < 1.4f) color = white;
            const int rx = std::abs(x - cx), ry = std::abs(y - cy);
            if ((ry <= 2 && rx >= 30 && rx <= 88) || (rx <= 2 && ry <= 14)) color = mark;
            horizon_px_[y * w_ + x] = color;
        }
    }
    lv_obj_invalidate(horizon_canvas_);
}

void ChipoTools::UpdateSubtitle(uint32_t now_ms) {
    if (!sub_visible_) return;
    const int32_t left = static_cast<int32_t>(sub_hide_at_ - now_ms);
    if (!subs_on_ || left <= 0) {
        sub_opa_ = Approach(sub_opa_, 0, 28);
        if (sub_opa_ <= 0) {
            sub_visible_ = false;
            Show(sub_, false);
            return;
        }
    } else if (left < 400) {
        sub_opa_ = static_cast<float>(left) * 255.0f / 400.0f;
    }
    lv_obj_set_style_opa(sub_, static_cast<lv_opa_t>(std::clamp(static_cast<int>(sub_opa_), 0, 255)), 0);
}

ChipoTools::DebugInfo ChipoTools::Debug() const {
    DebugInfo d{};
    d.tool = tool_;
    d.strip_visible = time_label_ && !lv_obj_has_flag(time_label_, LV_OBJ_FLAG_HIDDEN);
    std::snprintf(d.time_text, sizeof d.time_text, "%s", time_text_);
    d.temp_visible = temp_row_ && !lv_obj_has_flag(temp_row_, LV_OBJ_FLAG_HIDDEN);
    d.temp_c = temp_c_;
    d.ring_visible = ring_ && !lv_obj_has_flag(ring_, LV_OBJ_FLAG_HIDDEN);
    d.ring_value = ring_value_;
    d.ring_work = ring_work_;
    d.level_flat = level_.flat;
    d.level_ok = level_.level;
    d.level_calibrating = level_calibrating_;
    d.level_calibrated = level_calibrated_;
    d.bubble_x = bubble_x_;
    d.bubble_y = bubble_y_;
    d.angle_deg = level_.angle_deg;
    d.gravity_x = gravity_x_; d.gravity_y = gravity_y_;
    d.horizon_roll = horizon_roll_; d.horizon_pitch = horizon_pitch_;
    d.subtitle_visible = sub_visible_ && sub_ && !lv_obj_has_flag(sub_, LV_OBJ_FLAG_HIDDEN);
    std::snprintf(d.subtitle_text, sizeof d.subtitle_text, "%s", sub_text_);
    d.canvas_dots = dots_;
    d.shakes = shakes_;
    return d;
}
