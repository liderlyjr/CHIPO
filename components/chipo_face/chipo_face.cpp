#include "chipo_face.h"

#include "esp_log.h"
#include <cstdlib>
#include <cstdio>

namespace {
constexpr char TAG[] = "ChipoFace";
constexpr lv_coord_t kCanvasWidth = 300;
constexpr lv_coord_t kCanvasHeight = 220;
constexpr lv_coord_t kEyeY = 54;
constexpr lv_coord_t kEyeGap = 34;
constexpr lv_coord_t kMouthY = 158;
constexpr uint32_t kBlinkPeriodMs = 4200;
constexpr uint32_t kBlinkClosedMs = 115;
constexpr uint32_t kMotionPeriodMs = 1800;
constexpr uint32_t kFallCooldownMs = 2800;
constexpr uint32_t kIdlePlayDelayMs = 14000;
constexpr uint32_t kPixelStepMs = 40;
constexpr int kFallTiltThresholdMg = 720;
constexpr uint32_t kGlowBlue = 0x75DFFF;
constexpr uint32_t kEyeCore = 0xDDF8FF;
constexpr uint32_t kPupil = 0x06121B;

void StyleGlow(lv_obj_t* object, lv_color_t color, lv_coord_t radius) {
    lv_obj_remove_style_all(object);
    lv_obj_set_style_bg_color(object, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(object, radius, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(object, lv_color_hex(kGlowBlue), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(object, 20, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(object, LV_OPA_60, LV_PART_MAIN);
}
void StylePupil(lv_obj_t* object) {
    lv_obj_remove_style_all(object);
    lv_obj_set_style_bg_color(object, lv_color_hex(kPupil), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(object, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}
}

ChipoFace::~ChipoFace() { Stop(); }

bool ChipoFace::Start(lv_obj_t* parent) {
    if (!parent) {
        ESP_LOGE(TAG, "Start called with null parent");
        return false;
    }
    if (root_) return true;

    root_ = lv_obj_create(parent);
    if (!root_) return false;
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, kCanvasWidth, kCanvasHeight);
    lv_obj_center(root_);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_x(root_, kCanvasWidth / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(root_, kCanvasHeight / 2, LV_PART_MAIN);

    left_eye_ = lv_obj_create(root_);
    right_eye_ = lv_obj_create(root_);
    left_pupil_ = lv_obj_create(root_);
    right_pupil_ = lv_obj_create(root_);
    mouth_ = lv_obj_create(root_);
    battery_label_ = lv_label_create(root_);
    timer_label_ = lv_label_create(root_);\n    weather_label_ = lv_label_create(root_);
    if (!left_eye_ || !right_eye_ || !left_pupil_ || !right_pupil_ || !mouth_ || !battery_label_ || !timer_label_ || !weather_label_) {
        Stop();
        ESP_LOGE(TAG, "Could not create face objects");
        return false;
    }

    StyleGlow(left_eye_, lv_color_hex(kEyeCore), LV_RADIUS_CIRCLE);
    StyleGlow(right_eye_, lv_color_hex(kEyeCore), LV_RADIUS_CIRCLE);
    StylePupil(left_pupil_);
    StylePupil(right_pupil_);
    StyleGlow(mouth_, lv_color_hex(kGlowBlue), LV_RADIUS_CIRCLE);

    expression_eye_width_ = 62;
    expression_eye_height_ = 70;
    lv_obj_set_size(left_eye_, expression_eye_width_, expression_eye_height_);
    lv_obj_set_size(right_eye_, expression_eye_width_, expression_eye_height_);
    lv_obj_set_size(left_pupil_, 22, 30);
    lv_obj_set_size(right_pupil_, 22, 30);
    PositionEyes(0, 0);
    lv_obj_set_size(mouth_, 42, 8);
    lv_obj_set_pos(mouth_, (kCanvasWidth - 42) / 2, kMouthY);

    lv_label_set_text(battery_label_, "BAT --%");
    lv_obj_set_width(battery_label_, 76);
    lv_obj_set_style_text_color(battery_label_, lv_color_hex(kGlowBlue), LV_PART_MAIN);
    lv_obj_set_style_text_opa(battery_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_align(battery_label_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_pos(battery_label_, 216, 8);

    lv_label_set_text(weather_label_, "SBD WEATHER --");
    lv_obj_set_width(weather_label_, 202);
    lv_obj_set_style_text_color(weather_label_, lv_color_hex(kGlowBlue), LV_PART_MAIN);
    lv_obj_set_style_text_opa(weather_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_align(weather_label_, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_pos(weather_label_, 8, 8);

    lv_label_set_text(timer_label_, "");
    lv_obj_set_width(timer_label_, 150);
    lv_obj_set_style_text_color(timer_label_, lv_color_hex(kGlowBlue), LV_PART_MAIN);
    lv_obj_set_style_text_opa(timer_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_align(timer_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(timer_label_, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_add_flag(timer_label_, LV_OBJ_FLAG_HIDDEN);

    // Touch is deliberately attached only to facial features, not the full screen,
    // so XiaoZhi's normal menus and dialogs remain usable outside CHIPO's face.
    auto add_touch_events = [this](lv_obj_t* feature) {
        lv_obj_add_flag(feature, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(feature, TouchEventCallback, LV_EVENT_PRESSED, this);
        lv_obj_add_event_cb(feature, TouchEventCallback, LV_EVENT_PRESSING, this);
        lv_obj_add_event_cb(feature, TouchEventCallback, LV_EVENT_LONG_PRESSED, this);
        lv_obj_add_event_cb(feature, TouchEventCallback, LV_EVENT_RELEASED, this);
        lv_obj_add_event_cb(feature, TouchEventCallback, LV_EVENT_CLICKED, this);
    };
    add_touch_events(left_eye_);
    add_touch_events(right_eye_);
    add_touch_events(left_pupil_);
    add_touch_events(right_pupil_);
    add_touch_events(mouth_);

    blink_timer_ = lv_timer_create(BlinkTimerCallback, kBlinkPeriodMs, this);
    if (!blink_timer_) {
        Stop();
        ESP_LOGE(TAG, "Could not create blink timer");
        return false;
    }
    motion_timer_ = lv_timer_create(MotionTimerCallback, kMotionPeriodMs, this);
    last_activity_tick_ = lv_tick_get();
    idle_timer_ = lv_timer_create(IdleTimerCallback, 1000, this);
    ApplyExpression();
    ESP_LOGI(TAG, "CHIPO Face initialized with touch interactions");
    return true;
}

void ChipoFace::Stop() {
    if (blink_timer_) { lv_timer_delete(blink_timer_); blink_timer_ = nullptr; }
    if (restore_timer_) { lv_timer_delete(restore_timer_); restore_timer_ = nullptr; }
    if (motion_timer_) { lv_timer_delete(motion_timer_); motion_timer_ = nullptr; }
    if (interaction_timer_) { lv_timer_delete(interaction_timer_); interaction_timer_ = nullptr; }
    if (fall_timer_) { lv_timer_delete(fall_timer_); fall_timer_ = nullptr; }
    if (battery_timer_) { lv_timer_delete(battery_timer_); battery_timer_ = nullptr; }
    if (idle_timer_) { lv_timer_delete(idle_timer_); idle_timer_ = nullptr; }
    if (pixel_timer_) { lv_timer_delete(pixel_timer_); pixel_timer_ = nullptr; }
    if (idle_pixel_) { lv_obj_delete(idle_pixel_); idle_pixel_ = nullptr; }
    lv_anim_delete(this, nullptr);
    if (root_) lv_obj_delete(root_);
    root_ = left_eye_ = right_eye_ = left_pupil_ = right_pupil_ = mouth_ = battery_label_ = timer_label_ = weather_label_ = nullptr;
    blinking_ = false;
}

void ChipoFace::SetState(State state) {
    if (state_ == state) return;
    last_activity_tick_ = lv_tick_get();
    StopIdlePlay();
    lv_anim_delete(this, GazeAnimationCallback);
    gaze_offset_x_ = 0;
    state_ = state;
    ApplyExpression();
}

void ChipoFace::StartIdlePlay() {
    if (!root_ || idle_pixel_ || state_ != State::Idle ||
        interaction_ != Interaction::None || falling_) return;

    interaction_ = Interaction::Bored;
    ApplyExpression();

    idle_pixel_ = lv_obj_create(root_);
    if (!idle_pixel_) {
        interaction_ = Interaction::None;
        ApplyExpression();
        return;
    }
    StyleGlow(idle_pixel_, lv_color_hex(kGlowBlue), 1);
    lv_obj_set_size(idle_pixel_, 5, 5);
    pixel_x_ = 12;
    pixel_y_ = 18;
    pixel_vx_ = 3;
    pixel_vy_ = 2;
    lv_obj_set_pos(idle_pixel_, pixel_x_, pixel_y_);
    pixel_timer_ = lv_timer_create(PixelTimerCallback, kPixelStepMs, this);
    if (!pixel_timer_) StopIdlePlay();
    ESP_LOGI(TAG, "CHIPO is bored: starting its little pixel game");
}

void ChipoFace::StopIdlePlay() {
    if (pixel_timer_) {
        lv_timer_delete(pixel_timer_);
        pixel_timer_ = nullptr;
    }
    if (idle_pixel_) {
        lv_obj_delete(idle_pixel_);
        idle_pixel_ = nullptr;
    }
    if (interaction_ == Interaction::Bored) {
        interaction_ = Interaction::None;
        gaze_offset_x_ = 0;
        if (root_ && !blinking_) ApplyExpression();
    }
}

void ChipoFace::IdleTimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoFace*>(lv_timer_get_user_data(timer));
    if (!self || self->idle_pixel_ || self->state_ != State::Idle ||
        self->interaction_ != Interaction::None || self->falling_) return;
    if (lv_tick_get() - self->last_activity_tick_ >= kIdlePlayDelayMs) {
        self->StartIdlePlay();
    }
}

void ChipoFace::PixelTimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoFace*>(lv_timer_get_user_data(timer));
    if (!self || !self->root_ || !self->idle_pixel_) return;

    int next_x = self->pixel_x_ + self->pixel_vx_;
    int next_y = self->pixel_y_ + self->pixel_vy_;
    if (next_x < 3 || next_x > kCanvasWidth - 8) {
        self->pixel_vx_ = -self->pixel_vx_;
        next_x = self->pixel_x_ + self->pixel_vx_;
    }
    if (next_y < 3 || next_y > kCanvasHeight - 8) {
        self->pixel_vy_ = -self->pixel_vy_;
        next_y = self->pixel_y_ + self->pixel_vy_;
    }
    self->pixel_x_ = static_cast<lv_coord_t>(next_x);
    self->pixel_y_ = static_cast<lv_coord_t>(next_y);
    lv_obj_set_pos(self->idle_pixel_, self->pixel_x_, self->pixel_y_);

    // CHIPO watches its tiny toy as it bounces around the display.
    if (!self->blinking_) {
        const int gaze_x = (self->pixel_x_ - kCanvasWidth / 2) / 10;
        const int gaze_y = (self->pixel_y_ - kCanvasHeight / 2) / 12;
        self->PositionEyes(static_cast<lv_coord_t>(gaze_x < -8 ? -8 : (gaze_x > 8 ? 8 : gaze_x)),
                           static_cast<lv_coord_t>(gaze_y < -7 ? -7 : (gaze_y > 7 ? 7 : gaze_y)));
    }
}

void ChipoFace::SetBatteryStatus(int level, bool charging, bool discharging) {
    if (!battery_label_) return;
    if (level < 0) level = 0;
    if (level > 100) level = 100;

    char text[20];
    if (charging) {
        std::snprintf(text, sizeof(text), "CHG %d%%", level);
    } else if (discharging) {
        std::snprintf(text, sizeof(text), "BAT %d%%", level);
    } else {
        std::snprintf(text, sizeof(text), "%d%%", level);
    }
    lv_label_set_text(battery_label_, text);

    const bool low_battery = level <= 15 && !charging;
    lv_obj_set_style_text_color(battery_label_,
        lv_color_hex(low_battery ? 0xFF7185 : kGlowBlue), LV_PART_MAIN);

    if (charging && !battery_timer_) {
        battery_pulse_visible_ = true;
        lv_obj_set_style_text_opa(battery_label_, LV_OPA_COVER, LV_PART_MAIN);
        battery_timer_ = lv_timer_create(BatteryPulseTimerCallback, 550, this);
    } else if (!charging && battery_timer_) {
        lv_timer_delete(battery_timer_);
        battery_timer_ = nullptr;
        battery_pulse_visible_ = true;
        lv_obj_set_style_text_opa(battery_label_, LV_OPA_COVER, LV_PART_MAIN);
    }
}

void ChipoFace::SetTimerStatus(const char* text) {
    if (!timer_label_) return;
    if (text == nullptr || text[0] == '\0') {
        lv_label_set_text(timer_label_, "");
        lv_obj_add_flag(timer_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(timer_label_, text);
    lv_obj_remove_flag(timer_label_, LV_OBJ_FLAG_HIDDEN);
}

void ChipoFace::SetWeatherStatus(const char* text) {
    if (!weather_label_) return;
    lv_label_set_text(weather_label_, (text == nullptr || text[0] == '\\0') ? "SBD WEATHER --" : text);
}

void ChipoFace::PositionEyes(lv_coord_t pupil_x, lv_coord_t pupil_y) {
    if (!left_eye_ || !right_eye_ || !left_pupil_ || !right_pupil_) return;
    const lv_coord_t left_x = kCanvasWidth / 2 - kEyeGap / 2 - expression_eye_width_;
    const lv_coord_t right_x = kCanvasWidth / 2 + kEyeGap / 2;
    lv_obj_set_pos(left_eye_, left_x, kEyeY);
    lv_obj_set_pos(right_eye_, right_x, kEyeY);
    constexpr lv_coord_t pupil_w = 22;
    constexpr lv_coord_t pupil_h = 30;
    lv_obj_set_size(left_pupil_, pupil_w, pupil_h);
    lv_obj_set_size(right_pupil_, pupil_w, pupil_h);
    lv_obj_set_pos(left_pupil_, left_x + (expression_eye_width_ - pupil_w) / 2 + pupil_x,
                   kEyeY + (expression_eye_height_ - pupil_h) / 2 + pupil_y);
    lv_obj_set_pos(right_pupil_, right_x + (expression_eye_width_ - pupil_w) / 2 + pupil_x,
                   kEyeY + (expression_eye_height_ - pupil_h) / 2 + pupil_y);
}

void ChipoFace::ApplyExpression() {
    if (!root_ || !left_eye_ || !right_eye_ || !mouth_) return;
    lv_coord_t eye_w = 62, eye_h = 70, pupil_x = touch_pupil_x_, pupil_y = touch_pupil_y_;
    lv_coord_t mouth_w = 42, mouth_h = 8, mouth_y = kMouthY;

    if (interaction_ == Interaction::Dizzy) {
        eye_w = 66; eye_h = 66; mouth_w = 38; mouth_h = 20; mouth_y -= 5;
    } else if (interaction_ == Interaction::Annoyed) {
        eye_w = 62; eye_h = 48; pupil_y = 10; mouth_w = 36; mouth_h = 6; mouth_y += 8;
    } else if (interaction_ == Interaction::Curious) {
        eye_w = 70; eye_h = 78; mouth_w = 13; mouth_h = 13; mouth_y -= 4;
    } else if (interaction_ == Interaction::Bored) {
        eye_w = 62; eye_h = 48; pupil_y = 8; mouth_w = 32; mouth_h = 5; mouth_y += 9;
    } else if (interaction_ == Interaction::Tilted) {
        pupil_x = touch_pupil_x_; pupil_y = touch_pupil_y_;
    } else {
        switch (state_) {
        case State::Idle: break;
        case State::Listening: eye_w = 66; eye_h = 78; mouth_w = 13; mouth_h = 13; mouth_y -= 3; break;
        case State::Thinking: pupil_x = 8; pupil_y = -9; mouth_w = 28; mouth_h = 7; break;
        case State::Speaking: mouth_w = 34; mouth_h = 28; mouth_y -= 9; break;
        case State::Connecting: eye_w = 58; eye_h = 64; mouth_w = 24; mouth_h = 7; break;
        case State::Error: eye_w = 62; eye_h = 48; mouth_w = 36; mouth_h = 7; mouth_y += 5; pupil_y = 4; break;
        }
    }

    expression_eye_width_ = eye_w;
    expression_eye_height_ = eye_h;
    if (!blinking_) {
        lv_obj_set_size(left_eye_, eye_w, eye_h);
        lv_obj_set_size(right_eye_, eye_w, eye_h);
        PositionEyes(pupil_x, pupil_y);
    }
    lv_obj_set_size(mouth_, mouth_w, mouth_h);
    lv_obj_set_pos(mouth_, (kCanvasWidth - mouth_w) / 2, mouth_y);
}

void ChipoFace::SetInteraction(Interaction interaction, uint32_t duration_ms) {
    last_activity_tick_ = lv_tick_get();
    StopIdlePlay();
    lv_anim_delete(this, GazeAnimationCallback);
    gaze_offset_x_ = 0;
    interaction_ = interaction;
    ApplyExpression();
    if (interaction_timer_) {
        lv_timer_delete(interaction_timer_);
        interaction_timer_ = nullptr;
    }
    if (duration_ms > 0) {
        interaction_timer_ = lv_timer_create(InteractionTimerCallback, duration_ms, this);
        if (interaction_timer_) lv_timer_set_repeat_count(interaction_timer_, 1);
    }
}
void ChipoFace::ClearInteraction() {
    interaction_timer_ = nullptr;
    interaction_ = Interaction::None;
    last_activity_tick_ = lv_tick_get();
    ApplyExpression();
}
void ChipoFace::InteractionTimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoFace*>(lv_timer_get_user_data(timer));
    if (self) self->ClearInteraction();
}

void ChipoFace::TouchEventCallback(lv_event_t* event) {
    auto* self = static_cast<ChipoFace*>(lv_event_get_user_data(event));
    if (self) self->HandleTouch(event);
}
void ChipoFace::HandleTouch(lv_event_t* event) {
    const auto code = lv_event_get_code(event);
    const uint32_t now = lv_tick_get();
    if (code == LV_EVENT_PRESSED) {
        last_activity_tick_ = now;
        StopIdlePlay();
        long_press_active_ = false;
        return;
    }
    if (code == LV_EVENT_PRESSING) {
        lv_indev_t* indev = lv_event_get_indev(event);
        if (indev) {
            lv_point_t point;
            lv_area_t area;
            lv_indev_get_point(indev, &point);
            lv_obj_get_coords(root_, &area);
            const int local_x = point.x - area.x1;
            const int local_y = point.y - area.y1;
            touch_pupil_x_ = local_x < kCanvasWidth / 2 ? -7 : 7;
            touch_pupil_y_ = local_y < kCanvasHeight / 2 ? -5 : 5;
            ApplyExpression();
        }
        return;
    }
    if (code == LV_EVENT_LONG_PRESSED) {
        long_press_active_ = true;
        SetInteraction(Interaction::Annoyed, 1500);
        return;
    }
    if (code == LV_EVENT_RELEASED) {
        if (long_press_active_) {
            long_press_active_ = false;
            SetInteraction(Interaction::Curious, 900);
        }
        return;
    }
    if (code == LV_EVENT_CLICKED) {
        if (now - last_tap_tick_ < 900) ++tap_count_; else tap_count_ = 1;
        last_tap_tick_ = now;
        if (tap_count_ >= 4) {
            tap_count_ = 0;
            SetInteraction(Interaction::Annoyed, 2200);
        } else {
            SetInteraction(Interaction::Curious, 1000);
        }
        Blink();
    }
}

void ChipoFace::StartFall(int direction) {
    if (!root_ || falling_) return;
    falling_ = true;
    fall_direction_ = direction < 0 ? -1 : 1;
    SetInteraction(Interaction::Dizzy, 0);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, this);
    lv_anim_set_values(&animation, 0, 1000);
    lv_anim_set_duration(&animation, 620);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in);
    lv_anim_set_exec_cb(&animation, FallAnimationCallback);
    lv_anim_set_completed_cb(&animation, FallAnimationReadyCallback);
    lv_anim_start(&animation);
}

void ChipoFace::FallAnimationCallback(void* var, int32_t value) {
    auto* self = static_cast<ChipoFace*>(var);
    if (!self || !self->root_) return;
    const int32_t eased = value;
    const int32_t rotation = self->fall_direction_ * 780 * eased / 1000;
    const int32_t shift_x = self->fall_direction_ * 34 * eased / 1000;
    const int32_t shift_y = 48 * eased / 1000;
    lv_obj_set_style_transform_rotation(self->root_, rotation, LV_PART_MAIN);
    lv_obj_set_style_translate_x(self->root_, shift_x, LV_PART_MAIN);
    lv_obj_set_style_translate_y(self->root_, shift_y, LV_PART_MAIN);
}

void ChipoFace::FallAnimationReadyCallback(lv_anim_t* animation) {
    auto* self = static_cast<ChipoFace*>(animation->var);
    if (!self || !self->root_) return;
    self->fall_timer_ = lv_timer_create(RecoverFallTimerCallback, 850, self);
    if (self->fall_timer_) lv_timer_set_repeat_count(self->fall_timer_, 1);
    else self->StartRecovery();
}

void ChipoFace::RecoverFallTimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoFace*>(lv_timer_get_user_data(timer));
    if (self) {
        self->fall_timer_ = nullptr;
        self->StartRecovery();
    }
}

void ChipoFace::StartRecovery() {
    if (!root_) return;
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, this);
    lv_anim_set_values(&animation, 1000, 0);
    lv_anim_set_duration(&animation, 720);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, RecoveryAnimationCallback);
    lv_anim_set_completed_cb(&animation, RecoveryAnimationReadyCallback);
    lv_anim_start(&animation);
}

void ChipoFace::RecoveryAnimationCallback(void* var, int32_t value) {
    auto* self = static_cast<ChipoFace*>(var);
    if (!self || !self->root_) return;
    const int32_t remaining = value;
    lv_obj_set_style_transform_rotation(self->root_, self->fall_direction_ * 780 * remaining / 1000, LV_PART_MAIN);
    lv_obj_set_style_translate_x(self->root_, self->fall_direction_ * 34 * remaining / 1000, LV_PART_MAIN);
    lv_obj_set_style_translate_y(self->root_, 48 * remaining / 1000, LV_PART_MAIN);
}

void ChipoFace::RecoveryAnimationReadyCallback(lv_anim_t* animation) {
    auto* self = static_cast<ChipoFace*>(animation->var);
    if (!self || !self->root_) return;
    lv_obj_set_style_transform_rotation(self->root_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_x(self->root_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(self->root_, 0, LV_PART_MAIN);
    self->falling_ = false;
    self->interaction_ = Interaction::Annoyed;
    self->SetInteraction(Interaction::Annoyed, 1700);
}

void ChipoFace::OnMotion(int x_mg, int y_mg, int z_mg) {
    if (!root_) return;
    const int dx = has_previous_motion_ ? std::abs(x_mg - previous_ax_) : 0;
    const int dy = has_previous_motion_ ? std::abs(y_mg - previous_ay_) : 0;
    const int dz = has_previous_motion_ ? std::abs(z_mg - previous_az_) : 0;
    previous_ax_ = x_mg; previous_ay_ = y_mg; previous_az_ = z_mg;
    const bool first = !has_previous_motion_;
    has_previous_motion_ = true;
    if (first) return;

    const uint32_t now = lv_tick_get();
    if (dx + dy + dz > 220) {
        last_activity_tick_ = now;
        StopIdlePlay();
    }
    // Detect a shake before evaluating static tilt so a vigorous shake is not
    // mistaken for CHIPO tipping over.
    const int shake_score = dx + dy + dz;
    if (shake_score > 2200 && now - last_shake_tick_ > 1800) {
        last_shake_tick_ = now;
        SetInteraction(Interaction::Dizzy, 2600);
        Blink();
        return;
    }

    const int tilt_x = std::abs(x_mg);
    const int tilt_y = std::abs(y_mg);
    if (!falling_ && (tilt_x > kFallTiltThresholdMg || tilt_y > kFallTiltThresholdMg) &&
        now - last_fall_tick_ > kFallCooldownMs) {
        last_fall_tick_ = now;
        StartFall(tilt_x >= tilt_y ? (x_mg < 0 ? -1 : 1) : (y_mg < 0 ? -1 : 1));
        return;
    }
    if (falling_) return;

    // Gravity vector gives a simple tilt estimate. Clamp eye travel so pupils
    // remain inside their luminous eye shapes.
    const int px = (x_mg > 250 ? 8 : (x_mg < -250 ? -8 : 0));
    const int py = (y_mg > 250 ? 7 : (y_mg < -250 ? -7 : 0));
    const bool pupil_position_changed = px != touch_pupil_x_ || py != touch_pupil_y_;
    touch_pupil_x_ = px;
    touch_pupil_y_ = py;
    if (interaction_ == Interaction::None || interaction_ == Interaction::Tilted) {
        const Interaction next_interaction = (px || py) ? Interaction::Tilted : Interaction::None;
        if (pupil_position_changed || next_interaction != interaction_) {
            if (px || py) {
                lv_anim_delete(this, GazeAnimationCallback);
                gaze_offset_x_ = 0;
            }
            interaction_ = next_interaction;
            ApplyExpression();
        }
    }
}

void ChipoFace::BlinkTimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoFace*>(lv_timer_get_user_data(timer));
    if (self) self->Blink();
}
void ChipoFace::RestoreEyesTimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoFace*>(lv_timer_get_user_data(timer));
    if (!self) return;
    self->blinking_ = false;
    self->restore_timer_ = nullptr;
    self->ApplyExpression();
}
void ChipoFace::GazeAnimationCallback(void* var, int32_t value) {
    auto* self = static_cast<ChipoFace*>(var);
    if (!self || !self->root_ || self->blinking_) return;
    self->gaze_offset_x_ = static_cast<lv_coord_t>(value);
    self->PositionEyes(self->gaze_offset_x_, 0);
}

void ChipoFace::MotionTimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoFace*>(lv_timer_get_user_data(timer));
    if (!self || self->blinking_ || self->interaction_ != Interaction::None ||
        self->state_ != State::Idle || self->touch_pupil_x_ != 0 || self->touch_pupil_y_ != 0) return;

    lv_anim_delete(self, GazeAnimationCallback);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, self);
    lv_anim_set_values(&animation, self->gaze_offset_x_, self->gaze_direction_ * 8);
    lv_anim_set_duration(&animation, 520);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&animation, GazeAnimationCallback);
    lv_anim_start(&animation);
    self->gaze_direction_ *= -1;
}

void ChipoFace::BatteryPulseTimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoFace*>(lv_timer_get_user_data(timer));
    if (!self || !self->battery_label_) return;
    self->battery_pulse_visible_ = !self->battery_pulse_visible_;
    lv_obj_set_style_text_opa(self->battery_label_,
        self->battery_pulse_visible_ ? LV_OPA_COVER : LV_OPA_50, LV_PART_MAIN);
}
void ChipoFace::Blink() {
    if (!left_eye_ || !right_eye_ || blinking_) return;
    lv_anim_delete(this, GazeAnimationCallback);
    gaze_offset_x_ = 0;
    blinking_ = true;
    const lv_coord_t blink_y = kEyeY + (expression_eye_height_ - 4) / 2;
    lv_obj_set_y(left_eye_, blink_y);
    lv_obj_set_y(right_eye_, blink_y);
    lv_obj_set_height(left_eye_, 4);
    lv_obj_set_height(right_eye_, 4);
    lv_obj_set_height(left_pupil_, 1);
    lv_obj_set_height(right_pupil_, 1);
    lv_obj_set_y(left_pupil_, blink_y + 1);
    lv_obj_set_y(right_pupil_, blink_y + 1);
    restore_timer_ = lv_timer_create(RestoreEyesTimerCallback, kBlinkClosedMs, this);
    if (!restore_timer_) { blinking_ = false; ApplyExpression(); }
    else lv_timer_set_repeat_count(restore_timer_, 1);
}
