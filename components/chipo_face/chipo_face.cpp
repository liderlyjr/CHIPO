#include "chipo_face.h"

#include "esp_log.h"

namespace {
constexpr char TAG[] = "ChipoFace";
constexpr lv_coord_t kCanvasWidth = 240;
constexpr lv_coord_t kEyeWidth = 54;
constexpr lv_coord_t kEyeGap = 28;
constexpr lv_coord_t kEyeY = 54;
constexpr lv_coord_t kMouthY = 142;
constexpr uint32_t kBlinkPeriodMs = 4300;
constexpr uint32_t kBlinkClosedMs = 105;

void StyleFeature(lv_obj_t* object, lv_color_t color, lv_coord_t radius) {
    lv_obj_set_style_bg_color(object, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(object, radius, LV_PART_MAIN);
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
    lv_obj_set_size(root_, kCanvasWidth, 210);
    lv_obj_center(root_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, LV_PART_MAIN);

    const lv_color_t cream = lv_color_hex(0xF4E9D8);
    left_eye_ = lv_obj_create(root_);
    right_eye_ = lv_obj_create(root_);
    mouth_ = lv_obj_create(root_);

    if (!left_eye_ || !right_eye_ || !mouth_) {
        Stop();
        ESP_LOGE(TAG, "Could not create face objects");
        return false;
    }

    StyleFeature(left_eye_, cream, 18);
    StyleFeature(right_eye_, cream, 18);
    StyleFeature(mouth_, cream, 8);

    lv_obj_set_size(left_eye_, kEyeWidth, expression_eye_height_);
    lv_obj_set_size(right_eye_, kEyeWidth, expression_eye_height_);
    lv_obj_set_pos(left_eye_, kCanvasWidth / 2 - kEyeGap / 2 - kEyeWidth, kEyeY);
    lv_obj_set_pos(right_eye_, kCanvasWidth / 2 + kEyeGap / 2, kEyeY);
    lv_obj_set_size(mouth_, 34, 6);
    lv_obj_set_pos(mouth_, (kCanvasWidth - 34) / 2, kMouthY);

    blink_timer_ = lv_timer_create(BlinkTimerCallback, kBlinkPeriodMs, this);
    if (blink_timer_ == nullptr) {
        Stop();
        ESP_LOGE(TAG, "Could not create blink timer");
        return false;
    }

    ApplyExpression();
    ESP_LOGI(TAG, "CHIPO Face initialized");
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
    if (root_) lv_obj_delete(root_);
    root_ = left_eye_ = right_eye_ = mouth_ = nullptr;
    blinking_ = false;
}

void ChipoFace::SetState(State state) {
    if (state_ == state) return;
    state_ = state;
    ApplyExpression();
}

void ChipoFace::ApplyExpression() {
    if (!root_ || !left_eye_ || !right_eye_ || !mouth_) return;

    lv_coord_t eye_h = 28;
    lv_coord_t mouth_w = 34;
    lv_coord_t mouth_h = 6;
    lv_coord_t mouth_r = 6;
    lv_coord_t mouth_y = kMouthY;
    lv_color_t eye_color = lv_color_hex(0xF4E9D8);

    switch (state_) {
    case State::Idle:
        mouth_w = 34; mouth_h = 6; break;
    case State::Listening:
        eye_h = 34; mouth_w = 18; mouth_h = 18; mouth_r = 9; mouth_y -= 5; break;
    case State::Thinking:
        eye_h = 20; mouth_w = 28; mouth_h = 5; break;
    case State::Speaking:
        mouth_w = 30; mouth_h = 20; mouth_r = 10; mouth_y -= 5; break;
    case State::Connecting:
        eye_h = 18; mouth_w = 24; mouth_h = 5; break;
    case State::Error:
        eye_h = 12; mouth_w = 42; mouth_h = 6;
        eye_color = lv_color_hex(0xFF746C); break;
    }

    expression_eye_height_ = eye_h;
    if (!blinking_) {
        lv_obj_set_height(left_eye_, eye_h);
        lv_obj_set_height(right_eye_, eye_h);
    }
    lv_obj_set_style_bg_color(left_eye_, eye_color, LV_PART_MAIN);
    lv_obj_set_style_bg_color(right_eye_, eye_color, LV_PART_MAIN);
    lv_obj_set_size(mouth_, mouth_w, mouth_h);
    lv_obj_set_style_radius(mouth_, mouth_r, LV_PART_MAIN);
    lv_obj_set_pos(mouth_, (kCanvasWidth - mouth_w) / 2, mouth_y);
}

void ChipoFace::BlinkTimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoFace*>(lv_timer_get_user_data(timer));
    if (self) self->Blink();
}

void ChipoFace::RestoreEyesTimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoFace*>(lv_timer_get_user_data(timer));
    if (!self) return;
    if (self->left_eye_) lv_obj_set_height(self->left_eye_, self->expression_eye_height_);
    if (self->right_eye_) lv_obj_set_height(self->right_eye_, self->expression_eye_height_);
    self->blinking_ = false;
    self->restore_timer_ = nullptr;
    lv_timer_delete(timer);
}

void ChipoFace::Blink() {
    if (!left_eye_ || !right_eye_ || blinking_) return;
    blinking_ = true;
    lv_obj_set_height(left_eye_, 2);
    lv_obj_set_height(right_eye_, 2);
    restore_timer_ = lv_timer_create(RestoreEyesTimerCallback, kBlinkClosedMs, this);
    if (!restore_timer_) {
        lv_obj_set_height(left_eye_, expression_eye_height_);
        lv_obj_set_height(right_eye_, expression_eye_height_);
        blinking_ = false;
    } else {
        lv_timer_set_repeat_count(restore_timer_, 1);
    }
}
