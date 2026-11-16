#pragma once

#include <cstdint>
#include "lvgl.h"

/**
 * CHIPO Face: visual layer for the XiaoZhi LVGL interface.
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
    void ApplyExpression();
    void Blink();

    lv_obj_t* root_ = nullptr;
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_obj_t* mouth_ = nullptr;
    lv_timer_t* blink_timer_ = nullptr;
    lv_timer_t* restore_timer_ = nullptr;
    State state_ = State::Idle;
    lv_coord_t expression_eye_height_ = 28;
    bool blinking_ = false;
};
