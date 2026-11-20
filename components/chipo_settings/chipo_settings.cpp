#include "chipo_settings.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t kTickMs = 1000;
constexpr uint32_t kMenuIdleCloseMs = 30000;
constexpr uint32_t kBatteryEveryMs = 5000;
constexpr uint32_t kRestartArmMs = 3000;
constexpr uint32_t kCyan = 0x00E5FF;
constexpr uint32_t kPink = 0xFF2E93;
constexpr uint32_t kGreen = 0x39FF88;
constexpr uint32_t kAmber = 0xFFB000;
constexpr uint32_t kRed = 0xFF3B30;

lv_obj_t* Box(lv_obj_t* parent, int x, int y, int w, int h, int radius, uint32_t color, int opa = 255) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, static_cast<lv_opa_t>(opa), 0);
    lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    return o;
}

lv_obj_t* Text(lv_obj_t* parent, const lv_font_t* font, int x, int y, int w, uint32_t color, lv_text_align_t align) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, "");
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(l, align, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_size(l, w, 30);
    return l;
}

// A neon outlined button; returns it, the label is its first child.
lv_obj_t* Button(lv_obj_t* parent, const lv_font_t* font, int x, int y, int w, int h, const char* text, uint32_t color) {
    lv_obj_t* b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, 14, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x001018), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(b, 3, 0);
    lv_obj_set_style_shadow_color(b, lv_color_hex(color), 0);
    lv_obj_set_style_shadow_width(b, 12, 0);
    lv_obj_set_style_shadow_opa(b, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_40, LV_STATE_PRESSED);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_center(l);
    return b;
}

lv_obj_t* Bar(lv_obj_t* parent, int x, int y, int w) {
    lv_obj_t* b = lv_bar_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, 16);
    lv_bar_set_range(b, 0, 100);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x0A1A22), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(b, lv_color_hex(kCyan), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(b, lv_color_hex(kPink), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
    return b;
}
}  // namespace

ChipoSettings::ChipoSettings() = default;
ChipoSettings::~ChipoSettings() { Stop(); }

uint32_t ChipoSettings::Now() const { return hooks_.clock ? hooks_.clock() : lv_tick_get(); }

bool ChipoSettings::Start(lv_obj_t* parent, Hooks hooks, const char* version, const lv_font_t* font) {
    if (running_) return true;
    if (!parent) return false;
    hooks_ = std::move(hooks);
#if LV_FONT_MONTSERRAT_24
    if (!font) font = &lv_font_montserrat_24;
#endif
    if (!font) font = lv_font_get_default();
    int32_t w = lv_obj_get_width(parent), h = lv_obj_get_height(parent);
    if (w <= 0) w = 368;
    if (h <= 0) h = 448;
    (void)w; (void)h;
    Build(font, version ? version : "");
    if (!menu_ || !indicator_) {
        Stop();
        return false;
    }
    timer_ = lv_timer_create(TimerCallback, kTickMs, this);
    for (lv_indev_t* in = lv_indev_get_next(nullptr); in; in = lv_indev_get_next(in)) {
        if (lv_indev_get_type(in) == LV_INDEV_TYPE_POINTER) {
            lv_indev_add_event_cb(in, IndevCallback, LV_EVENT_PRESSED, this);
            lv_indev_add_event_cb(in, IndevCallback, LV_EVENT_LONG_PRESSED, this);
            lv_indev_add_event_cb(in, IndevCallback, LV_EVENT_CLICKED, this);
            lv_indev_add_event_cb(in, IndevCallback, LV_EVENT_PRESSING, this);
            lv_indev_add_event_cb(in, IndevCallback, LV_EVENT_RELEASED, this);
        }
    }
    start_ms_ = last_activity_ms_ = last_battery_ms_ = Now();
    if (hooks_.get_volume) volume_ = hooks_.get_volume();
    if (hooks_.get_brightness) user_brightness_ = brightness_ = hooks_.get_brightness();
    running_ = true;
    return true;
}

