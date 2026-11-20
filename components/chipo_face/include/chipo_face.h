#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "chipo_audio_level.h"
#include "chipo_fx.h"
#include "chipo_fx_logic.h"
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
        Grumpy,      // flat eyes, red glow, flat mouth
        Dance        // happy squint, mouth follows the music, colours cycle (shown while dancing)
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
        bool gaze_active;        // the eyes follow a finger on the screen
        float gaze_px_x, gaze_px_y;  // where the pupils are told to look (px from the centre)
        int stretches;           // yawn-and-stretch moves since start
        int slow_blinks;         // slow loving blinks since start
        int fx_hearts, fx_sparkles, fx_tears, fx_zzz;  // live decorations
        bool crying;             // tears are running
        bool tantrum;            // sore-loser tantrum in progress
        Costume costume;         // costume currently worn (date or voice)
        bool dancing;
        int dance_hops;          // beat hops since the dance started
        float dance_level;       // 0..1 loudness the dance reacts to
    };

    ChipoFace();
    ~ChipoFace();

    ChipoFace(const ChipoFace&) = delete;
    ChipoFace& operator=(const ChipoFace&) = delete;

    bool Start(lv_obj_t* parent);
    // AMOLED burn-in protection: the whole face drifts a few pixels (set slowly from outside; not part of the face's own
    // position, so tilt and fall reactions are untouched).
    void SetPixelShift(int dx, int dy);
    // Eye colour chosen by the owner ("ojos rosa"): replaces CHIPO's default blue glow. Emotions that have their
    // own colour (love = pink, happy = gold...) keep it. ClearEyeTint() goes back to blue.
    void SetEyeTint(uint32_t rgb);
    void ClearEyeTint();
    bool has_eye_tint() const { return has_tint_; }
    uint32_t eye_tint() const { return tint_rgb_; }
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

    // The finger is on the screen: the eyes follow it (nx, ny in -1..1 from the screen centre). Call it on every touch
    // move; it lets go by itself ~0.5 s after the last call. StopLookAt() lets go at once.
    void LookAt(float nx, float ny);
    void StopLookAt();
    // A shower of hearts or sparkles around the face (petting, "gracias", a win...). Hearts also give a slow loving blink.
    enum class Burst : uint8_t { Hearts, Sparkles };
    void Celebrate(Burst burst, int count = 5);
    // Sad face with tears for `ms` (a very neglected CHIPO, or after losing a game).
    void Cry(uint32_t ms);
    // Sore loser: a short angry tantrum (face shaking, red glow) that ends in tears.
    void SoreLoser();
    // Costumes: Santa hat (by date: 1 Dec - 6 Jan), sunglasses, moustache. SetCostume is the owner's choice (by voice)
    // and wins over the date; Costume::None takes everything off and keeps it off until the next start.
    using DateSource = std::function<bool(int* month, int* day)>;  // false: the clock is not set yet
    void SetDateSource(DateSource source) { date_source_ = std::move(source); }
    void SetCostume(Costume costume);
    Costume costume() const { return costume_; }
    // Dance: the face bounces and sways, the mouth follows the loudness and the glow cycles through colours. The beat hops
    // come from loudness peaks of the level source (see chipo_audio_level.h); with no sound it just sways gently.
    // Ends by itself after 3 minutes. It keeps CHIPO awake while it lasts.
    using LevelSource = std::function<float()>;  // 0..1
    void SetLevelSource(LevelSource source) { level_source_ = std::move(source); }
    void SetDance(bool on);
    bool dancing() const { return dance_on_; }
    // One slow, affectionate blink (like a cat) instead of the quick one.
    void SlowBlink();

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
        int16_t brow_rot;     // tenths of a degree, left brow (the right one mirrors it)
        int16_t brow_lift;    // px above the resting place
        bool brows_visible;
        bool blush_visible;   // only while CHIPO likes something
        uint8_t mouth_kind;   // MouthKind
        uint8_t eye_arc;      // 0 open eyes, 1 happy arcs (like ^ ^), 2 sleepy arcs (like u u)
    };

    bool has_tint_ = false;
    uint32_t tint_rgb_ = 0;

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
    lv_obj_t* tongue_ = nullptr;      // red tongue inside the mouth (cute skin)
    lv_obj_t* eye_arc_[2] = {nullptr, nullptr};  // closed eyes drawn as arcs (happy / asleep)
    lv_obj_t* inner_ = nullptr;       // dark inside of the open mouth
    lv_obj_t* brow_[2] = {nullptr, nullptr};
    lv_obj_t* blush_[2] = {nullptr, nullptr};  // soft translucent blush under the eyes
    lv_obj_t* bowl_ = nullptr;        // lower half of an ellipse, clipped by mouth_: the smile
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
    int shift_x_ = 0, shift_y_ = 0;
    lv_obj_t* sun_ = nullptr;       // synthwave sun on the horizon (upper half; the mask hides the rest)
    lv_obj_t* sun_mask_ = nullptr;  // black ground below the horizon
    lv_obj_t* horizon_ = nullptr;   // neon horizon line
    int sun_opa_applied_ = -1;
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

    // Decorations and small behaviours (see chipo_fx_logic.h).
    FxEngine fx_;
    ChipoFx fx_view_;
    int fx_w_ = 368, fx_h_ = 448;
    float fs_ = 1.0f;  // face scale: 1.0 on the 368 px wide screen, bigger on larger panels (480 px -> 1.3)
    int S(float v) const { return static_cast<int>(v * fs_ + (v >= 0 ? 0.5f : -0.5f)); }
    void FaceCentre(float* x, float* y) const;
    float gaze_nx_ = 0, gaze_ny_ = 0;     // finger position, -1..1
    uint32_t gaze_until_ms_ = 0;
    bool gaze_on_ = false;
    float gaze_px_x_ = 0, gaze_px_y_ = 0;  // eased pupil target
    float gaze_lean_ = 0;                  // eased lean of the face towards the finger
    uint32_t tired_t0_ = 0;
    uint32_t yawn_index_ = 0xFFFFFFFFu;
    int stretches_ = 0;
    bool slow_blink_pending_ = false, blink_slow_ = false;
    int slow_blinks_ = 0;
    uint32_t next_slow_blink_ms_ = 0;

    // Dance.
    LevelSource level_source_;
    bool dance_on_ = false;
    uint32_t dance_until_ms_ = 0, dance_t0_ = 0, next_hop_ms_ = 0;
    float dance_level_ = 0, dance_avg_ = 0;
    int dance_hops_ = 0, hop_dir_ = 1;

    // Tears, tantrum, sleep, costumes.
    uint32_t tears_until_ms_ = 0, next_tear_ms_ = 0;
    int tear_side_ = 0;
    uint32_t tantrum_until_ms_ = 0, next_kick_ms_ = 0;
    int kick_dir_ = 1;
    uint32_t next_zzz_ms_ = 0;
    DateSource date_source_;
    Costume costume_ = Costume::None;
    bool costume_manual_ = false;
    uint32_t next_date_check_ms_ = 0;
    // Hat: dome, band, tip, pompom. Moustache: two curls.
    static constexpr int kHatPieces = 4;
    static constexpr int kMoustachePieces = 2;
    lv_obj_t* hat_[kHatPieces] = {};
    lv_obj_t* moustache_[kMoustachePieces] = {};
    int hat_bob_applied_ = -9999;
    bool hat_shown_ = false, moustache_shown_ = false;
    void CreateCostumes();
    void UpdateCostumes(uint32_t now_ms);
    void EmitStateFx(State shown, Mood mood, uint32_t now_ms);

    uint32_t speak_step_ms_ = 0;
    uint32_t speak_index_ = 0;
    bool dots_visible_ = false;
    int dots_opa_[3] = {-1, -1, -1};

#ifdef ESP_PLATFORM
    std::unique_ptr<ChipoImu> imu_;
#endif
};
