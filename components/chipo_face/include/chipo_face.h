#pragma once

#include <cstdint>
#include "lvgl.h"

/**
 * CHIPO Face: a floating expressive face made only of luminous eyes and mouth.
 *
 * Start it from the UI/LVGL task after the display is initialized.
 * All public methods must be called from that same UI task.
 * This component does not initialize display, touch, audio, Wi-Fi or PMU.
 */
class ChipoFace {
public:
    enum class State : uint8_t {
        Idle,
        Listening,
        Thinking,
        Speaking,
        Connecting,
        Error
    };

    ChipoFace() = default;
    ~ChipoFace();

    ChipoFace(const ChipoFace&) = delete;
    ChipoFace& operator=(const ChipoFace&) = delete;

    bool Start(lv_obj_t* parent);
    void Stop();
    void SetState(State state);
    State GetState() const { return state_; }
    bool IsRunning() const { return root_ != nullptr; }

private:
    static void BlinkTimerCallback(lv_timer_t* timer);
    static void RestoreEyesTimerCallback(lv_timer_t* timer);
    static void MotionTimerCallback(lv_timer_t* timer);
    void ApplyExpression();
    void PositionEyes();
    void Blink();
    void Tick();

    lv_obj_t* root_ = nullptr;
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_obj_t* left_pupil_ = nullptr;
    lv_obj_t* right_pupil_ = nullptr;
    lv_obj_t* mouth_ = nullptr;
    lv_timer_t* blink_timer_ = nullptr;
    lv_timer_t* restore_timer_ = nullptr;
    lv_timer_t* motion_timer_ = nullptr;
    State state_ = State::Idle;
    int32_t expression_eye_width_ = 62;
    int32_t expression_eye_height_ = 70;
    int32_t pupil_x_ = 0;   // fixed gaze offset of the current expression
    int32_t pupil_y_ = 0;
    int32_t look_x_ = 0;    // small random wander on top of the gaze (Idle/Connecting)
    int32_t look_y_ = 0;
    uint32_t tick_ = 0;
    bool blinking_ = false;
};