void ChipoSettings::Stop() {
    if (running_) {
        for (lv_indev_t* in = lv_indev_get_next(nullptr); in; in = lv_indev_get_next(in)) {
            lv_indev_remove_event_cb_with_user_data(in, IndevCallback, this);
        }
    }
    if (timer_) lv_timer_delete(timer_);
    timer_ = nullptr;
    if (dimmed_ && hooks_.set_brightness) hooks_.set_brightness(user_brightness_, false);
    dimmed_ = false;
    if (menu_) lv_obj_delete(menu_);
    if (indicator_) lv_obj_delete(indicator_);
    menu_ = bar_vol_ = bar_bri_ = val_vol_ = val_bri_ = info_bat_ = info_wifi_ = info_ver_ = restart_label_ = nullptr;
    indicator_ = indicator_fill_ = indicator_text_ = nullptr;
    open_ = false;
    running_ = false;
}

void ChipoSettings::Build(const lv_font_t* font, const char* version) {
    // The menu layout was drawn for 368 x 448; on a bigger screen (2.16": 480 x 480) it is centred in
    // a full-screen black menu_, so nothing else shows through and the content stays clear of the corners.
    constexpr int kLayoutW = 368, kLayoutH = 448;
    int w = static_cast<int>(lv_display_get_horizontal_resolution(nullptr));
    int h = static_cast<int>(lv_display_get_vertical_resolution(nullptr));
    if (w <= 0) w = kLayoutW;
    if (h <= 0) h = kLayoutH;
    // ---- battery indicator (top centre, only while charging or low) ----------------------------
    indicator_ = Box(lv_screen_active(), (w - 120) / 2, 8, 120, 34, 8, 0x000000, 0);
    lv_obj_set_style_border_width(indicator_, 3, 0);
    lv_obj_set_style_border_color(indicator_, lv_color_hex(kGreen), 0);
    lv_obj_set_style_border_opa(indicator_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(indicator_, LV_OPA_TRANSP, 0);
    indicator_fill_ = Box(indicator_, 3, 3, 10, 22, 4, kGreen);
    indicator_text_ = Text(indicator_, font, 18, 0, 96, kGreen, LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_height(indicator_text_, 28);
    lv_obj_add_flag(indicator_, LV_OBJ_FLAG_HIDDEN);

    // ---- menu ---------------------------------------------------------------------------------
    menu_ = Box(lv_screen_active(), 0, 0, w, h, 0, 0x000000);
    lv_obj_add_flag(menu_, LV_OBJ_FLAG_CLICKABLE);  // swallows the taps: nothing under the menu reacts
    lv_obj_add_flag(menu_, LV_OBJ_FLAG_HIDDEN);
    const int ox = w > kLayoutW ? (w - kLayoutW) / 2 : 0;
    const int oy = h > kLayoutH ? (h - kLayoutH) / 2 : 0;
    lv_obj_t* content = Box(menu_, ox, oy, kLayoutW, kLayoutH, 0, 0x000000, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_t* title = Text(content, font, 0, 18, kLayoutW, kCyan, LV_TEXT_ALIGN_CENTER);
    lv_label_set_text(title, "AJUSTES");
    lv_obj_set_style_text_letter_space(title, 4, 0);

    lv_obj_t* l_vol = Text(content, font, 24, 68, 160, 0xFFFFFF, LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(l_vol, "Volumen");
    val_vol_ = Text(content, font, 204, 68, 140, kCyan, LV_TEXT_ALIGN_RIGHT);
    lv_obj_t* b0 = Button(content, font, 24, 104, 64, 50, "-", kCyan);
    bar_vol_ = Bar(content, 102, 121, 164);
    lv_obj_t* b1 = Button(content, font, 280, 104, 64, 50, "+", kCyan);

    lv_obj_t* l_bri = Text(content, font, 24, 176, 160, 0xFFFFFF, LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(l_bri, "Brillo");
    val_bri_ = Text(content, font, 204, 176, 140, kPink, LV_TEXT_ALIGN_RIGHT);
    lv_obj_t* b2 = Button(content, font, 24, 212, 64, 50, "-", kPink);
    bar_bri_ = Bar(content, 102, 229, 164);
    lv_obj_t* b3 = Button(content, font, 280, 212, 64, 50, "+", kPink);

    info_bat_ = Text(content, font, 24, 292, 320, kGreen, LV_TEXT_ALIGN_LEFT);
    info_wifi_ = Text(content, font, 24, 326, 320, kCyan, LV_TEXT_ALIGN_LEFT);
    info_ver_ = Text(content, font, 24, 360, 320, 0x9AA7B2, LV_TEXT_ALIGN_LEFT);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "CHIPO OS %s", version);
    lv_label_set_text(info_ver_, buf);

    lv_obj_t* b4 = Button(content, font, 24, 394, 152, 46, "Reiniciar", kPink);
    restart_label_ = lv_obj_get_child(b4, 0);
    lv_obj_t* b5 = Button(content, font, 192, 394, 152, 46, "Cerrar", kCyan);

    lv_obj_t* buttons[kActionCount] = {b0, b1, b2, b3, b4, b5};
    for (int i = 0; i < kActionCount; ++i) {
        btn_[i] = Btn{this, i};
        lv_obj_add_event_cb(buttons[i], ButtonCallback, LV_EVENT_CLICKED, &btn_[i]);
    }
}

void ChipoSettings::Refresh() {
    if (!menu_) return;
    char buf[48];
    lv_bar_set_value(bar_vol_, volume_, LV_ANIM_OFF);
    std::snprintf(buf, sizeof(buf), "%d %%", volume_);
    lv_label_set_text(val_vol_, buf);
    lv_bar_set_value(bar_bri_, brightness_, LV_ANIM_OFF);
    std::snprintf(buf, sizeof(buf), "%d %%", brightness_);
    lv_label_set_text(val_bri_, buf);
    lv_label_set_text(info_bat_, (std::string("Bateria  ") + battery_.text).c_str());
    WifiView wv;
    if (hooks_.wifi) hooks_.wifi(&wv);
    lv_label_set_text(info_wifi_, ("Wi-Fi  " + WifiText(wv)).c_str());
}

void ChipoSettings::Open() {
    if (!running_ || open_) return;
    const bool was_dimmed = dimmed_;
    NotifyActivity();  // wakes the screen first
    if (hooks_.get_volume) volume_ = hooks_.get_volume();
    // While the saver was on, the backlight reads the dimmed level (or a value in transition): use the user's own.
    if (!was_dimmed && hooks_.get_brightness) user_brightness_ = hooks_.get_brightness();
    brightness_ = user_brightness_;
    open_ = true;
    opened_ms_ = Now();
    restart_armed_until_ms_ = 0;
    lv_label_set_text(restart_label_, "Reiniciar");
    last_battery_ms_ = Now() - kBatteryEveryMs;  // read the battery right now
    UpdateBattery(Now());
    Refresh();
    lv_obj_remove_flag(menu_, LV_OBJ_FLAG_HIDDEN);
}

void ChipoSettings::Close() {
    if (!running_ || !open_) return;
    open_ = false;
    lv_obj_add_flag(menu_, LV_OBJ_FLAG_HIDDEN);
}

void ChipoSettings::NotifyActivity() {
    last_activity_ms_ = Now();
    if (dimmed_) {
        dimmed_ = false;
        dim_percent_ = 100;
        if (hooks_.set_brightness) hooks_.set_brightness(user_brightness_, false);
    }
}

void ChipoSettings::OnPressed() {
    NotifyActivity();
    if (open_) opened_ms_ = Now();  // touching the menu keeps it open
}

void ChipoSettings::OnLongPress() {
    if (!running_ || open_) return;
    if (hooks_.overlay_active && hooks_.overlay_active()) return;
    Open();
}

bool ChipoSettings::BlocksWeatherTap(int y) const {
    if (open_) return true;
    if (hooks_.overlay_active && hooks_.overlay_active()) return true;
    return InFaceBand(y);
}

void ChipoSettings::OnTap(int x, int y) {
    (void)x;
    if (!running_ || open_) return;
    if (hooks_.overlay_active && hooks_.overlay_active()) return;
    if (!InFaceBand(y)) return;
    const TapKind kind = taps_.OnTap(Now());
    if (kind == TapKind::Tickle && hooks_.play_clip) hooks_.play_clip("murmur_2");  // a giggle
    if (hooks_.react) hooks_.react(kind);
}

void ChipoSettings::OnTouchMove(int x, int y) {
    if (!running_ || open_ || !hooks_.gaze) return;
    if (hooks_.overlay_active && hooks_.overlay_active()) return;
    int32_t w = lv_display_get_horizontal_resolution(nullptr);
    int32_t h = lv_display_get_vertical_resolution(nullptr);
    if (w <= 0) w = 368;
    if (h <= 0) h = 448;
    float nx = (static_cast<float>(x) - static_cast<float>(w) / 2.0f) / (static_cast<float>(w) / 2.0f);
    float ny = (static_cast<float>(y) - static_cast<float>(h) / 2.0f) / (static_cast<float>(h) / 2.0f);
    hooks_.gaze(nx < -1 ? -1 : (nx > 1 ? 1 : nx), ny < -1 ? -1 : (ny > 1 ? 1 : ny), true);
}

void ChipoSettings::OnTouchEnd() {
    if (running_ && hooks_.gaze) hooks_.gaze(0, 0, false);
}

void ChipoSettings::Press(int action) {
    if (!running_ || !open_) return;
    opened_ms_ = Now();
    switch (action) {
    case kVolDown:
    case kVolUp:
        volume_ = SettingsStep(volume_, action == kVolUp ? 1 : -1, 0, 100);
        if (hooks_.set_volume) hooks_.set_volume(volume_);
        if (hooks_.play_clip) hooks_.play_clip("buddy_chime");  // so the new level can be heard
        break;
    case kBriDown:
    case kBriUp:
        brightness_ = SettingsStep(brightness_, action == kBriUp ? 1 : -1, 5, 100);
        user_brightness_ = brightness_;
        if (hooks_.set_brightness) hooks_.set_brightness(brightness_, true);
        break;
    case kRestart:
        if (Now() < restart_armed_until_ms_) {
            if (hooks_.restart) hooks_.restart();
        } else {
            restart_armed_until_ms_ = Now() + kRestartArmMs;
            lv_label_set_text(restart_label_, "Seguro?");
        }
        break;
    case kClose:
        Close();
        break;
    default:
        break;
    }
    Refresh();
}

void ChipoSettings::UpdateBattery(uint32_t now_ms) {
    if (now_ms - last_battery_ms_ < kBatteryEveryMs) return;
    last_battery_ms_ = now_ms;
    int level = -1;
    bool charging = false, discharging = false;
    const bool ok = hooks_.battery && hooks_.battery(&level, &charging, &discharging);
    battery_ = ClassifyBattery(ok, level, charging, discharging);
    int threshold = 0;
    if (BatteryCrossedWarning(last_level_, battery_.percent, &threshold) && !charging) {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "Batería baja: %d %%", threshold);
        if (hooks_.notify) hooks_.notify(buf);
        if (hooks_.play_clip) hooks_.play_clip("buddy_timer_done");
    }
    last_level_ = battery_.percent;
    const bool show = BatteryIndicatorVisible(battery_);
    if (show != indicator_visible_) {
        indicator_visible_ = show;
        if (show) lv_obj_remove_flag(indicator_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(indicator_, LV_OBJ_FLAG_HIDDEN);
    }
    if (show) {
        const uint32_t color = battery_.state == BatteryState::Charging ? kGreen
                             : battery_.state == BatteryState::Critical ? kRed : kAmber;
        lv_obj_set_style_border_color(indicator_, lv_color_hex(color), 0);
        lv_obj_set_style_bg_color(indicator_fill_, lv_color_hex(color), 0);
        lv_obj_set_style_text_color(indicator_text_, lv_color_hex(color), 0);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d %%", battery_.percent);
        lv_label_set_text(indicator_text_, buf);
        lv_obj_set_width(indicator_fill_, std::max(4, (battery_.percent * 14) / 100));
    }
    if (open_) Refresh();
}

void ChipoSettings::ApplyBrightness(int user_percent, int dim_percent) {
    if (!hooks_.set_brightness) return;
    hooks_.set_brightness(std::max(3, user_percent * dim_percent / 100), false);
}

void ChipoSettings::UpdateSaver(uint32_t now_ms) {
    if (hooks_.keep_awake && hooks_.keep_awake()) {
        last_activity_ms_ = now_ms;  // something is going on: the idle clock restarts
        if (dimmed_) NotifyActivity();
        return;
    }
    if (open_) return;
    const int target = DimPercent(now_ms - last_activity_ms_);
    if (target < 100) {
        if (!dimmed_) {
            if (hooks_.get_brightness) user_brightness_ = hooks_.get_brightness();
            dimmed_ = true;
        }
        if (target != dim_percent_) {
            dim_percent_ = target;
            ApplyBrightness(user_brightness_, target);
        }
    }
}

void ChipoSettings::Step(uint32_t now_ms) {
    if (!running_) return;
    UpdateBattery(now_ms);
    UpdateSaver(now_ms);
    if (open_ && now_ms - opened_ms_ >= kMenuIdleCloseMs) Close();
    if (restart_armed_until_ms_ != 0 && now_ms >= restart_armed_until_ms_ && open_) {
        restart_armed_until_ms_ = 0;
        lv_label_set_text(restart_label_, "Reiniciar");
    }
    if (indicator_visible_ && battery_.state == BatteryState::Critical) {  // blinks when nearly empty
        lv_obj_set_style_opa(indicator_, ((now_ms / 1000) % 2) ? LV_OPA_40 : LV_OPA_COVER, 0);
    } else if (indicator_) {
        lv_obj_set_style_opa(indicator_, LV_OPA_COVER, 0);
    }
    int dx = 0, dy = 0;
    PixelShift(now_ms - start_ms_, &dx, &dy);
    if (dx != shift_x_ || dy != shift_y_) {
        shift_x_ = dx;
        shift_y_ = dy;
        if (hooks_.shift) hooks_.shift(dx, dy);
    }
}

ChipoSettings::DebugInfo ChipoSettings::Debug() const {
    DebugInfo d{};
    d.open = open_;
    d.volume = volume_;
    d.brightness = brightness_;
    d.dimmed = dimmed_;
    d.dim_percent = dim_percent_;
    d.indicator_visible = indicator_visible_;
    d.battery_state = battery_.state;
    d.shift_x = shift_x_;
    d.shift_y = shift_y_;
    d.restart_armed = restart_armed_until_ms_ != 0;
    return d;
}

void ChipoSettings::TimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoSettings*>(lv_timer_get_user_data(timer));
    if (self) self->Step(self->Now());
}

void ChipoSettings::IndevCallback(lv_event_t* e) {
    auto* self = static_cast<ChipoSettings*>(lv_event_get_user_data(e));
    if (!self) return;
    switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED: self->OnPressed(); break;
    case LV_EVENT_LONG_PRESSED: self->OnLongPress(); break;
    case LV_EVENT_PRESSING: {
        lv_point_t p{0, 0};
        lv_indev_t* in = lv_event_get_indev(e);
        if (in) lv_indev_get_point(in, &p);
        self->OnTouchMove(p.x, p.y);
        break;
    }
    case LV_EVENT_RELEASED: self->OnTouchEnd(); break;
    case LV_EVENT_CLICKED: {
        lv_point_t p{0, 0};
        lv_indev_t* in = lv_event_get_indev(e);
        if (in) lv_indev_get_point(in, &p);
        self->OnTap(p.x, p.y);
        break;
    }
    default: break;
    }
}

void ChipoSettings::ButtonCallback(lv_event_t* e) {
    auto* b = static_cast<Btn*>(lv_event_get_user_data(e));
    if (b && b->self) b->self->Press(b->action);
}
