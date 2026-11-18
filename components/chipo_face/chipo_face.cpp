#include "chipo_face.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <initializer_list>
#include <utility>

#include "chipo_config.h"
#include "esp_log.h"

#ifdef ESP_PLATFORM
#include "chipo_imu.h"
#include "esp_random.h"
#endif

namespace {
constexpr char TAG[] = "ChipoFace";

constexpr int32_t kCanvasWidth = 300;
constexpr int32_t kCanvasHeight = 220;
constexpr float kCenterX = kCanvasWidth / 2.0f;
constexpr float kEyeCenterY = 89.0f;
constexpr float kEyeGap = 34.0f;
constexpr float kMouthCenterY = 162.0f;
constexpr int32_t kDotsY = 196;

constexpr uint32_t kStepPeriodMs = 40;  // 25 animation steps per second
constexpr uint32_t kBlinkMinMs = 2800;
constexpr uint32_t kBlinkJitterMs = 3400;
constexpr float kBlinkCloseMs = 70.0f;
constexpr float kBlinkHoldMs = 45.0f;
constexpr float kBlinkOpenMs = 90.0f;
constexpr uint32_t kSpeakStepMs = 140;
constexpr uint32_t kStartleMs = 800;
constexpr uint32_t kReliefMs = 900;
// Falling off the screen (tilted past CHIPO_FALL_ANGLE_DEG): px/s^2 sideways and downwards, and how far
// the face centre must travel for the whole face (glow included) to be out of the 368 px wide screen.
constexpr float kFallAccelX = 1600.0f;
constexpr float kFallAccelY = 500.0f;
constexpr float kFallMaxDropY = 110.0f;
constexpr float kGoneX = 330.0f;
constexpr float kFallMinPush = 0.15f;  // less sideways pull than this: nothing to slide towards
constexpr float kNormalRangeX = 90.0f; // beyond this the face is flying in from the edge

// Mouth heights while speaking; the centre stays put so it opens and closes in place.
constexpr float kSpeakMouth[] = {28, 12, 22, 8, 26, 14, 20, 10};
constexpr uint32_t kSpeakMouthCount = sizeof(kSpeakMouth) / sizeof(kSpeakMouth[0]);

// Share of CHIPO_SLEEP_AFTER_MS spent looking sleepy before CHIPO falls asleep.
constexpr uint32_t kTiredStartMs =
    (CHIPO_TIRED_BEFORE_SLEEP_MS > 0 && CHIPO_SLEEP_AFTER_MS > CHIPO_TIRED_BEFORE_SLEEP_MS)
        ? CHIPO_SLEEP_AFTER_MS - CHIPO_TIRED_BEFORE_SLEEP_MS
        : 0xFFFFFFFFu;  // never
constexpr uint32_t kQuirkFirstMs = 30000;   // first idle wink/smile no sooner than this after start
constexpr uint32_t kQuirkEveryMs = 25000;   // then every 25 s + up to 25 s
constexpr uint32_t kQuirkDurationMs = 1400;
constexpr uint32_t kGestureEmotionMs = 1800;
constexpr uint32_t kSpinDizzyAfterMs = 2500;  // spin at least this long and CHIPO gets dizzy
constexpr uint32_t kSpinDizzyMs = 2800;

constexpr uint32_t kGlowBlue = 0x75DFFF;
constexpr uint32_t kEyeCore = 0xDDF8FF;
constexpr uint32_t kPupil = 0x06121B;

constexpr float kTwoPi = 6.2831853f;

float Clamp(float v, float lo, float hi) { return std::min(hi, std::max(lo, v)); }

void Ease(float& current, float target, float dt, float tau) {
    current += (target - current) * (1.0f - std::exp(-dt / tau));
}

// The face overlays XiaoZhi's own UI: it must never swallow touches or scroll events.
void MakeInert(lv_obj_t* object) {
    lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

void StyleGlow(lv_obj_t* object, uint32_t color, int32_t glow_width) {
    lv_obj_remove_style_all(object);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(object, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(object, lv_color_hex(kGlowBlue), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(object, glow_width, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(object, LV_OPA_60, LV_PART_MAIN);
    MakeInert(object);
}

void StylePupil(lv_obj_t* object) {
    lv_obj_remove_style_all(object);
    lv_obj_set_style_bg_color(object, lv_color_hex(kPupil), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(object, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    MakeInert(object);
}

int16_t Round16(float v) { return static_cast<int16_t>(std::lround(v)); }
}  // namespace

ChipoFace::ChipoFace() : tilt_(CHIPO_TILT_MIRROR_X != 0) {
    idle_quirks_ = CHIPO_IDLE_QUIRKS != 0;
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

#ifdef ESP_PLATFORM
    lv_rand_set_seed(esp_random());
#endif

    root_ = lv_obj_create(parent);
    if (root_ == nullptr) return false;

    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, kCanvasWidth, kCanvasHeight);
    lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, LV_PART_MAIN);
    MakeInert(root_);

    left_eye_ = lv_obj_create(root_);
    right_eye_ = lv_obj_create(root_);
    left_pupil_ = lv_obj_create(root_);
    right_pupil_ = lv_obj_create(root_);
    mouth_ = lv_obj_create(root_);
    for (auto& dot : dots_) dot = lv_obj_create(root_);

    bool ok = left_eye_ && right_eye_ && left_pupil_ && right_pupil_ && mouth_;
    for (auto* dot : dots_) ok = ok && dot != nullptr;
    if (!ok) {
        Stop();
        ESP_LOGE(TAG, "Could not create face objects");
        return false;
    }

    StyleGlow(left_eye_, kEyeCore, 20);
    StyleGlow(right_eye_, kEyeCore, 20);
    StylePupil(left_pupil_);
    StylePupil(right_pupil_);
    StyleGlow(mouth_, kGlowBlue, 20);
    for (int i = 0; i < 3; ++i) {
        StyleGlow(dots_[i], kGlowBlue, 8);
        lv_obj_set_size(dots_[i], 8, 8);
        lv_obj_set_pos(dots_[i], static_cast<int32_t>(kCenterX) + (i - 1) * 22 - 4, kDotsY);
        lv_obj_add_flag(dots_[i], LV_OBJ_FLAG_HIDDEN);
    }

    const uint32_t now = lv_tick_get();
    cur_ = Pose{62, 70, 22, 30, 0, 0, 42, 8, kMouthCenterY, 20, 60};
    pose_initialised_ = true;
    layout_applied_ = false;
    stepped_ = false;
    idle_since_ms_ = now;
    next_blink_ms_ = now + 1800;
    next_look_ms_ = now + 1500;
    blink_phase_ = 0;
    face_x_ = face_y_ = vel_x_ = vel_y_ = 0;
    fell_ = false;
    fall_dir_ = 0;
    mood_ = Mood::Normal;
    startle_until_ms_ = relief_until_ms_ = dizzy_until_ms_ = 0;
    gyro_.Reset();
    emotion_ = shown_emotion_ = Emotion::None;
    emotion_until_ms_ = 0;
    next_quirk_ms_ = 0;  // scheduled the first time CHIPO is idle with quirks on
    calibrated_logged_ = false;
    next_debug_log_ms_ = now + 3000;
    tilt_.Reset();

#if defined(ESP_PLATFORM) && CHIPO_ENABLE_IMU
    if (!tilt_source_) {
        auto imu = std::make_unique<ChipoImu>();
        if (imu->Start()) {
            ChipoImu* raw = imu.get();
            tilt_source_ = [raw](ImuSample* sample) { return raw->Poll(sample); };
            imu_ = std::move(imu);
        }
    }
#endif

    motion_timer_ = lv_timer_create(MotionTimerCallback, kStepPeriodMs, this);
    if (motion_timer_ == nullptr) {
        Stop();
        ESP_LOGE(TAG, "Could not create animation timer");
        return false;
    }

    Step(now);
    ESP_LOGI(TAG, "CHIPO Face initialized (tilt reactions %s)", tilt_source_ ? "on" : "off");
    return true;
}

void ChipoFace::Stop() {
    if (motion_timer_) {
        lv_timer_delete(motion_timer_);
        motion_timer_ = nullptr;
    }
#ifdef ESP_PLATFORM
    tilt_source_ = nullptr;
    imu_.reset();
#endif
    if (root_) lv_obj_delete(root_);
    root_ = left_eye_ = right_eye_ = left_pupil_ = right_pupil_ = mouth_ = nullptr;
    for (auto& dot : dots_) dot = nullptr;
    dots_visible_ = false;
    for (int& o : dots_opa_) o = -1;
}

void ChipoFace::SetTiltSource(TiltSource source) {
#ifdef ESP_PLATFORM
    imu_.reset();
#endif
    tilt_source_ = std::move(source);
}

void ChipoFace::SetState(State state) {
    if (state_ == state) return;
    state_ = state;
    const uint32_t now = lv_tick_get();
    if (state == State::Idle) {
        idle_since_ms_ = now;
    } else if (state != State::Listening) {
        thinking_ = false;  // Speaking, Error, Connecting... the wait is over
    }
}

void ChipoFace::SetThinking(bool thinking) {
    if (!thinking) {
        thinking_ = false;
        return;
    }
    if (state_ == State::Idle || state_ == State::Listening) {
        thinking_ = true;
        thinking_since_ms_ = lv_tick_get();
    }
}

// Time with no voice, no touch and no movement (the smaller of the two clocks).
uint32_t ChipoFace::QuietMs(uint32_t now_ms) const {
    const uint32_t quiet = now_ms - idle_since_ms_;
    const uint32_t untouched = lv_display_get_default() ? lv_display_get_inactive_time(nullptr) : quiet;
    return std::min(quiet, untouched);
}

ChipoFace::State ChipoFace::ShownState(uint32_t now_ms) const {
    if (state_ == State::Error) return State::Error;
    if (thinking_ && (state_ == State::Idle || state_ == State::Listening)) return State::Thinking;
#if CHIPO_SLEEP_AFTER_MS > 0
    if (state_ == State::Idle && QuietMs(now_ms) >= CHIPO_SLEEP_AFTER_MS) return State::Sleep;
#else
    (void)now_ms;
#endif
    return state_;
}

ChipoFace::Emotion ChipoFace::ActiveEmotion(State shown, Mood mood, uint32_t now_ms) const {
    if (mood != Mood::Normal) return Emotion::None;
    if (shown != State::Idle && shown != State::Listening && shown != State::Speaking) {
        return Emotion::None;
    }
    if (emotion_ != Emotion::None && now_ms < emotion_until_ms_) return emotion_;
    // Shortly before falling asleep CHIPO looks sleepy and yawns.
    if (shown == State::Idle && state_ == State::Idle && QuietMs(now_ms) >= kTiredStartMs) {
        return Emotion::Tired;
    }
    return Emotion::None;
}

void ChipoFace::StartEmotion(Emotion emotion, uint32_t now_ms, uint32_t duration_ms) {
    emotion_ = emotion;
    emotion_until_ms_ = now_ms + duration_ms;
    if (emotion != Emotion::None) idle_since_ms_ = now_ms;  // wakes CHIPO up
}

void ChipoFace::ShowEmotion(Emotion emotion, uint32_t duration_ms) {
    StartEmotion(emotion, lv_tick_get(), duration_ms);
}

ChipoFace::Emotion ChipoFace::EmotionFromName(const char* name) {
    if (name == nullptr) return Emotion::None;
    char key[24];
    size_t n = 0;
    for (; name[n] != '\0' && n < sizeof(key) - 1; ++n) {
        key[n] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[n])));
    }
    key[n] = '\0';
    struct Entry {
        const char* name;
        Emotion emotion;
    };
    static const Entry kTable[] = {
        {"happy", Emotion::Happy},         {"delicious", Emotion::Happy},
        {"relaxed", Emotion::Happy},       {"confident", Emotion::Happy},
        {"cool", Emotion::Happy},          {"laughing", Emotion::Laugh},
        {"funny", Emotion::Laugh},         {"silly", Emotion::Laugh},
        {"winking", Emotion::Wink},        {"loving", Emotion::Love},
        {"kissy", Emotion::Love},          {"sad", Emotion::Sad},
        {"crying", Emotion::Sad},          {"embarrassed", Emotion::Sad},
        {"sleepy", Emotion::Tired},        {"confused", Emotion::Suspicious},
        {"surprised", Emotion::Surprised}, {"shocked", Emotion::Surprised},
        {"angry", Emotion::Grumpy},
    };
    for (const Entry& e : kTable) {
        if (std::strcmp(key, e.name) == 0) return e.emotion;
    }
    return Emotion::None;
}

void ChipoFace::ShowEmotionByName(const char* name, uint32_t duration_ms) {
    const Emotion e = EmotionFromName(name);
    if (e != Emotion::None) ShowEmotion(e, duration_ms);
}

float ChipoFace::BlinkAmount(uint32_t now_ms) {
    if (blink_phase_ == 0) {
        if (now_ms < next_blink_ms_) return 0.0f;
        blink_phase_ = 1;
        blink_t0_ = now_ms;
    }
    const float elapsed = static_cast<float>(now_ms - blink_t0_);
    if (blink_phase_ == 1) {
        if (elapsed < kBlinkCloseMs) return elapsed / kBlinkCloseMs;
        blink_phase_ = 2;
        blink_t0_ = now_ms;
        return 1.0f;
    }
    if (blink_phase_ == 2) {
        if (elapsed < kBlinkHoldMs) return 1.0f;
        blink_phase_ = 3;
        blink_t0_ = now_ms;
        return 1.0f;
    }
    if (elapsed < kBlinkOpenMs) return 1.0f - elapsed / kBlinkOpenMs;
    blink_phase_ = 0;
    // Now and then blink twice in a row, otherwise wait a random while.
    if (!blink_was_double_ && lv_rand(0, 4) == 0) {
        blink_was_double_ = true;
        next_blink_ms_ = now_ms + 170;
    } else {
        blink_was_double_ = false;
        next_blink_ms_ = now_ms + kBlinkMinMs + lv_rand(0, kBlinkJitterMs);
    }
    return 0.0f;
}

ChipoFace::Pose ChipoFace::TargetPose(State shown, Mood mood, Emotion emotion, uint32_t now_ms,
                                      const TiltOutput& tilt) {
    Pose p{62, 70, 22, 30, 0, 0, 42, 8, kMouthCenterY, 20, 60};
    const float t = static_cast<float>(now_ms);

    switch (shown) {
    case State::Idle:
        p.pupil_x = look_x_;
        p.pupil_y = look_y_;
        break;
    case State::Listening:
        p.eye_w = 66; p.eye_h = 78;
        p.mouth_w = 13; p.mouth_h = 13; p.mouth_cy = 161;
        // Soft pulsing glow: "I'm all ears".
        p.glow_w = 22 + 10 * (0.5f + 0.5f * std::sin(kTwoPi * t / 1200.0f));
        p.glow_opa = 72;
        break;
    case State::Thinking: {
        const float angle = kTwoPi * static_cast<float>(now_ms % 1800) / 1800.0f;
        p.eye_h = 66;
        p.pupil_x = 8 * std::cos(angle);
        p.pupil_y = -7 + 5 * std::sin(angle);
        p.mouth_w = 28; p.mouth_h = 7;
        break;
    }
    case State::Speaking:
        p.mouth_w = 34;
        p.mouth_h = kSpeakMouth[speak_index_ % kSpeakMouthCount];
        p.mouth_cy = 163;
        break;
    case State::Connecting:
        p.eye_w = 58; p.eye_h = 64;
        p.mouth_w = 24; p.mouth_h = 7;
        p.pupil_x = look_x_;
        p.pupil_y = look_y_;
        break;
    case State::Error:
        p.eye_h = 48;
        p.pupil_y = 4;
        p.mouth_w = 36; p.mouth_h = 7; p.mouth_cy = 167;
        break;
    case State::Sleep:
        p.eye_h = 4; p.pupil_h = 2;
        p.mouth_w = 16; p.mouth_h = 5; p.mouth_cy = 164;
        p.glow_w = 10;
        p.glow_opa = 28 + 10 * std::sin(kTwoPi * t / 4000.0f);  // slow breathing
        break;
    }

    if (emotion != Emotion::None) ApplyEmotion(p, emotion, shown, now_ms);

    switch (mood) {
    case Mood::Startled:
        p.eye_w = 74; p.eye_h = 88;
        p.pupil_w = 14; p.pupil_h = 18;
        p.pupil_x = tilt.push_x * 6; p.pupil_y = 0;
        p.mouth_w = 24; p.mouth_h = 30; p.mouth_cy = 170;
        p.glow_w = 30; p.glow_opa = 85;
        break;
    case Mood::Dizzy:
        p.eye_w = 62; p.eye_h = 16;
        p.pupil_w = 22; p.pupil_h = 12;
        p.pupil_x = 9 * std::sin(kTwoPi * t / 1100.0f); p.pupil_y = 0;
        p.mouth_w = 22; p.mouth_h = 6; p.mouth_cy = 168;
        p.glow_w = 14; p.glow_opa = 40;
        break;
    case Mood::Relieved:
        p.eye_w = 62; p.eye_h = 56;
        p.pupil_h = 24; p.pupil_x = 0; p.pupil_y = 0;
        p.mouth_w = 54; p.mouth_h = 9; p.mouth_cy = 160;
        break;
    case Mood::Normal:
        if (tilt.calibrated && shown != State::Sleep) {
            // Leaning: the eyes follow the pull of gravity a little.
            p.pupil_x += tilt.push_x * 14.0f;
            if (tilt.angle_deg > 8.0f) p.pupil_y += 3.0f;
        }
        break;
    }

    // Pupils must stay inside the eye.
    const float max_x = std::max(0.0f, (p.eye_w - p.pupil_w) / 2.0f - 2.0f);
    const float max_y = std::max(0.0f, (p.eye_h - p.pupil_h) / 2.0f - 2.0f);
    p.pupil_x = Clamp(p.pupil_x, -max_x, max_x);
    p.pupil_y = Clamp(p.pupil_y, -max_y, max_y);
    return p;
}

void ChipoFace::ApplyEmotion(Pose& p, Emotion emotion, State shown, uint32_t now_ms) const {
    const float t = static_cast<float>(now_ms);
    const bool talking = shown == State::Speaking;  // then the mouth keeps following the voice
    auto glow = [&p](uint32_t rgb, float width, float opacity) {
        p.glow_r = static_cast<float>((rgb >> 16) & 0xFF);
        p.glow_g = static_cast<float>((rgb >> 8) & 0xFF);
        p.glow_b = static_cast<float>(rgb & 0xFF);
        p.glow_w = width;
        p.glow_opa = opacity;
    };
    auto mouth = [&](float w, float h, float cy) {
        if (talking) return;
        p.mouth_w = w; p.mouth_h = h; p.mouth_cy = cy;
    };

    switch (emotion) {
    case Emotion::None:
        break;
    case Emotion::Happy:
        p.eye_w = 66; p.eye_h = 36; p.pupil_h = 0;
        mouth(64, 22, 164);
        glow(0xFFD86B, 24, 70);
        break;
    case Emotion::Laugh:
        p.eye_w = 66; p.eye_h = 28; p.pupil_h = 0;
        mouth(60, 24 + 12 * (0.5f + 0.5f * std::sin(kTwoPi * t / 260.0f)), 166);
        glow(0xFFD86B, 26, 75);
        break;
    case Emotion::Wink:
        p.eye_w = 64; p.eye_h = 64;
        p.right_k = 0.07f;
        p.pupil_x = look_x_; p.pupil_y = look_y_;
        mouth(56, 14, 162);
        break;
    case Emotion::Love: {
        const float beat = 0.5f + 0.5f * std::sin(kTwoPi * t / 900.0f);
        p.eye_w = 66 + 4 * beat; p.eye_h = 74 + 5 * beat;
        p.pupil_w = 28; p.pupil_h = 34; p.pupil_x = 0; p.pupil_y = 0;
        mouth(38, 14, 164);
        glow(0xFF6FB5, 22 + 14 * beat, 80);
        break;
    }
    case Emotion::Sad:
        p.eye_w = 58; p.eye_h = 60;
        p.pupil_w = 24; p.pupil_h = 30; p.pupil_x = 0; p.pupil_y = 9;
        mouth(26, 7, 174);
        glow(0x8FA6FF, 14, 38);
        break;
    case Emotion::Tired: {
        // A yawn every 5 s: the eyes squeeze shut while the mouth opens wide.
        const float cycle = std::fmod(t, 5200.0f) / 5200.0f;
        const float yawn = cycle < 0.4f ? std::sin(cycle / 0.4f * 3.14159265f) : 0.0f;
        p.eye_w = 62; p.eye_h = 30 - 10 * yawn;
        p.pupil_w = 22; p.pupil_h = 14; p.pupil_x = 0; p.pupil_y = 3;
        mouth(18 + 14 * yawn, 8 + 26 * yawn, 164 + 3 * yawn);
        glow(kGlowBlue, 12, 34);
        break;
    }
    case Emotion::Suspicious:
        p.eye_w = 62; p.eye_h = 56;
        p.right_k = 0.55f;
        p.pupil_w = 18; p.pupil_h = 22; p.pupil_x = 10; p.pupil_y = -3;
        mouth(30, 6, 167);
        glow(0xFFB347, 20, 60);
        break;
    case Emotion::Surprised:
        p.eye_w = 72; p.eye_h = 84;
        p.pupil_w = 12; p.pupil_h = 14; p.pupil_x = 0; p.pupil_y = 0;
        mouth(22, 28, 169);
        glow(0xBFF2FF, 28, 82);
        break;
    case Emotion::Whee: {
        const float angle = kTwoPi * t / 450.0f;
        p.eye_w = 66; p.eye_h = 74;
        p.pupil_w = 22; p.pupil_h = 26;
        p.pupil_x = 12 * std::cos(angle); p.pupil_y = 12 * std::sin(angle);
        mouth(46, 30, 166);
        glow(0xD27BFF, 28, 85);
        break;
    }
    case Emotion::No:
        p.eye_w = 60; p.eye_h = 50;
        p.pupil_w = 20; p.pupil_h = 24;
        p.pupil_x = 11 * std::sin(kTwoPi * t / 520.0f); p.pupil_y = 0;
        mouth(30, 6, 167);
        break;
    case Emotion::Grumpy:
        p.eye_w = 60; p.eye_h = 34;
        p.pupil_w = 16; p.pupil_h = 16; p.pupil_x = 0; p.pupil_y = 4;
        mouth(40, 6, 171);
        glow(0xFF5B4A, 20, 70);
        break;
    }
}

void ChipoFace::UpdateDots(uint32_t now_ms, bool visible) {
    if (visible != dots_visible_) {
        dots_visible_ = visible;
        for (int i = 0; i < 3; ++i) {
            if (!dots_[i]) continue;
            if (visible) lv_obj_remove_flag(dots_[i], LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(dots_[i], LV_OBJ_FLAG_HIDDEN);
            dots_opa_[i] = -1;
        }
    }
    if (!visible) return;
    for (int i = 0; i < 3; ++i) {
        if (!dots_[i]) continue;
        const float phase = kTwoPi * static_cast<float>(now_ms) / 1200.0f - 0.9f * i;
        const float v = 0.5f + 0.5f * std::sin(phase);
        const int level = static_cast<int>(v * 7.0f + 0.5f);  // 8 brightness steps
        if (level != dots_opa_[i]) {
            dots_opa_[i] = level;
            lv_obj_set_style_bg_opa(dots_[i], static_cast<lv_opa_t>(50 + level * 29), LV_PART_MAIN);
        }
    }
}

void ChipoFace::MotionTimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoFace*>(lv_timer_get_user_data(timer));
    if (self) self->Step(lv_tick_get());
}

void ChipoFace::Step(uint32_t now_ms) {
    if (!root_) return;

    float dt = 0.04f;
    if (stepped_) dt = Clamp(static_cast<float>(now_ms - last_step_ms_) / 1000.0f, 0.001f, 0.1f);
    stepped_ = true;
    last_step_ms_ = now_ms;

    // ---- tilt sensor -----------------------------------------------------------------------
    bool nodded = false, shook = false, spin_ended = false;
    uint32_t spin_len_ms = 0;
    if (tilt_source_) {
        ImuSample sample;
        for (int guard = 0; guard < 4 && tilt_source_(&sample); ++guard) {
            tilt_.Update(sample, now_ms);
            if (sample.has_gyro) {
                const GyroOutput& g = gyro_.Update(sample, tilt_.output().calibrated,
                                                   tilt_.vertical_axis(), now_ms);
                nodded = nodded || g.nod;
                shook = shook || g.shake;
                if (g.spin_ended) {
                    spin_ended = true;
                    spin_len_ms = g.spin_ms;
                }
            }
        }
    }
    const TiltOutput tilt = tilt_.output();
    const GyroOutput gyro = gyro_.output();
    if (tilt.calibrated && !calibrated_logged_) {
        calibrated_logged_ = true;
        ESP_LOGI(TAG, "Tilt calibrated: home pose stored (vertical sensor axis %s)",
                 tilt_.vertical_axis() == 0 ? "X" : "Y");
    }
    if (tilt.startle) {
        startle_until_ms_ = now_ms + kStartleMs;
        vel_y_ -= 240.0f;           // a little jump of fright
        idle_since_ms_ = now_ms;    // wakes CHIPO up
    }
    // Gyroscope gestures: nod = "yes", shake = "no", spin = wheee (and dizzy if it lasts).
    if (nodded) {
        StartEmotion(Emotion::Happy, now_ms, kGestureEmotionMs);
        vel_y_ += 160.0f;  // a little dip of the head
    }
    if (shook) StartEmotion(Emotion::No, now_ms, kGestureEmotionMs);
    if (gyro.spinning) StartEmotion(Emotion::Whee, now_ms, 500);
    if (spin_ended && spin_len_ms >= kSpinDizzyAfterMs) dizzy_until_ms_ = now_ms + kSpinDizzyMs;
    if (tilt.calibrated) {
        if (!fell_ && tilt.angle_deg >= CHIPO_FALL_ANGLE_DEG) fell_ = true;
        else if (fell_ && tilt.angle_deg < CHIPO_FALL_ANGLE_DEG - 12) fell_ = false;
    } else {
        fell_ = false;
    }
    // Which side does it slip towards? The side gravity pulls (latched once there is a clear pull).
    if (!fell_) fall_dir_ = 0;
    else if (fall_dir_ == 0 && std::fabs(tilt.push_x) >= kFallMinPush) fall_dir_ = tilt.push_x > 0 ? 1 : -1;
    if (fell_) idle_since_ms_ = now_ms;

    // ---- state and mood --------------------------------------------------------------------
    if (thinking_ && now_ms - thinking_since_ms_ > CHIPO_THINKING_TIMEOUT_MS) thinking_ = false;
    const State shown = ShownState(now_ms);
    shown_ = shown;

    Mood mood = Mood::Normal;
    if (state_ != State::Error) {
        if (tilt.lying || now_ms < dizzy_until_ms_) mood = Mood::Dizzy;
        else if (fell_ || now_ms < startle_until_ms_) mood = Mood::Startled;
    }
    if (mood == Mood::Normal && (mood_ == Mood::Startled || mood_ == Mood::Dizzy) &&
        state_ != State::Error) {
        relief_until_ms_ = now_ms + kReliefMs;  // "phew"
        next_blink_ms_ = now_ms;                // with a blink
    }
    if (mood == Mood::Normal && now_ms < relief_until_ms_) mood = Mood::Relieved;
    mood_ = mood;

    // Idle wink or smile now and then. It does not count as activity: CHIPO still falls asleep.
    const bool emotion_running = emotion_ != Emotion::None && now_ms < emotion_until_ms_;
    if (shown != State::Idle) {
        next_quirk_ms_ = std::max(next_quirk_ms_, now_ms + 20000);
    } else if (idle_quirks_ && mood == Mood::Normal && !emotion_running) {
        if (next_quirk_ms_ == 0) {
            next_quirk_ms_ = now_ms + kQuirkFirstMs + lv_rand(0, 20000);
        } else if (now_ms >= next_quirk_ms_ && QuietMs(now_ms) < kTiredStartMs) {
            emotion_ = lv_rand(0, 2) == 0 ? Emotion::Happy : Emotion::Wink;
            emotion_until_ms_ = now_ms + kQuirkDurationMs;
            next_quirk_ms_ = now_ms + kQuirkEveryMs + lv_rand(0, kQuirkEveryMs);
        }
    }
    const Emotion emotion = ActiveEmotion(shown, mood, now_ms);
    shown_emotion_ = emotion;

    // ---- small timed behaviours ------------------------------------------------------------
    if (shown == State::Speaking && now_ms >= speak_step_ms_) {
        ++speak_index_;
        speak_step_ms_ = now_ms + kSpeakStepMs;
    }
    if (shown == State::Idle || shown == State::Connecting) {
        if (now_ms >= next_look_ms_) {
            look_x_ = static_cast<float>(lv_rand(0, 12)) - 6.0f;
            look_y_ = static_cast<float>(lv_rand(0, 8)) - 4.0f;
            next_look_ms_ = now_ms + 1800 + lv_rand(0, 1800);
        }
    } else {
        look_x_ = look_y_ = 0;
        next_look_ms_ = now_ms + 1200;
    }

    // ---- ease the pose towards its target --------------------------------------------------
    const Pose target = TargetPose(shown, mood, emotion, now_ms, tilt);
    const bool snappy = mood == Mood::Startled;
    const float tau_shape = snappy ? 0.04f
                            : mood == Mood::Relieved ? 0.12f
                            : emotion != Emotion::None ? 0.07f
                                                       : 0.09f;
    const float tau_mouth = shown == State::Speaking && mood == Mood::Normal ? 0.045f : tau_shape;
    Ease(cur_.eye_w, target.eye_w, dt, tau_shape);
    Ease(cur_.eye_h, target.eye_h, dt, tau_shape);
    Ease(cur_.pupil_w, target.pupil_w, dt, tau_shape);
    Ease(cur_.pupil_h, target.pupil_h, dt, tau_shape);
    Ease(cur_.pupil_x, target.pupil_x, dt, 0.05f);
    Ease(cur_.pupil_y, target.pupil_y, dt, 0.05f);
    Ease(cur_.mouth_w, target.mouth_w, dt, tau_mouth);
    Ease(cur_.mouth_h, target.mouth_h, dt, tau_mouth);
    Ease(cur_.mouth_cy, target.mouth_cy, dt, tau_mouth);
    Ease(cur_.glow_w, target.glow_w, dt, 0.15f);
    Ease(cur_.glow_opa, target.glow_opa, dt, 0.15f);
    Ease(cur_.right_k, target.right_k, dt, 0.05f);
    Ease(cur_.glow_r, target.glow_r, dt, 0.12f);
    Ease(cur_.glow_g, target.glow_g, dt, 0.12f);
    Ease(cur_.glow_b, target.glow_b, dt, 0.12f);

    // ---- spring that carries the whole face ------------------------------------------------
    // Tilted past the fall angle: the face slips, speeds up and drops off the screen on the side it
    // leans to. It stays gone while CHIPO is held that far over (unless it ends up lying flat: then
    // it comes back dizzy) and slides back in from that edge when CHIPO is stood up again.
    const bool falling = fell_ && fall_dir_ != 0 && !tilt.lying;
    float tx = 0, ty = 0;
    if (tilt.calibrated && !tilt.lying) {
        tx = Clamp(tilt.push_x * 170.0f, -64.0f, 64.0f);
        ty = Clamp(tilt.lean_z * 46.0f, 0.0f, 46.0f);
        if (mood == Mood::Startled) tx = Clamp(tx * 1.2f, -72.0f, 72.0f);
    }
    const bool flying_in = !falling && std::fabs(face_x_) > kNormalRangeX;
    if (falling) {
        vel_x_ += static_cast<float>(fall_dir_) * kFallAccelX * dt;
        vel_y_ += kFallAccelY * dt;
        face_x_ += vel_x_ * dt;
        face_y_ += vel_y_ * dt;
        if (std::fabs(face_x_) >= kGoneX) {  // gone: park it out of sight
            face_x_ = static_cast<float>(fall_dir_) * kGoneX;
            vel_x_ = 0;
        }
        if (face_y_ >= kFallMaxDropY) {
            face_y_ = kFallMaxDropY;
            vel_y_ = 0;
        }
    } else {
        constexpr float kSpring = 120.0f;
        const float damping = flying_in ? 20.0f : 8.8f;
        const int substeps = std::max(1, static_cast<int>(std::ceil(dt / 0.02f)));
        const float h = dt / static_cast<float>(substeps);
        for (int i = 0; i < substeps; ++i) {
            vel_x_ += (kSpring * (tx - face_x_) - damping * vel_x_) * h;
            vel_y_ += (kSpring * (ty - face_y_) - damping * vel_y_) * h;
            face_x_ += vel_x_ * h;
            face_y_ += vel_y_ * h;
        }
        if (!flying_in && std::fabs(face_x_) <= kNormalRangeX) {
            face_x_ = Clamp(face_x_, -84.0f, 84.0f);
            face_y_ = Clamp(face_y_, -48.0f, 64.0f);
        }
        if (std::fabs(vel_x_) < 0.05f && std::fabs(tx - face_x_) < 0.2f) { face_x_ = tx; vel_x_ = 0; }
        if (std::fabs(vel_y_) < 0.05f && std::fabs(ty - face_y_) < 0.2f) { face_y_ = ty; vel_y_ = 0; }
    }

    // ---- blink on top of the pose ----------------------------------------------------------
    float blink = BlinkAmount(now_ms);
    if (mood == Mood::Startled || shown == State::Sleep) blink = 0.0f;
    const float eye_h = std::max(4.0f, cur_.eye_h * (1.0f - blink));
    const float pupil_h = std::min(cur_.pupil_h * (1.0f - blink), eye_h - 2.0f);
    // The right eye can be narrower than the left one (wink, suspicion); otherwise identical.
    const float right_eye_h = std::max(4.0f, cur_.eye_h * cur_.right_k * (1.0f - blink));
    const float right_pupil_h = std::min(cur_.pupil_h * (1.0f - blink), right_eye_h - 2.0f);

    // ---- pose -> pixels --------------------------------------------------------------------
    Layout layout{};
    layout.eye_w = Round16(cur_.eye_w);
    layout.eye_h = Round16(eye_h);
    layout.eye_y = Round16(kEyeCenterY - eye_h / 2.0f);
    layout.left_eye_x = Round16(kCenterX - kEyeGap / 2.0f - cur_.eye_w);
    layout.right_eye_x = Round16(kCenterX + kEyeGap / 2.0f);
    layout.pupils_visible = pupil_h >= 3.0f;
    layout.pupil_w = Round16(cur_.pupil_w);
    layout.pupil_h = Round16(std::max(pupil_h, 0.0f));
    const float oy = Clamp(cur_.pupil_y, -(eye_h - pupil_h) / 2.0f, (eye_h - pupil_h) / 2.0f);
    layout.pupil_y = Round16(kEyeCenterY - pupil_h / 2.0f + oy);
    layout.right_eye_h = Round16(right_eye_h);
    layout.right_eye_y = Round16(kEyeCenterY - right_eye_h / 2.0f);
    layout.right_pupil_visible = right_pupil_h >= 3.0f;
    layout.right_pupil_h = Round16(std::max(right_pupil_h, 0.0f));
    const float right_oy =
        Clamp(cur_.pupil_y, -(right_eye_h - right_pupil_h) / 2.0f, (right_eye_h - right_pupil_h) / 2.0f);
    layout.right_pupil_y = Round16(kEyeCenterY - right_pupil_h / 2.0f + right_oy);
    {
        // Glow colour in steps of 4 so a slow colour change does not repaint every step.
        auto channel = [](float v) -> uint32_t {
            const long q = std::lround(Clamp(v, 0.0f, 255.0f) / 4.0f) * 4;
            return static_cast<uint32_t>(q > 255 ? 255 : q);
        };
        layout.glow_rgb = (channel(cur_.glow_r) << 16) | (channel(cur_.glow_g) << 8) | channel(cur_.glow_b);
    }
    layout.left_pupil_x = Round16(layout.left_eye_x + (cur_.eye_w - cur_.pupil_w) / 2.0f + cur_.pupil_x);
    layout.right_pupil_x = Round16(layout.right_eye_x + (cur_.eye_w - cur_.pupil_w) / 2.0f + cur_.pupil_x);
    layout.mouth_w = Round16(cur_.mouth_w);
    layout.mouth_h = Round16(cur_.mouth_h);
    layout.mouth_x = Round16(kCenterX - cur_.mouth_w / 2.0f);
    layout.mouth_y = Round16(cur_.mouth_cy - cur_.mouth_h / 2.0f);
    layout.glow_w = static_cast<int16_t>(2 * std::lround(cur_.glow_w / 2.0f));  // steps of 2 px
    layout.glow_opa = Round16(Clamp(cur_.glow_opa, 0.0f, 100.0f));
    layout.face_dx = Round16(face_x_);
    layout.face_dy = Round16(face_y_);
    ApplyLayout(layout);

    UpdateDots(now_ms, shown == State::Thinking && mood == Mood::Normal);

#if CHIPO_DEBUG_TILT_LOG
    if (tilt_source_ && now_ms >= next_debug_log_ms_) {
        next_debug_log_ms_ = now_ms + 1000;
        ESP_LOGI(TAG, "tilt: calibrated=%d angle=%.1f push_x=%.2f lean_z=%.2f lying=%d mood=%d "
                      "gyro=%d rate=%.0f dps spin=%d emotion=%d",
                 tilt.calibrated ? 1 : 0, tilt.angle_deg, tilt.push_x, tilt.lean_z,
                 tilt.lying ? 1 : 0, static_cast<int>(mood), gyro.available ? 1 : 0, gyro.rate_dps,
                 gyro.spinning ? 1 : 0, static_cast<int>(emotion));
    }
#endif
}

void ChipoFace::ApplyLayout(const Layout& l) {
    const bool first = !layout_applied_;
    const Layout& a = applied_;

    if (first || l.eye_w != a.eye_w || l.eye_h != a.eye_h) {
        lv_obj_set_size(left_eye_, l.eye_w, l.eye_h);
    }
    if (first || l.eye_w != a.eye_w || l.right_eye_h != a.right_eye_h) {
        lv_obj_set_size(right_eye_, l.eye_w, l.right_eye_h);
    }
    if (first || l.eye_y != a.eye_y || l.left_eye_x != a.left_eye_x) {
        lv_obj_set_pos(left_eye_, l.left_eye_x, l.eye_y);
    }
    if (first || l.right_eye_y != a.right_eye_y || l.right_eye_x != a.right_eye_x) {
        lv_obj_set_pos(right_eye_, l.right_eye_x, l.right_eye_y);
    }
    if (first || l.pupils_visible != a.pupils_visible) {
        if (l.pupils_visible) lv_obj_remove_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
    }
    if (first || l.right_pupil_visible != a.right_pupil_visible) {
        if (l.right_pupil_visible) lv_obj_remove_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
    }
    if (l.pupils_visible) {
        if (first || l.pupil_w != a.pupil_w || l.pupil_h != a.pupil_h) {
            lv_obj_set_size(left_pupil_, l.pupil_w, l.pupil_h);
        }
        if (first || l.pupil_y != a.pupil_y || l.left_pupil_x != a.left_pupil_x) {
            lv_obj_set_pos(left_pupil_, l.left_pupil_x, l.pupil_y);
        }
    }
    if (l.right_pupil_visible) {
        if (first || l.pupil_w != a.pupil_w || l.right_pupil_h != a.right_pupil_h) {
            lv_obj_set_size(right_pupil_, l.pupil_w, l.right_pupil_h);
        }
        if (first || l.right_pupil_y != a.right_pupil_y || l.right_pupil_x != a.right_pupil_x) {
            lv_obj_set_pos(right_pupil_, l.right_pupil_x, l.right_pupil_y);
        }
    }
    if (first || l.mouth_w != a.mouth_w || l.mouth_h != a.mouth_h) {
        lv_obj_set_size(mouth_, l.mouth_w, l.mouth_h);
    }
    if (first || l.mouth_x != a.mouth_x || l.mouth_y != a.mouth_y) {
        lv_obj_set_pos(mouth_, l.mouth_x, l.mouth_y);
    }
    if (first || l.glow_w != a.glow_w) {
        for (lv_obj_t* o : {left_eye_, right_eye_, mouth_}) {
            lv_obj_set_style_shadow_width(o, l.glow_w, LV_PART_MAIN);
        }
    }
    if (first || l.glow_opa != a.glow_opa) {
        const lv_opa_t opa = static_cast<lv_opa_t>(l.glow_opa * 255 / 100);
        for (lv_obj_t* o : {left_eye_, right_eye_, mouth_}) {
            lv_obj_set_style_shadow_opa(o, opa, LV_PART_MAIN);
        }
    }
    if (first || l.glow_rgb != a.glow_rgb) {
        const lv_color_t color = lv_color_hex(l.glow_rgb);
        for (lv_obj_t* o : {left_eye_, right_eye_, mouth_}) {
            lv_obj_set_style_shadow_color(o, color, LV_PART_MAIN);
        }
        lv_obj_set_style_bg_color(mouth_, color, LV_PART_MAIN);  // the mouth is made of the glow itself
    }
    if (first || l.face_dx != a.face_dx || l.face_dy != a.face_dy) {
        lv_obj_align(root_, LV_ALIGN_CENTER, l.face_dx, l.face_dy);
    }
    applied_ = l;
    layout_applied_ = true;
}

ChipoFace::DebugInfo ChipoFace::Debug() const {
    DebugInfo d{};
    d.shown_state = shown_;
    d.mood = mood_;
    d.face_dx = applied_.face_dx;
    d.face_dy = applied_.face_dy;
    d.fall_dir = fall_dir_;
    d.fell = fell_;
    d.eye_w = applied_.eye_w;
    d.eye_h = applied_.eye_h;
    d.mouth_w = applied_.mouth_w;
    d.mouth_h = applied_.mouth_h;
    d.thinking_dots_visible = dots_visible_;
    d.tilt_calibrated = tilt_.output().calibrated;
    d.tilt_angle_deg = tilt_.output().angle_deg;
    d.emotion = shown_emotion_;
    d.right_eye_h = applied_.right_eye_h;
    d.glow_rgb = applied_.glow_rgb;
    d.gyro_available = gyro_.output().available;
    d.gyro_spinning = gyro_.output().spinning;
    return d;
}
