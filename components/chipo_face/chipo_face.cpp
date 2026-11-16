#include "chipo_face.h"

#include "esp_log.h"

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

ChipoFace::~ChipoFace() {
    Stop();
}

bool ChipoFace::Start(lv_obj_t* parent) {
    if (parent == nullptr) {
        ESP_LOGE(TAG, "Start called with null parent");
        return false;
    }
    if (root_ != nullptr) return true;

    root_ = lv_obj_create(parent);
    if (root_ == nullptr) return false;

    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, kCanvasWidth, kCanvasHeight);
    lv_obj_center(root_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, LV_PART_MAIN);

    left_eye_ = lv_obj_create(root_);
    right_eye_ = lv_obj_create(root_);
    left_pupil_ = lv_obj_create(root_);
    right_pupil_ = lv_obj_create(root_);
    mouth_ = lv_obj_create(root_);

    if (!left_eye_ || !right_eye_ || !left_pupil_ || !right_pupil_ || !mouth_) {
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

    blink_timer_ = lv_timer_create(BlinkTimerCallback, kBlinkPeriodMs, this);
    if (blink_timer_ == nullptr) {
        Stop();
        ESP_LOGE(TAG, "Could not create blink timer");
        return false;
    }

    motion_timer_ = lv_timer_create(MotionTimerCallback, kMotionPeriodMs, this);
    ApplyExpression();
    ESP_LOGI(TAG, "CHIPO Face initialized with smooth luminous eyes");
    return true;
}

void ChipoFace::Stop() {
    if (blink_timer_) {
        lv_timer_delete(blink_timer_);
        blink_timer_ = nullptr;
    }
    if (restore_timer_) {
        lv_timer_delete(restore_timer_);
        restore_timer_ = nullptr;
    }
    if (motion_timer_) {
        lv_timer_delete(motion_timer_);
        motion_timer_ = nullptr;
    }
    if (root_) lv_obj_delete(root_);
    root_ = left_eye_ = right_eye_ = left_pupil_ = right_pupil_ = mouth_ = nullptr;
    blinking_ = false;
}

void ChipoFace::SetState(State state) {
    if (state_ == state) return;
    state_ = state;
    ApplyExpression();
}

void ChipoFace::PositionEyes(lv_coord_t pupil_x, lv_coord_t pupil_y) {
    if (!left_eye_ || !right_eye_ || !left_pupil_ || !right_pupil_) return;

    const lv_coord_t left_x = kCanvasWidth / 2 - kEyeGap / 2 - expression_eye_width_;
    const lv_coord_t right_x = kCanvasWidth / 2 + kEyeGap / 2;
    lv_obj_set_pos(left_eye_, left_x, kEyeY);
    lv_obj_set_pos(right_eye_, right_x, kEyeY);

    const lv_coord_t pupil_w = 22;
    const lv_coord_t pupil_h = 30;
    lv_obj_set_size(left_pupil_, pupil_w, pupil_h);
    lv_obj_set_size(right_pupil_, pupil_w, pupil_h);
    lv_obj_set_pos(left_pupil_, left_x + (expression_eye_width_ - pupil_w) / 2 + pupil_x,
                   kEyeY + (expression_eye_height_ - pupil_h) / 2 + pupil_y);
    lv_obj_set_pos(right_pupil_, right_x + (expression_eye_width_ - pupil_w) / 2 + pupil_x,
                   kEyeY + (expression_eye_height_ - pupil_h) / 2 + pupil_y);
}

void ChipoFace::ApplyExpression() {
    if (!root_ || !left_eye_ || !right_eye_ || !mouth_) return;

    lv_coord_t eye_w = 62;
    lv_coord_t eye_h = 70;
    lv_coord_t pupil_x = 0;
    lv_coord_t pupil_y = 0;
    lv_coord_t mouth_w = 42;
    lv_coord_t mouth_h = 8;
    lv_coord_t mouth_y = kMouthY;
    lv_coord_t mouth_radius = LV_RADIUS_CIRCLE;

    switch (state_) {
    case State::Idle:
        break;
    case State::Listening:
        eye_w = 66; eye_h = 78;
        mouth_w = 13; mouth_h = 13; mouth_y -= 3;
        break;
    case State::Thinking:
        pupil_x = 8; pupil_y = -9;
        mouth_w = 28; mouth_h = 7;
        break;
    case State::Speaking:
        mouth_w = 34; mouth_h = 28; mouth_y -= 9;
        break;
    case State::Connecting:
        eye_w = 58; eye_h = 64;
        mouth_w = 24; mouth_h = 7;
        break;
    case State::Error:
        eye_w = 62; eye_h = 48;
        mouth_w = 36; mouth_h = 7; mouth_y += 5;
        pupil_y = 4;
        break;
    }

    expression_eye_width_ = eye_w;
    expression_eye_height_ = eye_h;
    if (!blinking_) {
        lv_obj_set_size(left_eye_, eye_w, eye_h);
        lv_obj_set_size(right_eye_, eye_w, eye_h);
        lv_obj_set_style_radius(left_eye_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_radius(right_eye_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        PositionEyes(pupil_x, pupil_y);
    }

    lv_obj_set_size(mouth_, mouth_w, mouth_h);
    lv_obj_set_style_radius(mouth_, mouth_radius, LV_PART_MAIN);
    lv_obj_set_pos(mouth_, (kCanvasWidth - mouth_w) / 2, mouth_y);
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
    lv_timer_delete(timer);
}

void ChipoFace::MotionTimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoFace*>(lv_timer_get_user_data(timer));
    if (!self || self->blinking_) return;
    if (self->state_ == State::Thinking) {
        self->PositionEyes(8, -9);
    } else if (self->state_ == State::Listening) {
        self->PositionEyes(0, 2);
    } else {
        self->PositionEyes(0, 0);
    }
}

void ChipoFace::Blink() {
    if (!left_eye_ || !right_eye_ || blinking_) return;
    blinking_ = true;
    lv_obj_set_height(left_eye_, 4);
    lv_obj_set_height(right_eye_, 4);
    lv_obj_set_height(left_pupil_, 1);
    lv_obj_set_height(right_pupil_, 1);
    restore_timer_ = lv_timer_create(RestoreEyesTimerCallback, kBlinkClosedMs, this);
    if (!restore_timer_) {
        blinking_ = false;
        ApplyExpression();
    } else {
        lv_timer_set_repeat_count(restore_timer_, 1);
    }
}
