#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "chipo_gyro.h"
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
 * With the gyroscope it also reacts to nodding ("yes"), shaking ("no") and spinning (and gets
 * dizzy after a long spin). On top of the base state it can show emotions (see ShowEmotion).
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

    // Expressions laid over Idle, Listening and Speaking (while speaking the mouth keeps moving
    // with the voice). Hidden while Thinking, Connecting, Error and Sleep, and by Startled/Dizzy.
    enum class Emotion : uint8_t {
        None,
        Happy,       // squinting smile
        Laugh,       // squinting, mouth bouncing
        Wink,        // right eye closed
        Love,        // pink glow that beats, big round pupils
        Sad,         // eyes down, small low mouth, dim glow
        Tired,       // half-closed eyes, yawns; shown by itself shortly before falling asleep
        Suspicious,  // one eye narrower, looking sideways
        Surprised,   // wide eyes, round mouth
        Whee,        // pupils circling fast (shown while spinning)
        No,          // pupils swinging left and right (shown after shaking "no")
        Grumpy       // flat eyes, red glow, flat mouth
    };

    // CHIPO DRIVE (car mode) -----------------------------------------------------------------
    // Called for every IMU sample (all of them, not only the latest one) with the sensor clock.
    // Told when CHIPO has been lying on its side long enough to be a clock (true) and when it is stood up (false).
    using DeskClockObserver = std::function<void(bool on, int side)>;  // side: -1 left down, +1 right down
    void SetDeskClockObserver(DeskClockObserver observer) { desk_clock_observer_ = std::move(observer); }

    using SampleObserver = std::function<void(const ImuSample& sample, uint32_t now_ms)>;
    void SetSampleObserver(SampleObserver observer) { sample_observer_ = std::move(observer); }
    // Sunglasses slide down over the eyes (and up again when switched off). While the car mode is
    // on, the tilt reactions (fall, dizzy, shake, spin) are paused: the road would trigger them.
    void SetDriveMode(bool on);
    bool DriveMode() const { return drive_mode_; }
    // Lean of the face in a curve, -1..1 (positive: to the screen's right); the eyes look into the turn.
    void SetDriveLean(float lean) { drive_lean_ = lean < -1 ? -1 : (lean > 1 ? 1 : lean); }
    enum class DriveReaction : uint8_t { Accelerate, Brake, Bump, Abrupt };
    void DriveReact(DriveReaction reaction, float intensity);
    // 50 Hz IMU readout (needs the real board; no effect elsewhere).
    void SetFastSampling(bool fast);

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
        Emotion emotion;      // emotion currently on screen (None when there is none)
        int right_eye_h;
        uint32_t glow_rgb;
        bool gyro_available;  // gyroscope samples are arriving
        bool gyro_spinning;
        bool drive_mode;
        bool side_lying;   // on its side (the desk-clock side), whether or not the clock is up yet
        bool desk_clock;   // lying long enough: the clock is wanted
        float glasses_progress;  // 0: no glasses, 1: on the eyes
        bool road_visible;       // car scene shown behind the face
        float road_speed;        // decorative scroll speed 0..1.4
        float wheel_angle_deg;   // steering wheel marker, + = clockwise
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
    // Samples with has_gyro = true also drive the gyroscope gestures.
    void SetTiltSource(TiltSource source);

    // Show an emotion for `duration_ms`, then go back to the base expression. It wakes CHIPO up.
    void ShowEmotion(Emotion emotion, uint32_t duration_ms = 2500);
    // Emotion names as the assistant sends them (happy, laughing, sad, angry, loving, winking,
    // surprised, sleepy, confused...). Unknown names and "neutral" give Emotion::None.
    static Emotion EmotionFromName(const char* name);
    void ShowEmotionByName(const char* name, uint32_t duration_ms = 2500);
    // Winks and smiles by themselves now and then while idle (default: CHIPO_IDLE_QUIRKS).
    void SetIdleQuirks(bool enabled) { idle_quirks_ = enabled; }

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
        float right_k = 1.0f;  // right eye height as a share of the left one (winking, suspicion)
        float glow_r = 117, glow_g = 223, glow_b = 255;  // glow colour (default: CHIPO blue)
    };
    struct Layout {
        int16_t eye_w, eye_h, eye_y;
        int16_t right_eye_h, right_eye_y;
        int16_t left_eye_x, right_eye_x;
        int16_t pupil_w, pupil_h, pupil_y, left_pupil_x, right_pupil_x;
        int16_t right_pupil_h, right_pupil_y;
        bool right_pupil_visible;
        uint32_t glow_rgb;
        int16_t mouth_w, mouth_h, mouth_x, mouth_y;
        int16_t glow_w, glow_opa;
        int16_t face_dx, face_dy;
        bool pupils_visible;
    };

    uint32_t QuietMs(uint32_t now_ms) const;
    State ShownState(uint32_t now_ms) const;
    Emotion ActiveEmotion(State shown, Mood mood, uint32_t now_ms) const;
    void StartEmotion(Emotion emotion, uint32_t now_ms, uint32_t duration_ms);
    void ApplyEmotion(Pose& pose, Emotion emotion, State shown, uint32_t now_ms) const;
    Pose TargetPose(State shown, Mood mood, Emotion emotion, uint32_t now_ms,
                    const TiltOutput& tilt);
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
    // Aviator sunglasses: left lens, right lens, bridge, brow bar, left arm, right arm, left glint, right glint.
    static constexpr int kGlassesPieces = 8;
    lv_obj_t* glasses_[kGlassesPieces] = {};
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
    uint32_t dizzy_until_ms_ = 0;  // dizzy after a long spin
    Mood mood_ = Mood::Normal;
    bool calibrated_logged_ = false;
    uint32_t next_debug_log_ms_ = 0;

    // Gyroscope gestures and emotions.
    ChipoGyro gyro_;
    Emotion emotion_ = Emotion::None;
    uint32_t emotion_until_ms_ = 0;
    Emotion shown_emotion_ = Emotion::None;
    bool idle_quirks_ = true;
    uint32_t next_quirk_ms_ = 0;

    // CHIPO DRIVE.
    SampleObserver sample_observer_;
    DeskClockObserver desk_clock_observer_;
    bool side_lying_ = false;
    bool desk_clock_ = false;
    uint32_t side_lying_ms_ = 0;
    bool drive_mode_ = false;
    float drive_lean_ = 0;       // target, from the motion detector
    float drive_lean_cur_ = 0;   // eased
    float glasses_t_ = 0;        // 0..1 progress of the sunglasses
    float glasses_jolt_ = 0;     // glasses bumped by a jolt (+: down, -: up), decays
    bool glasses_shown_ = false;
    int glasses_applied_[4] = {-9999, -9999, -9999, -9999};  // eye_w, left_eye_x, dx, dy
    void UpdateGlasses(const Layout& layout, float dt);

    // Car scene behind the face (decoration, NOT a speed reading): lane dashes ahead, speed streaks
    // at the sides and a steering wheel that turns with the curves. Only while the car mode is on.
    static constexpr int kLaneDashes = 5;
    static constexpr int kStreaks = 8;
    lv_obj_t* road_ = nullptr;  // full-screen container below the face
    lv_obj_t* lane_[kLaneDashes] = {};
    lv_obj_t* streak_[kStreaks] = {};
    lv_obj_t* wheel_ = nullptr;
    bool road_shown_ = false;
    float road_speed_ = 0;      // 0..1.4, eased: how fast the scenery streams by
    float road_boost_ = 0;      // from acceleration (+) and braking (-), decays
    float road_jolt_ = 0;       // bump: the scenery jumps, decays
    float road_phase_ = 0, streak_phase_ = 0;
    float wheel_angle_ = 0;     // degrees, + = clockwise
    struct RoadPiece { int16_t x = -9999, y = 0, w = 0, h = 0; int16_t opa = -1; };
    RoadPiece road_applied_[kLaneDashes + kStreaks];
    int wheel_rot_applied_ = -9999, wheel_opa_applied_ = -1, wheel_y_applied_ = -9999;
    void CreateRoad(lv_obj_t* parent);
    void UpdateRoad(float dt, uint32_t now_ms);

    uint32_t speak_step_ms_ = 0;
    uint32_t speak_index_ = 0;
    bool dots_visible_ = false;
    int dots_opa_[3] = {-1, -1, -1};

#ifdef ESP_PLATFORM
    std::unique_ptr<ChipoImu> imu_;
#endif
};
