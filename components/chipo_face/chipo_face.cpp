#include "chipo_face.h"

#include <initializer_list>

#include "esp_log.h"

namespace {
constexpr char TAG[] = "ChipoFace";
constexpr int32_t kCanvasWidth = 300;
constexpr int32_t kCanvasHeight = 220;
constexpr int32_t kEyeY = 54;
constexpr int32_t kEyeGap = 34;
constexpr int32_t kMouthY = 158;
constexpr uint32_t kBlinkMinMs = 2800;       // blinks are irregular: min + random(0..jitter)
constexpr uint32_t kBlinkJitterMs = 3400;
constexpr uint32_t kBlinkClosedMs = 115;
constexpr int32_t kBlinkClosedHeight = 4;
constexpr uint32_t kMotionPeriodMs = 140;    // mouth animation / gaze wander tick
constexpr uint32_t kLookChangeTicks = 18;    // new gaze target roughly every 2.5 s
constexpr int32_t kPupilW = 22;
constexpr int32_t kPupilH = 30;
// Mouth heights while speaking (centre stays fixed so it opens and closes in place).
constexpr int32_t kSpeakMouth[] = {28, 12, 22, 8, 26, 14, 20, 10};
constexpr uint32_t kSpeakMouthCount = sizeof(kSpeakMouth) / sizeof(kSpeakMouth[0]);
constexpr int32_t kSpeakMouthWidth = 34;
constexpr int32_t kSpeakMouthCenterY = kMouthY - 9 + 14;

constexpr uint32_t kGlowBlue = 0x75DFFF;
constexpr uint32_t kEyeCore = 0xDDF8FF;
constexpr uint32_t kPupil = 0x06121B;

void StyleGlow(lv_obj_t* object, lv_color_t color, int32_t radius) {
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

// The face overlays XiaoZhi's own UI: it must never swallow touches or scroll events.
void MakeInert(lv_obj_t* object) {
    lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
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
    MakeInert(root_);
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
    for (lv_obj_t* part : {left_eye_, right_eye_, left_pupil_, right_pupil_, mouth_}) {
        MakeInert(part);
    }

    expression_eye_width_ = 62;
    expression_eye_height_ = 70;
    lv_obj_set_size(left_eye_, expression_eye_width_, expression_eye_height_);
    lv_obj_set_size(right_eye_, expression_eye_width_, expression_eye_height_);

    blink_timer_ = lv_timer_create(BlinkTimerCallback, kBlinkMinMs, this);
    if (blink_timer_ == nullptr) {
        Stop();
        ESP_LOGE(TAG, "Could not create blink timer");
        return false;
    }

    motion_timer_ = lv_timer_create(MotionTimerCallback, kMotionPeriodMs, this);
    if (motion_timer_ == nullptr) {
        Stop();
        ESP_LOGE(TAG, "Could not create motion timer");
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
    if (motion_timer_) {
        lv_timer_delete(motion_timer_);
        motion_timer_ = nullptr;
    }
    if (root_) lv_obj_delete(root_);
    root_ = left_eye_ = right_eye_ = left_pupil_ = right_pupil_ = mouth_ = nullptr;
    blinking_ = false;
    look_x_ = look_y_ = 0;
    tick_ = 0;
}

void ChipoFace::SetState(State state) {
    if (state_ == state) return;
    state_ = state;
    ApplyExpression();
}

void ChipoFace::PositionEyes() {
    if (!left_eye_ || !right_eye_ || !left_pupil_ || !right_pupil_) return;

    const int32_t left_x = kCanvasWidth / 2 - kEyeGap / 2 - expression_eye_width_;
    const int32_t right_x = kCanvasWidth / 2 + kEyeGap / 2;
    lv_obj_set_pos(left_eye_, left_x, kEyeY);
    lv_obj_set_pos(right_eye_, right_x, kEyeY);

    const int32_t dx = pupil_x_ + look_x_;
    const int32_t dy = pupil_y_ + look_y_;
    lv_obj_set_size(left_pupil_, kPupilW, kPupilH);
    lv_obj_set_size(right_pupil_, kPupilW, kPupilH);
    lv_obj_set_pos(left_pupil_, left_x + (expression_eye_width_ - kPupilW) / 2 + dx,
                   kEyeY + (expression_eye_height_ - kPupilH) / 2 + dy);
    lv_obj_set_pos(right_pupil_, right_x + (expression_eye_width_ - kPupilW) / 2 + dx,
                   kEyeY + (expression_eye_height_ - kPupilH) / 2 + dy);
}

void ChipoFace::ApplyExpression() {
    if (!root_ || !left_eye_ || !right_eye_ || !mouth_) return;

    int32_t eye_w = 62;
    int32_t eye_h = 70;
    int32_t pupil_x = 0;
    int32_t pupil_y = 0;
    int32_t mouth_w = 42;
    int32_t mouth_h = 8;
    int32_t mouth_y = kMouthY;
    int32_t mouth_radius = LV_RADIUS_CIRCLE;

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
    pupil_x_ = pupil_x;
    pupil_y_ = pupil_y;
    look_x_ = 0;
    look_y_ = 0;
    if (!blinking_) {
        lv_obj_set_size(left_eye_, eye_w, eye_h);
        lv_obj_set_size(right_eye_, eye_w, eye_h);
        lv_obj_set_style_radius(left_eye_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_radius(right_eye_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        PositionEyes();
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
    if (self) self->Tick();
}

void ChipoFace::Tick() {
    if (!root_ || !mouth_) return;
    ++tick_;

    switch (state_) {
    case State::Speaking: {
        // No audio level is available here, so the mouth just "talks" while the
        // device is in the Speaking state. It stops as soon as the state changes.
        const int32_t h = kSpeakMouth[tick_ % kSpeakMouthCount];
        lv_obj_set_size(mouth_, kSpeakMouthWidth, h);
        lv_obj_set_pos(mouth_, (kCanvasWidth - kSpeakMouthWidth) / 2, kSpeakMouthCenterY - h / 2);
        break;
    }
    case State::Idle:
    case State::Connecting:
        if (!blinking_ && tick_ % kLookChangeTicks == 0) {
            // Small glance around, kept well inside the eye (pupil margin is 20 px).
            look_x_ = static_cast<int32_t>(lv_rand(0, 12)) - 6;
            look_y_ = static_cast<int32_t>(lv_rand(0, 8)) - 4;
            PositionEyes();
        }
        break;
    default:
        break;
    }
}

void ChipoFace::Blink() {
    if (!left_eye_ || !right_eye_ || !left_pupil_ || !right_pupil_ || blinking_) return;

    // Next blink at an irregular interval so it does not look mechanical.
    if (blink_timer_) {
        lv_timer_set_period(blink_timer_, kBlinkMinMs + lv_rand(0, kBlinkJitterMs));
    }

    blinking_ = true;
    // Close towards the vertical centre of the eye instead of collapsing to its top edge.
    const int32_t closed_y = kEyeY + (expression_eye_height_ - kBlinkClosedHeight) / 2;
    lv_obj_set_height(left_eye_, kBlinkClosedHeight);
    lv_obj_set_height(right_eye_, kBlinkClosedHeight);
    lv_obj_set_y(left_eye_, closed_y);
    lv_obj_set_y(right_eye_, closed_y);
    lv_obj_set_height(left_pupil_, 1);
    lv_obj_set_height(right_pupil_, 1);
    lv_obj_set_y(left_pupil_, kEyeY + expression_eye_height_ / 2);
    lv_obj_set_y(right_pupil_, kEyeY + expression_eye_height_ / 2);

    restore_timer_ = lv_timer_create(RestoreEyesTimerCallback, kBlinkClosedMs, this);
    if (!restore_timer_) {
        blinking_ = false;
        ApplyExpression();
    } else {
        lv_timer_set_repeat_count(restore_timer_, 1);
    }
}
