#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "chipo_tilt.h"
#include "lvgl.h"

#ifdef ESP_PLATFORM
class ChipoImu;
#endif

/**
 * CHIPO Face: a floating expressive face made only of luminous eyes and mouth.
 *
 * Start it from the UI/LVGL task after the display is initialized.
 * All public methods must be called from that same UI task (the LVGL lock must be held).
 * This component does not initialize display, touch, audio, Wi-Fi or PMU.
 *
 * Every expression change is eased (nothing snaps), blinking is time based, and the face sits
 * on a spring so tilting CHIPO makes it slide, wobble and, when tipped far enough, panic.
 */
class ChipoFace {
public:
    enum class State : uint8_t {
        Idle,
        Listening,
        Thinking,
        Speaking,
        Connecting,
        Error,
        Sleep
    };

    // Returns true and fills `sample` (accelerometer, g) when a new reading is available.
    using TiltSource = std::function<bool(ImuSample* sample)>;

    // What the face is actually showing right now (state plus tilt reactions).
    enum class Mood : uint8_t { Normal, Startled, Dizzy, Relieved };

    struct DebugInfo {
        State shown_state;
        Mood mood;
        int face_dx, face_dy;
        int fall_dir;  // -1 / +1: slipping off the left / right side, 0: not falling
        bool fell;     // tilted past the fall angle
        int eye_w, eye_h, mouth_w, mouth_h;
        bool thinking_dots_visible;
        bool tilt_calibrated;
        float tilt_angle_deg;
    };

    ChipoFace();
    ~ChipoFace();

    ChipoFace(const ChipoFace&) = delete;
    ChipoFace& operator=(const ChipoFace&) = delete;

    bool Start(lv_obj_t* parent);
    void Stop();

    // Base state coming from the assistant (see integration/xiaozhi/chipo_state_map.h).
    void SetState(State state);
    State GetState() const { return state_; }

    // Overlay: the user's speech was understood and the model is working on the answer.
    // Cleared automatically by Speaking/Error/Connecting, or after CHIPO_THINKING_TIMEOUT_MS.
    void SetThinking(bool thinking);

    // Replace the tilt sensor (tests, other boards). Pass nullptr to disable tilt reactions.
    void SetTiltSource(TiltSource source);

    bool IsRunning() const { return root_ != nullptr; }

    // One animation step. Called by the internal LVGL timer; public so tests can drive time.
    void Step(uint32_t now_ms);
    DebugInfo Debug() const;

private:
    static void MotionTimerCallback(lv_timer_t* timer);

    struct Pose {
        float eye_w, eye_h;
        float pupil_w, pupil_h, pupil_x, pupil_y;
        float mouth_w, mouth_h, mouth_cy;
        float glow_w, glow_opa;
    };
    struct Layout {
        int16_t eye_w, eye_h, eye_y;
        int16_t left_eye_x, right_eye_x;
        int16_t pupil_w, pupil_h, pupil_y, left_pupil_x, right_pupil_x;
        int16_t mouth_w, mouth_h, mouth_x, mouth_y;
        int16_t glow_w, glow_opa;
        int16_t face_dx, face_dy;
        bool pupils_visible;
    };

    State ShownState(uint32_t now_ms) const;
    Pose TargetPose(State shown, Mood mood, uint32_t now_ms, const TiltOutput& tilt);
    float BlinkAmount(uint32_t now_ms);
    void ApplyLayout(const Layout& layout);
    void UpdateDots(uint32_t now_ms, bool visible);

    lv_obj_t* root_ = nullptr;
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_obj_t* left_pupil_ = nullptr;
    lv_obj_t* right_pupil_ = nullptr;
    lv_obj_t* mouth_ = nullptr;
    lv_obj_t* dots_[3] = {nullptr, nullptr, nullptr};
    lv_timer_t* motion_timer_ = nullptr;

    State state_ = State::Idle;
    State shown_ = State::Idle;
    bool thinking_ = false;
    uint32_t thinking_since_ms_ = 0;
    uint32_t idle_since_ms_ = 0;

    Pose cur_{};
    bool pose_initialised_ = false;
    Layout applied_{};
    bool layout_applied_ = false;
    uint32_t last_step_ms_ = 0;
    bool stepped_ = false;

    // Spring that carries the whole face (tilt, startle jump).
    float face_x_ = 0, face_y_ = 0, vel_x_ = 0, vel_y_ = 0;

    // Gaze wander and blinking.
    float look_x_ = 0, look_y_ = 0;
    uint32_t next_look_ms_ = 0;
    int blink_phase_ = 0;
    uint32_t blink_t0_ = 0;
    uint32_t next_blink_ms_ = 0;
    bool blink_was_double_ = false;

    // Tilt reactions.
    ChipoTilt tilt_;
    TiltSource tilt_source_;
    bool fell_ = false;
    int fall_dir_ = 0;
    uint32_t startle_until_ms_ = 0;
    uint32_t relief_until_ms_ = 0;
    Mood mood_ = Mood::Normal;
    bool calibrated_logged_ = false;
    uint32_t next_debug_log_ms_ = 0;

    uint32_t speak_step_ms_ = 0;
    uint32_t speak_index_ = 0;
    bool dots_visible_ = false;
    int dots_opa_[3] = {-1, -1, -1};

#ifdef ESP_PLATFORM
    std::unique_ptr<ChipoImu> imu_;
#endif
};
