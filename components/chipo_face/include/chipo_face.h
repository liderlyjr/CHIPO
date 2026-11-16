#pragma once

#include <cstdint>
#include "lvgl.h"

/**
 * CHIPO Face: a floating expressive face made only of luminous eyes and mouth.
 * Start it from the UI/LVGL task after the display is initialized.
 * All public methods must be called from that same UI task.
 */
class ChipoFace {
public:
    enum class State : uint8_t { Idle, Listening, Thinking, Speaking, Connecting, Error };

    ChipoFace() = default;
    ~ChipoFace();
    ChipoFace(const ChipoFace&) = delete;
    ChipoFace& operator=(const ChipoFace&) = delete;

    bool Start(lv_obj_t* parent);
    void Stop();
    void SetState(State state);
    State GetState() const { return state_; }
    bool IsRunning() const { return root_ != nullptr; }

    // Called by the board IMU adapter from the LVGL/UI task.
    void OnMotion(int x_mg, int y_mg, int z_mg);
    void SetBatteryStatus(int level, bool charging, bool discharging);
    void SetTimerStatus(const char* text);\n    void SetWeatherStatus(const char* text);

private:
    enum class Interaction : uint8_t { None, Curious, Annoyed, Dizzy, Bored, Tilted };

    static void BlinkTimerCallback(lv_timer_t* timer);
    static void RestoreEyesTimerCallback(lv_timer_t* timer);
    static void MotionTimerCallback(lv_timer_t* timer);
    static void GazeAnimationCallback(void* var, int32_t value);
    static void BatteryPulseTimerCallback(lv_timer_t* timer);
    static void IdleTimerCallback(lv_timer_t* timer);
    static void PixelTimerCallback(lv_timer_t* timer);
    static void TouchEventCallback(lv_event_t* event);
    static void InteractionTimerCallback(lv_timer_t* timer);
    static void FallAnimationCallback(void* var, int32_t value);
    static void FallAnimationReadyCallback(lv_anim_t* animation);
    static void RecoverFallTimerCallback(lv_timer_t* timer);
    static void RecoveryAnimationCallback(void* var, int32_t value);
    static void RecoveryAnimationReadyCallback(lv_anim_t* animation);

    void ApplyExpression();
    void PositionEyes(lv_coord_t pupil_x, lv_coord_t pupil_y);
    void Blink();
    void HandleTouch(lv_event_t* event);
    void SetInteraction(Interaction interaction, uint32_t duration_ms);
    void ClearInteraction();
    void StartFall(int direction);
    void StartRecovery();
    void StartIdlePlay();
    void StopIdlePlay();

    lv_obj_t* root_ = nullptr;
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_obj_t* left_pupil_ = nullptr;
    lv_obj_t* right_pupil_ = nullptr;
    lv_obj_t* mouth_ = nullptr;
    lv_obj_t* battery_label_ = nullptr;
    lv_obj_t* timer_label_ = nullptr;\n    lv_obj_t* weather_label_ = nullptr;
    lv_obj_t* idle_pixel_ = nullptr;
    lv_timer_t* blink_timer_ = nullptr;
    lv_timer_t* restore_timer_ = nullptr;
    lv_timer_t* motion_timer_ = nullptr;
    lv_timer_t* interaction_timer_ = nullptr;
    lv_timer_t* fall_timer_ = nullptr;
    lv_timer_t* battery_timer_ = nullptr;
    lv_timer_t* idle_timer_ = nullptr;
    lv_timer_t* pixel_timer_ = nullptr;

    State state_ = State::Idle;
    Interaction interaction_ = Interaction::None;
    lv_coord_t expression_eye_width_ = 62;
    lv_coord_t expression_eye_height_ = 70;
    lv_coord_t touch_pupil_x_ = 0;
    lv_coord_t touch_pupil_y_ = 0;
    uint32_t last_tap_tick_ = 0;
    uint8_t tap_count_ = 0;
    int previous_ax_ = 0;
    int previous_ay_ = 0;
    int previous_az_ = 0;
    uint32_t last_shake_tick_ = 0;
    bool has_previous_motion_ = false;
    bool long_press_active_ = false;
    bool blinking_ = false;
    bool falling_ = false;
    bool battery_pulse_visible_ = true;
    lv_coord_t gaze_offset_x_ = 0;
    int gaze_direction_ = 1;
    int fall_direction_ = 1;
    uint32_t last_fall_tick_ = 0;
    uint32_t last_activity_tick_ = 0;
    lv_coord_t pixel_x_ = 12;
    lv_coord_t pixel_y_ = 18;
    int pixel_vx_ = 3;
    int pixel_vy_ = 2;
};
