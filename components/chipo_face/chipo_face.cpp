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

#ifdef ESP_PLATFORM
// The sensor task uses its ChipoImu object, so the object may only be freed once the task has finished. If the
// I2C bus is stuck and the task cannot be stopped, leak the small object on purpose rather than free memory a
// running task still touches (a use-after-free would reboot the device).
void ReleaseImu(std::unique_ptr<ChipoImu>& imu) {
    if (!imu) return;
    if (!imu->Stop()) {
        ESP_LOGE(TAG, "IMU task still running: leaving its object allocated");
        (void)imu.release();
        return;
    }
    imu.reset();
}
#endif

constexpr int32_t kCanvasWidth = 300;
constexpr int32_t kCanvasHeight = 220;
constexpr float kCenterX = kCanvasWidth / 2.0f;
constexpr float kEyeCenterY = 89.0f;
constexpr float kEyeGap = 34.0f;
constexpr float kMouthCenterY = 162.0f;
constexpr int32_t kDotsY = 196;

constexpr uint32_t kStepPeriodMs = 40;  // 25 animation steps per second
// Asleep the face only breathes (a 4 s swing of a couple of pixels): 10 steps per second look the same and cost far
// less CPU and display traffic. It matters most on the charger, where CHIPO stays asleep for good. Must stay <= 100 ms
// (Step() clamps dt to 0.1 s) and short enough that a touch or a jolt still wakes it up promptly.
constexpr uint32_t kSleepStepPeriodMs = 100;
constexpr uint32_t kBlinkMinMs = 2800;
constexpr uint32_t kBlinkJitterMs = 3400;
constexpr float kBlinkCloseMs = 70.0f;
constexpr float kBlinkHoldMs = 45.0f;
constexpr float kBlinkOpenMs = 90.0f;
constexpr uint32_t kSpeakStepMs = 140;
constexpr uint32_t kStartleMs = 800;
constexpr uint32_t kReliefMs = 900;
// Falling off the screen (tilted past CHIPO_FALL_ANGLE_DEG): px/s^2 sideways and downwards, and how far
// the face centre must travel for the whole face (glow included) to be out of the screen.
constexpr float kFallAccelX = 1600.0f;
constexpr float kFallAccelY = 500.0f;
constexpr float kFallMaxDropY = 110.0f;
constexpr float kGoneMargin = 146.0f;  // half the face canvas plus glow; gone_x = screen width / 2 + this x face scale
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
constexpr uint32_t kGlassesFrame = 0xFFCF4A;   // gold aviator frame
constexpr float kGlassesInS = 1.1f;         // sunglasses slide on
constexpr float kGlassesOutS = 0.8f;        // and off
constexpr float kGlassesTravel = 150.0f;    // px above their place when fully off
constexpr float kDriveLookPx = 10.0f;       // how far the eyes look into a curve
constexpr uint32_t kEyeCore = 0xDDF8FF;
constexpr float kAmoledGlowGain = 1.3f;  // halo width gain (px) for the AMOLED panel
constexpr float kAmoledOpaGain = 1.25f;  // and opacity gain (capped at 100 %)
constexpr uint32_t kPupil = 0x2B0F52;       // deep violet iris...
constexpr uint32_t kPupilLight = 0x8A55D8;  // ...lighter towards the bottom
constexpr uint32_t kBrow = 0x9A7FB5;
constexpr uint32_t kBlush = 0xFF8CB4;
constexpr uint32_t kMouthLip = 0xF0828C;
constexpr uint32_t kMouthInside = 0x5E1530;
// "Cute" skin: lilac eyes, pink smile with a red tongue, all on the AMOLED's true black.
constexpr uint32_t kEyeLilac = 0xE2C8FF;
constexpr uint32_t kSmile = 0xFF9DB0;
constexpr uint32_t kTongue = 0xFF2D3A;

// The idle/listening glow is (near) the default cyan: then the cute skin shows its own lilac and pink.
bool IsDefaultGlow(uint32_t rgb) {
    auto near = [](uint32_t a, uint32_t b) { return (a > b ? a - b : b - a) <= 40; };
    return near((rgb >> 16) & 0xFF, (kGlowBlue >> 16) & 0xFF) && near((rgb >> 8) & 0xFF, (kGlowBlue >> 8) & 0xFF) &&
           near(rgb & 0xFF, kGlowBlue & 0xFF);
}

// Mouth shapes: the half-moon smile only fits happy faces; sad ones get it upside down.
enum MouthKind : uint8_t { kMouthSmile = 0, kMouthFrown = 1, kMouthO = 2, kMouthFlat = 3, kMouthGrin = 4 };
constexpr uint32_t kRingCyan = 0x5CF0FF;   // ring + halo while listening / speaking (calm colour)
constexpr uint32_t kHappyArc = 0xFFD2B0;   // closed happy eyes (peach)
constexpr uint32_t kSleepArc = 0xC9B0FF;   // closed sleeping eyes (lilac)

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
    lv_obj_set_style_bg_grad_color(object, lv_color_hex(kPupilLight), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(object, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_main_stop(object, 60, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_stop(object, 255, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(object, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    MakeInert(object);
}

int16_t Round16(float v) { return static_cast<int16_t>(std::lround(v)); }

// Hue 0..1 (full turn), saturation and value 0..1 -> 0xRRGGBB.
uint32_t HsvToRgb(float h, float sat, float val) {
    h = h - std::floor(h);
    const float k = h * 6.0f;
    const int i = static_cast<int>(k);
    const float f = k - static_cast<float>(i);
    const float p = val * (1.0f - sat), q = val * (1.0f - sat * f), t = val * (1.0f - sat * (1.0f - f));
    float r = val, g = t, b = p;
    switch (i % 6) {
    case 0: r = val; g = t; b = p; break;
    case 1: r = q; g = val; b = p; break;
    case 2: r = p; g = val; b = t; break;
    case 3: r = p; g = q; b = val; break;
    case 4: r = t; g = p; b = val; break;
    default: r = val; g = p; b = q; break;
    }
    return (static_cast<uint32_t>(r * 255.0f) << 16) | (static_cast<uint32_t>(g * 255.0f) << 8) | static_cast<uint32_t>(b * 255.0f);
}
}  // namespace

ChipoFace::ChipoFace()
    : tilt_(CHIPO_TILT_MIRROR_X != 0),
      fx_([](int lo, int hi) { return static_cast<int>(lv_rand(static_cast<uint32_t>(lo), static_cast<uint32_t>(hi))); }) {
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

    {
        // Sizes are in design pixels (scale 1); the face is scaled to the panel (never smaller than 1,
        // never so big that the canvas would not fit the height).
        int32_t pw = lv_obj_get_width(parent), ph = lv_obj_get_height(parent);
        if (pw <= 0) pw = 480;
        if (ph <= 0) ph = 480;
        // The eyes and mouth (about 158 px wide at scale 1) fill ~58 % of the screen width: 1.76 on the
        // 2.16" (480 px). The 300 x 220 canvas must still fit 85 % of the height.
        fs_ = std::min(static_cast<float>(pw) * 0.58f / 158.0f, static_cast<float>(ph) * 0.85f / 220.0f);
        fs_ = Clamp(fs_, 1.0f, 2.0f);
    }
    // The face slides off the screen when CHIPO falls over: without this the parent (the screen) grows a grey
    // scrollbar on its edge while the face is out of bounds.
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
    // The face lives on a true-black AMOLED canvas. Cover XiaoZhi's light page behind
    // the eyes, while keeping the road effects and face themselves above this backdrop.
    backdrop_ = lv_obj_create(parent);
    if (backdrop_ == nullptr) return false;
    lv_obj_remove_style_all(backdrop_);
    lv_obj_set_size(backdrop_, pw > 0 ? pw : 480, ph > 0 ? ph : 480);
    lv_obj_set_pos(backdrop_, 0, 0);
    lv_obj_set_style_bg_color(backdrop_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(backdrop_, LV_OPA_COVER, LV_PART_MAIN);
    MakeInert(backdrop_);
    lv_obj_move_foreground(backdrop_);

    CreateRoad(parent);  // above the black backdrop, below the face

    root_ = lv_obj_create(parent);
    if (root_ == nullptr) return false;

    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, S(kCanvasWidth), S(kCanvasHeight));
    lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, LV_PART_MAIN);
    MakeInert(root_);

    left_eye_ = lv_obj_create(root_);
    right_eye_ = lv_obj_create(root_);
    left_pupil_ = lv_obj_create(root_);
    right_pupil_ = lv_obj_create(root_);
    mouth_ = lv_obj_create(root_);
    for (auto& dot : dots_) dot = lv_obj_create(root_);
    for (auto& piece : glasses_) piece = lv_obj_create(root_);

    bool ok = left_eye_ && right_eye_ && left_pupil_ && right_pupil_ && mouth_;
    for (auto* dot : dots_) ok = ok && dot != nullptr;
    for (auto* piece : glasses_) ok = ok && piece != nullptr;
    if (!ok) {
        Stop();
        ESP_LOGE(TAG, "Could not create face objects");
        return false;
    }

    // No LVGL shadow on the eyes: software box shadows can stall the renderer for seconds on some sizes
    // (seen at 480 x 480). The glow is drawn by the flat halo discs behind the eyes instead.
    StyleGlow(left_eye_, kEyeCore, 0);
    StyleGlow(right_eye_, kEyeCore, 0);
    StylePupil(left_pupil_);
    StylePupil(right_pupil_);
    // Two specular highlights on every pupil (a big and a small one) make the eyes look wet and deep.
    // They are children of the pupil: they move with it and are clipped when it shrinks.
    for (lv_obj_t* pupil : {left_pupil_, right_pupil_}) {
        for (int k = 0; k < 2; ++k) {
            lv_obj_t* spark = lv_obj_create(pupil);
            if (!spark) continue;
            lv_obj_remove_style_all(spark);
            lv_obj_set_size(spark, S(k == 0 ? 11 : 5), S(k == 0 ? 11 : 5));
            lv_obj_set_style_bg_color(spark, lv_color_white(), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(spark, k == 0 ? LV_OPA_COVER : LV_OPA_70, LV_PART_MAIN);
            lv_obj_set_style_radius(spark, LV_RADIUS_CIRCLE, LV_PART_MAIN);
            lv_obj_align(spark, k == 0 ? LV_ALIGN_TOP_LEFT : LV_ALIGN_BOTTOM_RIGHT, S(k == 0 ? 4 : -4), S(k == 0 ? 5 : -5));
            MakeInert(spark);
        }
    }
    StyleGlow(mouth_, kSmile, 20);
    // The mouth is a clipping window: a half-moon smile (bottom half of an ellipse) with a red tongue.
    lv_obj_set_style_bg_opa(mouth_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(mouth_, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(mouth_, 0, LV_PART_MAIN);
    bowl_ = lv_obj_create(mouth_);
    if (bowl_) {
        lv_obj_remove_style_all(bowl_);
        lv_obj_set_style_bg_color(bowl_, lv_color_hex(kMouthLip), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bowl_, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(bowl_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        MakeInert(bowl_);
    }
    inner_ = lv_obj_create(mouth_);
    if (inner_) {
        lv_obj_remove_style_all(inner_);
        lv_obj_set_style_bg_color(inner_, lv_color_hex(kMouthInside), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(inner_, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(inner_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        MakeInert(inner_);
        lv_obj_add_flag(inner_, LV_OBJ_FLAG_HIDDEN);
    }
    tongue_ = lv_obj_create(mouth_);
    if (tongue_) {
        lv_obj_remove_style_all(tongue_);
        lv_obj_set_style_bg_color(tongue_, lv_color_hex(0xFF7A88), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(tongue_, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(tongue_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        MakeInert(tongue_);
        lv_obj_add_flag(tongue_, LV_OBJ_FLAG_HIDDEN);
    }
    for (auto& blush : blush_) {
        blush = lv_obj_create(root_);
        if (!blush) continue;
        lv_obj_remove_style_all(blush);
        lv_obj_set_size(blush, S(40), S(22));
        lv_obj_set_style_bg_color(blush, lv_color_hex(kBlush), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(blush, LV_OPA_80, LV_PART_MAIN);  // translucent pink, one soft shape
        lv_obj_set_style_radius(blush, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        MakeInert(blush);
        lv_obj_move_background(blush);
    }
    for (auto& brow : brow_) {
        brow = lv_obj_create(root_);
        if (!brow) continue;
        lv_obj_remove_style_all(brow);
        lv_obj_set_size(brow, S(30), S(5));
        lv_obj_set_style_bg_color(brow, lv_color_hex(kBrow), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(brow, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(brow, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_x(brow, S(15), LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(brow, S(2), LV_PART_MAIN);
        MakeInert(brow);
        lv_obj_add_flag(brow, LV_OBJ_FLAG_HIDDEN);
    }
    for (auto& halo : halo_) {  // [0..1] outer, [2..3] inner, per eye
        halo = lv_obj_create(root_);
        if (!halo) continue;
        lv_obj_remove_style_all(halo);
        lv_obj_set_style_bg_opa(halo, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_radius(halo, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        MakeInert(halo);
        lv_obj_move_background(halo);  // behind the eyes
        lv_obj_add_flag(halo, LV_OBJ_FLAG_HIDDEN);
    }
    for (auto& arc : eye_arc_) {
        arc = lv_arc_create(root_);
        if (!arc) continue;
        lv_obj_remove_style_all(arc);
        lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, S(7), LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_arc_set_rotation(arc, 0);
        lv_arc_set_bg_angles(arc, 0, 360);
        MakeInert(arc);
        lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < 3; ++i) {
        StyleGlow(dots_[i], kEyeLilac, 0);
        lv_obj_set_size(dots_[i], S(8), S(8));
        lv_obj_set_pos(dots_[i], S(kCenterX) + S((i - 1) * 22 - 4), S(kDotsY));
        lv_obj_add_flag(dots_[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Aviator sunglasses (hidden until the car mode starts): mirrored lenses (dark blue at the top
    // fading to a sunset orange), gold frame, brow bar and a glint.
    for (int i = 0; i < kGlassesPieces; ++i) {
        lv_obj_t* piece = glasses_[i];
        lv_obj_remove_style_all(piece);
        lv_obj_set_style_border_width(piece, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(piece, 0, LV_PART_MAIN);
        if (i < 2) {  // lenses
            lv_obj_set_style_bg_color(piece, lv_color_hex(0x0B1730), LV_PART_MAIN);
            lv_obj_set_style_bg_grad_color(piece, lv_color_hex(0xFF6A1F), LV_PART_MAIN);
            lv_obj_set_style_bg_grad_dir(piece, LV_GRAD_DIR_VER, LV_PART_MAIN);
            lv_obj_set_style_bg_main_stop(piece, 90, LV_PART_MAIN);
            lv_obj_set_style_bg_grad_stop(piece, 255, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(piece, LV_OPA_90, LV_PART_MAIN);
            lv_obj_set_style_border_color(piece, lv_color_hex(kGlassesFrame), LV_PART_MAIN);
            lv_obj_set_style_border_width(piece, 4, LV_PART_MAIN);
            lv_obj_set_style_radius(piece, 40, LV_PART_MAIN);
        } else if (i < 6) {  // bridge, brow bar, arms
            lv_obj_set_style_bg_color(piece, lv_color_hex(kGlassesFrame), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(piece, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_radius(piece, 3, LV_PART_MAIN);
        } else {  // glints
            lv_obj_set_style_bg_color(piece, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(piece, LV_OPA_50, LV_PART_MAIN);
            lv_obj_set_style_radius(piece, 3, LV_PART_MAIN);
        }
        MakeInert(piece);
        lv_obj_add_flag(piece, LV_OBJ_FLAG_HIDDEN);
    }
    drive_mode_ = false;
    side_lying_ = desk_clock_ = false;
    side_lying_ms_ = 0;
    drive_lean_ = drive_lean_cur_ = glasses_t_ = glasses_jolt_ = 0;
    glasses_shown_ = false;
    for (int& v : glasses_applied_) v = -9999;
    road_speed_ = road_boost_ = road_jolt_ = wheel_angle_ = 0;
    road_phase_ = streak_phase_ = 0;
    road_shown_ = false;
    sun_opa_applied_ = -1;
    for (auto& piece : road_applied_) piece = RoadPiece{};
    wheel_rot_applied_ = wheel_y_applied_ = -9999;
    wheel_opa_applied_ = -1;

    {
        int32_t pw = lv_obj_get_width(parent), ph = lv_obj_get_height(parent);
        fx_w_ = pw > 0 ? pw : 480;
        fx_h_ = ph > 0 ? ph : 480;
        fx_.Clear();
        fx_view_.Start(parent, fx_w_, fx_h_);  // above the face; without it the decorations just do not show
    }
    CreateCostumes();
    tears_until_ms_ = tantrum_until_ms_ = next_zzz_ms_ = next_date_check_ms_ = 0;
    dance_on_ = false;
    dance_level_ = dance_avg_ = 0;
    dance_hops_ = 0;
    costume_ = Costume::None;
    costume_manual_ = false;
    gaze_on_ = false;
    gaze_until_ms_ = 0;
    gaze_px_x_ = gaze_px_y_ = gaze_lean_ = 0;
    yawn_index_ = 0xFFFFFFFFu;
    stretches_ = slow_blinks_ = 0;
    slow_blink_pending_ = blink_slow_ = false;
    next_slow_blink_ms_ = 0;

    const uint32_t now = lv_tick_get();
    cur_ = Pose{62, 70, 30, 38, 0, 0, 56, 24, kMouthCenterY, 20, 60};
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
    motion_period_ms_ = kStepPeriodMs;
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
    ReleaseImu(imu_);
#endif
    fx_view_.Stop();
    fx_.Clear();
    if (root_) lv_obj_delete(root_);
    if (road_) lv_obj_delete(road_);
    if (backdrop_) lv_obj_delete(backdrop_);
    backdrop_ = nullptr;
    road_ = wheel_ = sun_ = sun_mask_ = horizon_ = nullptr;
    for (auto& o : lane_) o = nullptr;
    for (auto& o : streak_) o = nullptr;
    road_shown_ = false;
    root_ = left_eye_ = right_eye_ = left_pupil_ = right_pupil_ = mouth_ = tongue_ = bowl_ = inner_ = nullptr;
    eye_arc_[0] = eye_arc_[1] = nullptr;
    for (auto& o : halo_) o = nullptr;
    for (auto& o : brow_) o = nullptr;
    for (auto& o : blush_) o = nullptr;
    for (auto& dot : dots_) dot = nullptr;
    for (auto& piece : glasses_) piece = nullptr;
    for (auto& piece : hat_) piece = nullptr;
    for (auto& piece : moustache_) piece = nullptr;
    hat_shown_ = moustache_shown_ = false;
    dots_visible_ = false;
    for (int& o : dots_opa_) o = -1;
}

void ChipoFace::SetTiltSource(TiltSource source) {
#ifdef ESP_PLATFORM
    tilt_source_ = nullptr;  // it holds a raw pointer to the sensor object released below
    ReleaseImu(imu_);
#endif
    tilt_source_ = std::move(source);
}

void ChipoFace::RecalibrateTilt() {
    tilt_.Reset();
    gyro_.Reset();
    calibrated_logged_ = false;
    fell_ = false;
    fall_dir_ = 0;
    side_lying_ = false;
    desk_clock_ = false;
    side_lying_ms_ = 0;
    startle_until_ms_ = 0;
    relief_until_ms_ = 0;
    dizzy_until_ms_ = 0;
    ESP_LOGI(TAG, "Tilt neutral pose cleared; hold CHIPO upright and still to recalibrate");
}

void ChipoFace::SetDriveMode(bool on) {
    if (on == drive_mode_) return;
    drive_mode_ = on;
    if (on) {
        idle_since_ms_ = last_step_ms_;  // wakes CHIPO up
        fell_ = false;
        fall_dir_ = 0;
    } else {
        drive_lean_ = drive_lean_cur_ = 0;
    }
}

void ChipoFace::DriveReact(DriveReaction reaction, float intensity) {
    if (!drive_mode_ || !root_) return;
    const uint32_t now = last_step_ms_;
    const float k = intensity < 0.2f ? 0.2f : (intensity > 1.0f ? 1.0f : intensity);
    idle_since_ms_ = now;
    switch (reaction) {
        case DriveReaction::Accelerate:
            StartEmotion(Emotion::Happy, now, 1300);
            vel_y_ += 90.0f + 90.0f * k;   // pushed back into the seat
            glasses_jolt_ = -0.4f * k;
            road_boost_ += 0.5f * k;   // the scenery streams by faster
            break;
        case DriveReaction::Brake:
            StartEmotion(Emotion::Surprised, now, 900);
            vel_y_ -= 120.0f + 120.0f * k;  // thrown forward
            glasses_jolt_ = 1.0f * k;       // glasses slide down the nose
            road_boost_ -= 0.45f * k;       // and the scenery slows
            break;
        case DriveReaction::Bump:
            vel_y_ -= 160.0f + 180.0f * k;  // bounce
            glasses_jolt_ = -0.8f * k;
            road_jolt_ = k;                 // the road jumps
            break;
        case DriveReaction::Abrupt:
            if (k > 0.75f) dizzy_until_ms_ = now + 1100;
            else StartEmotion(Emotion::Surprised, now, 1100);
            vel_y_ -= 200.0f;
            glasses_jolt_ = 1.0f;
            road_jolt_ = 1.0f;
            break;
    }
}

void ChipoFace::SetFastSampling(bool fast) {
#ifdef ESP_PLATFORM
    if (imu_) imu_->SetFastSampling(fast);
#else
    (void)fast;
#endif
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
    if (dance_on_) return Emotion::Dance;
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
        blink_slow_ = slow_blink_pending_;  // the affectionate blink: slow, held, slow
        slow_blink_pending_ = false;
        if (blink_slow_) ++slow_blinks_;
    }
    const float close_ms = blink_slow_ ? 380.0f : kBlinkCloseMs;
    const float hold_ms = blink_slow_ ? 320.0f : kBlinkHoldMs;
    const float open_ms = blink_slow_ ? 460.0f : kBlinkOpenMs;
    const float elapsed = static_cast<float>(now_ms - blink_t0_);
    if (blink_phase_ == 1) {
        if (elapsed < close_ms) return elapsed / close_ms;
        blink_phase_ = 2;
        blink_t0_ = now_ms;
        return 1.0f;
    }
    if (blink_phase_ == 2) {
        if (elapsed < hold_ms) return 1.0f;
        blink_phase_ = 3;
        blink_t0_ = now_ms;
        return 1.0f;
    }
    if (elapsed < open_ms) return 1.0f - elapsed / open_ms;
    blink_phase_ = 0;
    if (blink_slow_) {
        blink_slow_ = false;
        blink_was_double_ = false;
        next_blink_ms_ = now_ms + kBlinkMinMs + lv_rand(0, kBlinkJitterMs);
        return 0.0f;
    }
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
    Pose p{62, 70, 30, 38, 0, 0, 56, 24, kMouthCenterY, 20, 60};
    const float t = static_cast<float>(now_ms);

    switch (shown) {
    case State::Idle:
        p.pupil_x = look_x_;
        p.pupil_y = look_y_;
        break;
    case State::Listening:
        p.eye_w = 66; p.eye_h = 78;
        p.mouth_w = 36; p.mouth_h = 22; p.mouth_cy = 163;  // smiling, tongue out: all ears
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
        p.mouth_w = 44;  // wide enough to read as an open, talking mouth
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
        p.mouth_w = 34; p.mouth_h = 11; p.mouth_cy = 168;  // frown
        p.glow_r = 255; p.glow_g = 100; p.glow_b = 90;    // red ring: something went wrong
        break;
    case State::Sleep:
        p.eye_h = 4; p.pupil_h = 2;
        p.mouth_w = 16; p.mouth_h = 5 + 2.5f * (0.5f + 0.5f * std::sin(kTwoPi * t / 4000.0f)); p.mouth_cy = 164;  // breathing
        p.glow_w = 10;
        p.glow_opa = 28 + 10 * std::sin(kTwoPi * t / 4000.0f);  // slow breathing
        break;
    }

    if (emotion != Emotion::None) ApplyEmotion(p, emotion, shown, now_ms);

    // A finger on the screen: the eyes follow it (not while they spin or swing on purpose).
    if (gaze_on_ && mood == Mood::Normal && emotion != Emotion::Whee && emotion != Emotion::No &&
        (shown == State::Idle || shown == State::Listening || shown == State::Speaking ||
         shown == State::Connecting)) {
        p.pupil_x = gaze_px_x_;
        p.pupil_y = gaze_px_y_;
    }

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
    // The owner's eye colour only replaces the default blue, never an emotion's own colour.
    if (has_tint_ && std::fabs(p.glow_r - 117.0f) < 24.0f && std::fabs(p.glow_g - 223.0f) < 24.0f &&
        std::fabs(p.glow_b - 255.0f) < 24.0f) {
        p.glow_r = static_cast<float>((tint_rgb_ >> 16) & 0xFF);
        p.glow_g = static_cast<float>((tint_rgb_ >> 8) & 0xFF);
        p.glow_b = static_cast<float>(tint_rgb_ & 0xFF);
    }
    return p;
}

void ChipoFace::SetEyeTint(uint32_t rgb) {
    has_tint_ = true;
    tint_rgb_ = rgb & 0xFFFFFF;
}

void ChipoFace::ClearEyeTint() { has_tint_ = false; }

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
        mouth(34, 12, 172);  // frown
        glow(0x8FA6FF, 14, 38);
        break;
    case Emotion::Tired: {
        // A yawn every 5 s: the eyes squeeze shut while the mouth opens wide.
        const float cycle = std::fmod(t - static_cast<float>(tired_t0_), 5200.0f) / 5200.0f;
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
        p.pupil_w = 20; p.pupil_h = 22; p.pupil_x = 0; p.pupil_y = 0;  // small pupils in wide eyes
        mouth(26, 30, 169);
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
        mouth(30, 10, 168);  // frown
        break;
    case Emotion::Grumpy:
        p.eye_w = 60; p.eye_h = 34;
        p.pupil_w = 16; p.pupil_h = 16; p.pupil_x = 0; p.pupil_y = 4;
        mouth(38, 10, 170);  // frown
        glow(0xFF5B4A, 20, 70);
        break;
    case Emotion::Dance: {
        const float lv = dance_level_;
        p.eye_w = 66; p.eye_h = 36 + 14 * lv; p.pupil_h = 0;
        mouth(52, 10 + 26 * lv, 164);
        glow(HsvToRgb(t / 3000.0f, 0.55f, 1.0f), 20 + 16 * lv, 68 + 28 * lv);
        break;
    }
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
    bool nodded = false, shook = false, spin_ended = false, startle_seen = false;
    uint32_t spin_len_ms = 0;
    if (tilt_source_) {
        ImuSample sample;
        for (int guard = 0; guard < 8 && tilt_source_(&sample); ++guard) {
            const uint32_t t = sample.t_ms != 0 ? sample.t_ms : now_ms;
            if (sample_observer_) sample_observer_(sample, t);
            if (extra_sample_observer_) extra_sample_observer_(sample, t);
            tilt_.Update(sample, t);
            startle_seen = startle_seen || tilt_.output().startle;  // a later sample would clear it
            if (sample.has_gyro) {
                const GyroOutput& g = gyro_.Update(sample, tilt_.output().calibrated,
                                                   tilt_.vertical_axis(), t);
                nodded = nodded || g.nod;
                shook = shook || g.shake;
                if (g.spin_ended) {
                    spin_ended = true;
                    spin_len_ms = g.spin_ms;
                }
            }
        }
    }
    TiltOutput tilt = tilt_.output();
    tilt.startle = tilt.startle || startle_seen;
    GyroOutput gyro = gyro_.output();
    if (drive_mode_) {
        // In the car the road would set off every reaction below: only the lean of the curves is used.
        Ease(drive_lean_cur_, drive_lean_, dt, 0.12f);
        nodded = shook = spin_ended = false;
        gyro.spinning = false;
        tilt.startle = false;
        tilt.lying = false;
        tilt.calibrated = true;
        tilt.angle_deg = 0;
        tilt.push_x = Clamp(drive_lean_cur_ * 0.28f, -1.0f, 1.0f);
        tilt.lean_z = 0;
    }
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
    // Desk clock: lying on the configured side (not in the car). Lying for CHIPO_DESK_CLOCK_AFTER_MS turns it into
    // a clock; the face stays off the screen meanwhile (no dizziness: that comes when it is stood up again).
    {
        const bool side_ok = CHIPO_DESK_CLOCK_SIDE != 0 && tilt.calibrated && !drive_mode_;
        const float pull = tilt.push_x * static_cast<float>(CHIPO_DESK_CLOCK_SIDE);  // > 0: gravity towards that side
        if (side_ok && tilt.angle_deg >= 70.0f && pull > 0.75f) side_lying_ = true;
        else if (!side_ok || tilt.angle_deg < 50.0f || pull < 0.45f) side_lying_ = false;
        if (side_lying_) {
            side_lying_ms_ += static_cast<uint32_t>(dt * 1000.0f);
            idle_since_ms_ = now_ms;
        } else {
            side_lying_ms_ = 0;
        }
        const bool want_clock = side_lying_ && side_lying_ms_ >= CHIPO_DESK_CLOCK_AFTER_MS;
        if (want_clock != desk_clock_) {
            desk_clock_ = want_clock;
            if (!desk_clock_) {  // stood up again: "uy, me mareé"
                dizzy_until_ms_ = now_ms + CHIPO_DESK_CLOCK_DIZZY_MS;
                tilt.lying = false;
            }
            if (desk_clock_observer_) desk_clock_observer_(desk_clock_, CHIPO_DESK_CLOCK_SIDE);
        }
    }
    // Which side does it slip towards? The side gravity pulls (latched once there is a clear pull).
    if (!fell_) fall_dir_ = 0;
    else if (fall_dir_ == 0 && std::fabs(tilt.push_x) >= kFallMinPush) fall_dir_ = tilt.push_x > 0 ? 1 : -1;
    if (fell_) idle_since_ms_ = now_ms;

    // ---- state and mood --------------------------------------------------------------------
    if (thinking_ && now_ms - thinking_since_ms_ > CHIPO_THINKING_TIMEOUT_MS) thinking_ = false;
    const State shown = ShownState(now_ms);
    shown_ = shown;
    {
        const uint32_t want = shown == State::Sleep ? kSleepStepPeriodMs : kStepPeriodMs;
        if (motion_timer_ != nullptr && motion_period_ms_ != want) {
            lv_timer_set_period(motion_timer_, want);
            motion_period_ms_ = want;
        }
    }

    Mood mood = Mood::Normal;
    if (state_ != State::Error) {
        if ((tilt.lying && !side_lying_) || now_ms < dizzy_until_ms_) mood = Mood::Dizzy;
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
    if (emotion == Emotion::Tired && shown_emotion_ != Emotion::Tired) {
        tired_t0_ = now_ms;  // the yawn starts right away
        yawn_index_ = 0xFFFFFFFFu;
    }
    shown_emotion_ = emotion;
    // Yawn and stretch: at every yawn the face rises a little, like arms going up.
    if (emotion == Emotion::Tired) {
        const uint32_t idx = (now_ms - tired_t0_) / 5200u;
        if (idx != yawn_index_) {
            yawn_index_ = idx;
            vel_y_ -= 150.0f;
            ++stretches_;
        }
    }
    // Sore loser: angry shaking, then tears.
    if (tantrum_until_ms_ != 0) {
        if (now_ms < tantrum_until_ms_) {
            if (now_ms >= next_kick_ms_) {
                vel_x_ += static_cast<float>(kick_dir_) * 240.0f;
                kick_dir_ = -kick_dir_;
                next_kick_ms_ = now_ms + 200;
            }
        } else {
            tantrum_until_ms_ = 0;
            Cry(3000);
        }
    }
    // The finger: gaze target eased, face leans a little towards it. Lets go ~0.5 s after the last touch move.
    gaze_on_ = now_ms < gaze_until_ms_;
    {
        const float tgt_x = gaze_on_ ? gaze_nx_ * 14.0f : 0.0f;
        const float tgt_y = gaze_on_ ? gaze_ny_ * 9.0f : 0.0f;
        Ease(gaze_px_x_, tgt_x, dt, 0.06f);
        Ease(gaze_px_y_, tgt_y, dt, 0.06f);
        Ease(gaze_lean_, gaze_on_ ? gaze_nx_ * 9.0f : 0.0f, dt, 0.12f);
    }
    // Dance: loudness drives the mouth; peaks make the face hop to the beat. Awake and for 3 minutes at most.
    if (dance_on_) {
        if (now_ms >= dance_until_ms_) {
            dance_on_ = false;
        } else {
            idle_since_ms_ = now_ms;
            const float raw = level_source_ ? Clamp(level_source_(), 0.0f, 1.0f) : 0.0f;
            Ease(dance_level_, raw, dt, raw > dance_level_ ? 0.04f : 0.18f);
            const bool onset = raw > 0.16f && raw > dance_avg_ * 1.35f;
            Ease(dance_avg_, raw, dt, 0.5f);
            if (onset && now_ms >= next_hop_ms_ && mood == Mood::Normal) {
                vel_y_ -= 130.0f + 140.0f * raw;
                vel_x_ += static_cast<float>(hop_dir_) * 150.0f;
                hop_dir_ = -hop_dir_;
                next_hop_ms_ = now_ms + 240;
                ++dance_hops_;
            }
        }
    } else {
        dance_level_ = dance_avg_ = 0;
    }
    // A slow loving blink now and then (idle only).
    if (shown == State::Idle && mood == Mood::Normal && !emotion_running && idle_quirks_) {
        if (next_slow_blink_ms_ == 0) next_slow_blink_ms_ = now_ms + 40000 + lv_rand(0, 40000);
        else if (now_ms >= next_slow_blink_ms_ && blink_phase_ == 0) {
            SlowBlink();
            next_slow_blink_ms_ = now_ms + 50000 + lv_rand(0, 60000);
        }
    }

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

    if (drive_mode_ && (shown == State::Idle || shown == State::Listening || shown == State::Speaking)) {
        look_x_ = -drive_lean_cur_ * kDriveLookPx;  // the eyes look into the turn
        look_y_ = 0;
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
    const bool falling = fell_ && fall_dir_ != 0 && (!tilt.lying || side_lying_);
    float tx = 0, ty = 0;
    if (tilt.calibrated && !tilt.lying) {
        tx = Clamp(tilt.push_x * 170.0f, -64.0f, 64.0f);
        ty = Clamp(tilt.lean_z * 46.0f, 0.0f, 46.0f);
        if (mood == Mood::Startled) tx = Clamp(tx * 1.2f, -72.0f, 72.0f);
    }
    if (dance_on_ && !falling && mood == Mood::Normal) {  // gentle sway and bounce even without a beat
        const float tt = static_cast<float>(now_ms - dance_t0_);
        tx += 20.0f * std::sin(kTwoPi * tt / 1800.0f);
        ty -= 5.0f * (0.5f + 0.5f * std::sin(kTwoPi * tt / 900.0f)) + 14.0f * dance_level_;
    }
    if (shown == State::Sleep && !falling) ty += 2.5f * std::sin(kTwoPi * static_cast<float>(now_ms) / 4000.0f);  // soft breathing
    if (!falling && gaze_on_ && !tilt.lying) tx += gaze_lean_;
    const bool flying_in = !falling && std::fabs(face_x_) > kNormalRangeX;
    if (falling) {
        vel_x_ += static_cast<float>(fall_dir_) * kFallAccelX * dt;
        vel_y_ += kFallAccelY * dt;
        face_x_ += vel_x_ * dt;
        face_y_ += vel_y_ * dt;
        if (std::fabs(face_x_) >= (static_cast<float>(fx_w_) * 0.5f + kGoneMargin * fs_)) {  // gone: park it out of sight
            face_x_ = static_cast<float>(fall_dir_) * (static_cast<float>(fx_w_) * 0.5f + kGoneMargin * fs_);
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
    layout.eye_w = Round16(cur_.eye_w * fs_);
    layout.eye_h = Round16(eye_h * fs_);
    layout.eye_y = Round16((kEyeCenterY - eye_h / 2.0f) * fs_);
    layout.left_eye_x = Round16((kCenterX - kEyeGap / 2.0f - cur_.eye_w) * fs_);
    layout.right_eye_x = Round16((kCenterX + kEyeGap / 2.0f) * fs_);
    layout.pupils_visible = pupil_h >= 3.0f;
    layout.pupil_w = Round16(cur_.pupil_w * fs_);
    layout.pupil_h = Round16(std::max(pupil_h, 0.0f) * fs_);
    const float oy = Clamp(cur_.pupil_y, -(eye_h - pupil_h) / 2.0f, (eye_h - pupil_h) / 2.0f);
    layout.pupil_y = Round16((kEyeCenterY - pupil_h / 2.0f + oy) * fs_);
    layout.right_eye_h = Round16(right_eye_h * fs_);
    layout.right_eye_y = Round16((kEyeCenterY - right_eye_h / 2.0f) * fs_);
    layout.right_pupil_visible = right_pupil_h >= 3.0f;
    layout.right_pupil_h = Round16(std::max(right_pupil_h, 0.0f) * fs_);
    const float right_oy =
        Clamp(cur_.pupil_y, -(right_eye_h - right_pupil_h) / 2.0f, (right_eye_h - right_pupil_h) / 2.0f);
    layout.right_pupil_y = Round16((kEyeCenterY - right_pupil_h / 2.0f + right_oy) * fs_);
    {
        // Glow colour in steps of 4 so a slow colour change does not repaint every step.
        auto channel = [](float v) -> uint32_t {
            const long q = std::lround(Clamp(v, 0.0f, 255.0f) / 4.0f) * 4;
            return static_cast<uint32_t>(q > 255 ? 255 : q);
        };
        layout.glow_rgb = (channel(cur_.glow_r) << 16) | (channel(cur_.glow_g) << 8) | channel(cur_.glow_b);
    }
    layout.left_pupil_x = Round16(layout.left_eye_x + ((cur_.eye_w - cur_.pupil_w) / 2.0f + cur_.pupil_x) * fs_);
    layout.right_pupil_x = Round16(layout.right_eye_x + ((cur_.eye_w - cur_.pupil_w) / 2.0f + cur_.pupil_x) * fs_);
    layout.mouth_w = Round16(cur_.mouth_w * fs_);
    layout.mouth_h = Round16(cur_.mouth_h * fs_);
    layout.mouth_x = Round16((kCenterX - cur_.mouth_w / 2.0f) * fs_);
    layout.mouth_y = Round16((cur_.mouth_cy - cur_.mouth_h / 2.0f) * fs_);
    // AMOLED: black pixels are off, so a wider and brighter halo reads as light, not as a grey haze.
    layout.glow_w = static_cast<int16_t>(2 * std::lround(Clamp(cur_.glow_w * kAmoledGlowGain, 0.0f, 60.0f) / 2.0f));  // steps of 2 px
    layout.glow_opa = Round16(Clamp(cur_.glow_opa * kAmoledOpaGain, 0.0f, 100.0f));
    {
        // Eyebrows (thin bars): a gentle arch when calm, worried when sad, angry when grumpy, raised when surprised.
        // They only show where they add something; the squinting, kissing and sleeping faces have none.
        bool brows = false;
        int rot = 60, lift = 0;
        switch (emotion) {
        case Emotion::None:
            brows = shown == State::Idle || shown == State::Thinking || shown == State::Error || shown == State::Connecting;
            if (shown == State::Thinking) { rot = 90; lift = 3; }
            if (shown == State::Error) rot = 140;
            break;
        case Emotion::Sad: brows = true; rot = 150; break;
        case Emotion::Grumpy: brows = true; rot = -150; break;
        case Emotion::Suspicious: brows = true; rot = -90; break;
        case Emotion::Surprised: brows = true; rot = 0; lift = 9; break;
        default: break;
        }
        if (blink > 0.5f) brows = brows && false;
        layout.brows_visible = brows;
        uint8_t mk = kMouthSmile;
        switch (emotion) {
        case Emotion::Sad: case Emotion::Grumpy: case Emotion::No: mk = kMouthFrown; break;
        case Emotion::Surprised: case Emotion::Whee: case Emotion::Tired: mk = kMouthO; break;
        case Emotion::Suspicious: mk = kMouthFlat; break;
        case Emotion::None:
            if (shown == State::Error) mk = kMouthFrown;
            else if (shown == State::Sleep) mk = kMouthO;
            else if (shown == State::Thinking || shown == State::Connecting) mk = kMouthFlat;
            break;
        default: break;
        }
        if (mood == Mood::Startled || mood == Mood::Dizzy) mk = kMouthO;  // "oh!"
        if (drive_mode_ && mk == kMouthSmile) mk = kMouthGrin;  // cool driver: closed grin, no tongue over the sun
        layout.mouth_kind = mk;
        uint8_t arc = 0;
        if (emotion == Emotion::Happy || emotion == Emotion::Laugh) arc = 1;
        else if (emotion == Emotion::None && shown == State::Sleep) arc = 2;
        layout.eye_arc = arc;
        // A glowing ring round the eyes tells "I'm listening / talking / something happened";
        // calm faces (idle, thinking, asleep, sleepy) keep plain lilac eyes.
        layout.ring = mood != Mood::Normal ||
                      (emotion == Emotion::None
                           ? (shown == State::Listening || shown == State::Speaking || shown == State::Error)
                           : emotion != Emotion::Tired);
        if (arc) layout.pupils_visible = layout.right_pupil_visible = false;
        if (arc) layout.brows_visible = false;
        layout.blush_visible = emotion == Emotion::Happy || emotion == Emotion::Laugh || emotion == Emotion::Love ||
                               emotion == Emotion::Wink || emotion == Emotion::Dance;
        layout.brow_rot = static_cast<int16_t>(rot);
        layout.brow_lift = Round16(static_cast<float>(lift) * fs_);
    }
    layout.face_dx = Round16(face_x_);
    float sway = 0;  // the car rocks the face gently while driving
    if (drive_mode_) sway = glasses_t_ * (2.0f * std::sin(static_cast<float>(now_ms) * 0.0022f) + road_jolt_ * 3.0f);
    // Driving: the face rides a little higher so the sunset sits below the mouth, not behind it.
    layout.face_dy = Round16(face_y_ + sway - glasses_t_ * 36.0f * fs_);
    UpdateGlasses(layout, dt);
    UpdateRoad(dt, now_ms);
    ApplyLayout(layout);

    UpdateDots(now_ms, shown == State::Thinking && mood == Mood::Normal);
    EmitStateFx(shown, mood, now_ms);
    UpdateCostumes(now_ms);
    fx_.Step(dt);
    fx_view_.Render(fx_);

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

void ChipoFace::UpdateGlasses(const Layout& l, float dt) {
    const float target = (drive_mode_ || costume_ == Costume::Sunglasses) ? 1.0f : 0.0f;
    const float speed = dt / (drive_mode_ ? kGlassesInS : kGlassesOutS);
    if (glasses_t_ < target) glasses_t_ = std::min(target, glasses_t_ + speed);
    else if (glasses_t_ > target) glasses_t_ = std::max(target, glasses_t_ - speed);
    Ease(glasses_jolt_, 0.0f, dt, 0.18f);

    const bool visible = glasses_t_ > 0.001f;
    if (visible != glasses_shown_) {
        for (lv_obj_t* piece : glasses_) {
            if (visible) lv_obj_remove_flag(piece, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(piece, LV_OBJ_FLAG_HIDDEN);
        }
        glasses_shown_ = visible;
    }
    if (!visible) return;

    // Slide in with a small overshoot (settles on the eyes), slide out smoothly.
    const float p = glasses_t_;
    float e;
    if (drive_mode_) {
        const float c1 = 1.4f, c3 = c1 + 1.0f, q = p - 1.0f;
        e = 1.0f + c3 * q * q * q + c1 * q * q;  // ease-out-back
    } else {
        e = p * p * (3.0f - 2.0f * p);
    }
    const int dy = Round16((-(1.0f - e) * kGlassesTravel + glasses_jolt_ * 7.0f) * fs_);
    const int dx = Round16(drive_lean_cur_ * 4.0f * fs_);
    const int key[4] = {l.eye_w, l.left_eye_x, dx, dy};
    bool same = true;
    for (int i = 0; i < 4; ++i) same = same && key[i] == glasses_applied_[i];
    if (same) return;
    for (int i = 0; i < 4; ++i) glasses_applied_[i] = key[i];

    const int kPad = S(12);       // lens margin around the eye
    const int kLensH = S(82);
    const int lens_w = l.eye_w + 2 * kPad;
    const int lens_y = S(kEyeCenterY) - kLensH / 2 + dy;
    const int left_x = l.left_eye_x - kPad + dx;
    const int right_x = l.right_eye_x - kPad + dx;
    lv_obj_set_size(glasses_[0], lens_w, kLensH);
    lv_obj_set_pos(glasses_[0], left_x, lens_y);
    lv_obj_set_size(glasses_[1], lens_w, kLensH);
    lv_obj_set_pos(glasses_[1], right_x, lens_y);
    const int bridge_x = left_x + lens_w - S(5);
    lv_obj_set_size(glasses_[2], right_x - bridge_x + S(5), S(8));   // bridge
    lv_obj_set_pos(glasses_[2], bridge_x, lens_y + S(20));
    const int brow_x = left_x + lens_w / 2;
    lv_obj_set_size(glasses_[3], right_x + lens_w / 2 - brow_x, S(6));  // brow bar across the top
    lv_obj_set_pos(glasses_[3], brow_x, lens_y + S(5));
    lv_obj_set_size(glasses_[4], S(20), S(7));                        // arms
    lv_obj_set_pos(glasses_[4], left_x - S(16), lens_y + S(16));
    lv_obj_set_size(glasses_[5], S(20), S(7));
    lv_obj_set_pos(glasses_[5], right_x + lens_w - S(4), lens_y + S(16));
    lv_obj_set_size(glasses_[6], S(26), S(6));                        // glints
    lv_obj_set_pos(glasses_[6], left_x + 16, lens_y + 16);
    lv_obj_set_size(glasses_[7], S(26), S(6));
    lv_obj_set_pos(glasses_[7], right_x + 16, lens_y + 16);
}

void ChipoFace::CreateRoad(lv_obj_t* parent) {
    int32_t w = lv_obj_get_width(parent), h = lv_obj_get_height(parent);
    if (w <= 0) w = 480;
    if (h <= 0) h = 480;
    road_ = lv_obj_create(parent);
    if (road_ == nullptr) return;
    lv_obj_remove_style_all(road_);
    lv_obj_set_size(road_, w, h);
    lv_obj_set_pos(road_, 0, 0);
    MakeInert(road_);
    lv_obj_add_flag(road_, LV_OBJ_FLAG_HIDDEN);
    auto dash = [&](lv_obj_t*& o, uint32_t color) {
        o = lv_obj_create(road_);
        if (o == nullptr) return;
        lv_obj_remove_style_all(o);
        lv_obj_set_style_bg_color(o, lv_color_hex(color), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_radius(o, 3, LV_PART_MAIN);
        MakeInert(o);
    };
    // Synthwave scenery (AMOLED: true black ground, saturated neon): a half sun sets on a glowing horizon.
    const int horizon_y = static_cast<int>(h * 0.78f);  // low enough that the sun sits under the face
    const int sun_d = 150;
    sun_ = lv_obj_create(road_);
    sun_mask_ = lv_obj_create(road_);
    horizon_ = lv_obj_create(road_);
    for (lv_obj_t* o : {sun_, sun_mask_, horizon_}) {
        if (o == nullptr) continue;
        lv_obj_remove_style_all(o);
        lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, LV_PART_MAIN);
        MakeInert(o);
    }
    if (sun_ != nullptr) {
        lv_obj_set_size(sun_, sun_d, sun_d);
        lv_obj_set_pos(sun_, (w - sun_d) / 2, horizon_y - sun_d / 2);
        lv_obj_set_style_radius(sun_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(sun_, lv_color_hex(0xFFD23F), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_color(sun_, lv_color_hex(0xFF2E93), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_dir(sun_, LV_GRAD_DIR_VER, LV_PART_MAIN);
    }
    if (sun_mask_ != nullptr) {
        lv_obj_set_size(sun_mask_, w, h - horizon_y);
        lv_obj_set_pos(sun_mask_, 0, horizon_y);
        lv_obj_set_style_bg_color(sun_mask_, lv_color_black(), LV_PART_MAIN);
    }
    if (horizon_ != nullptr) {
        lv_obj_set_size(horizon_, w - 24, 3);
        lv_obj_set_pos(horizon_, 12, horizon_y);
        lv_obj_set_style_radius(horizon_, 2, LV_PART_MAIN);
        lv_obj_set_style_bg_color(horizon_, lv_color_hex(0x00E5FF), LV_PART_MAIN);
        lv_obj_set_style_shadow_color(horizon_, lv_color_hex(0x00E5FF), LV_PART_MAIN);
        lv_obj_set_style_shadow_width(horizon_, 14, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(horizon_, LV_OPA_70, LV_PART_MAIN);
    }
    for (auto& o : lane_) dash(o, 0x7FF3FF);   // neon cyan lane dashes
    for (auto& o : streak_) dash(o, 0xFF5CC8); // pink speed streaks
    // Steering wheel: only its top edge shows at the bottom of the screen; the orange marker at 12
    // o'clock turns with the curves.
    wheel_ = lv_arc_create(road_);
    if (wheel_ != nullptr) {
        lv_obj_remove_style_all(wheel_);
        lv_obj_set_size(wheel_, 210, 210);
        lv_obj_set_pos(wheel_, (w - 210) / 2, h - 80);
        lv_arc_set_bg_angles(wheel_, 0, 360);
        lv_arc_set_angles(wheel_, 0, 26);
        lv_arc_set_rotation(wheel_, 257);
        lv_obj_set_style_arc_width(wheel_, 13, LV_PART_MAIN);
        lv_obj_set_style_arc_color(wheel_, lv_color_hex(0x2B3644), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(wheel_, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_arc_width(wheel_, 13, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(wheel_, lv_color_hex(0xFFA23A), LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(wheel_, LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(wheel_, true, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(wheel_, true, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(wheel_, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_pad_all(wheel_, 0, LV_PART_KNOB);
        MakeInert(wheel_);
    }
}

void ChipoFace::UpdateRoad(float dt, uint32_t now_ms) {
    (void)now_ms;
    if (road_ == nullptr) return;
    const float target = drive_mode_ ? Clamp(0.5f + road_boost_, 0.08f, 1.4f) : 0.0f;
    Ease(road_boost_, 0.0f, dt, 1.8f);
    Ease(road_speed_, target, dt, 0.5f);
    Ease(road_jolt_, 0.0f, dt, 0.25f);
    const bool visible = glasses_t_ > 0.02f && (drive_mode_ || road_speed_ > 0.02f);  // costume sunglasses bring no road
    if (visible != road_shown_) {
        if (visible) lv_obj_remove_flag(road_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(road_, LV_OBJ_FLAG_HIDDEN);
        road_shown_ = visible;
    }
    if (!visible) return;

    const int w = lv_obj_get_width(road_), h = lv_obj_get_height(road_);
    const float fade = Clamp(glasses_t_, 0.0f, 1.0f);
    const float vigor = Clamp(road_speed_, 0.0f, 1.4f);
    road_phase_ = std::fmod(road_phase_ + dt * (0.2f + 0.9f * vigor), 1.0f);
    streak_phase_ = std::fmod(streak_phase_ + dt * (0.5f + 1.6f * vigor), 1.0f);

    auto place = [&](lv_obj_t* o, RoadPiece& applied, int x, int y, int pw, int ph, int opa) {
        if (opa < 6) opa = 0;
        opa = (opa / 8) * 8;  // few distinct values: fewer redraws
        if (pw < 1) pw = 1;
        if (ph < 1) ph = 1;
        if (x != applied.x || y != applied.y || pw != applied.w || ph != applied.h) {
            lv_obj_set_size(o, pw, ph);
            lv_obj_set_pos(o, x, y);
            applied.x = static_cast<int16_t>(x);
            applied.y = static_cast<int16_t>(y);
            applied.w = static_cast<int16_t>(pw);
            applied.h = static_cast<int16_t>(ph);
        }
        if (opa != applied.opa) {
            lv_obj_set_style_bg_opa(o, static_cast<lv_opa_t>(opa), LV_PART_MAIN);
            applied.opa = static_cast<int16_t>(opa);
        }
    };

    {   // the scenery fades in with the glasses (few opacity steps: few repaints)
        const int opa = fade > 0.97f ? 255 : (static_cast<int>(fade * 255.0f) / 16) * 16;
        if (opa != sun_opa_applied_) {
            if (sun_) lv_obj_set_style_bg_opa(sun_, static_cast<lv_opa_t>(opa * 3 / 4), LV_PART_MAIN);
            if (sun_mask_) lv_obj_set_style_bg_opa(sun_mask_, static_cast<lv_opa_t>(opa), LV_PART_MAIN);
            if (horizon_) {
                lv_obj_set_style_bg_opa(horizon_, static_cast<lv_opa_t>(opa), LV_PART_MAIN);
                lv_obj_set_style_shadow_opa(horizon_, static_cast<lv_opa_t>(opa * 2 / 3), LV_PART_MAIN);
            }
            sun_opa_applied_ = opa;
        }
    }
    // Lane dashes ahead: they grow and spread as they come towards the car, and swing sideways in curves.
    const float y0 = h * 0.66f, y1 = h * 0.80f;
    for (int i = 0; i < kLaneDashes; ++i) {
        const float p = std::fmod(road_phase_ + static_cast<float>(i) / kLaneDashes, 1.0f);
        const float yy = y0 + p * p * (y1 - y0) + road_jolt_ * 6.0f;
        const float dw = 4.0f + p * 14.0f, dh = 5.0f + p * 22.0f;
        const float cx = w / 2.0f - drive_lean_cur_ * 22.0f * p;
        const float in = p < 0.15f ? p / 0.15f : 1.0f;
        place(lane_[i], road_applied_[i], Round16(cx - dw / 2), Round16(yy), Round16(dw), Round16(dh),
              static_cast<int>(fade * in * 230.0f * Clamp(0.4f + vigor, 0.0f, 1.0f)));
    }
    // Speed streaks at the sides, flying outwards. Faster with a faster scene, invisible when it crawls.
    const float rows[4] = {0.30f, 0.38f, 0.50f, 0.62f};
    const float strength = Clamp((vigor - 0.15f) * 1.2f, 0.0f, 1.0f);
    for (int j = 0; j < kStreaks; ++j) {
        const int side = j % 2, row = j / 2;
        const float q = std::fmod(streak_phase_ + row * 0.23f + side * 0.11f, 1.0f);
        const float len = 10.0f + q * 70.0f * (0.4f + vigor);
        const float lead = w * 0.2f - q * (w * 0.2f + 16.0f);
        const int x = side == 0 ? Round16(lead) : Round16(w - lead - len);
        const float bell = std::sin(3.14159265f * q);
        place(streak_[j], road_applied_[kLaneDashes + j], x, Round16(h * rows[row] + road_jolt_ * 4.0f), Round16(len), 3,
              static_cast<int>(fade * strength * bell * 255.0f));
    }
    // Steering wheel marker follows the curves.
    if (wheel_ != nullptr) {
        Ease(wheel_angle_, Clamp(drive_lean_cur_ * 35.0f, -40.0f, 40.0f), dt, 0.15f);
        const int rotation = Round16(257.0f + wheel_angle_);
        if (rotation != wheel_rot_applied_) {
            lv_arc_set_rotation(wheel_, rotation);
            wheel_rot_applied_ = rotation;
        }
        const int y = h - 80 + Round16(road_jolt_ * 4.0f);
        if (y != wheel_y_applied_) {
            lv_obj_set_pos(wheel_, (w - 210) / 2, y);
            wheel_y_applied_ = y;
        }
        int opa = (static_cast<int>(fade * 255.0f) / 16) * 16;
        if (opa != wheel_opa_applied_) {
            lv_obj_set_style_arc_opa(wheel_, static_cast<lv_opa_t>(opa), LV_PART_MAIN);
            lv_obj_set_style_arc_opa(wheel_, static_cast<lv_opa_t>(opa), LV_PART_INDICATOR);
            wheel_opa_applied_ = opa;
        }
    }
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
    if (first || l.glow_rgb != a.glow_rgb || l.ring != a.ring) {
        const lv_color_t color = lv_color_hex(l.glow_rgb);
        // Cute skin: the mouth is a pink smile and the eyes lilac; the state colour tints them (about a third)
        // so LISTENING / THINKING / ERROR / the emotions still read, and keeps the soft halo around them.
        lv_obj_set_style_bg_color(bowl_, (IsDefaultGlow(l.glow_rgb) ? lv_color_hex(kMouthLip) : lv_color_mix(color, lv_color_hex(kMouthLip), 80)), LV_PART_MAIN);
        for (lv_obj_t* o : {left_eye_, right_eye_}) {  // a coloured ring tells the state; plain lilac when calm
            lv_obj_set_style_border_width(o, l.ring ? S(5) : 0, LV_PART_MAIN);
            lv_obj_set_style_border_color(o, IsDefaultGlow(l.glow_rgb) ? lv_color_hex(kRingCyan) : color, LV_PART_MAIN);
            lv_obj_set_style_border_opa(o, LV_OPA_COVER, LV_PART_MAIN);
        }
        for (lv_obj_t* o : {left_eye_, right_eye_}) {
            lv_obj_set_style_bg_color(o, IsDefaultGlow(l.glow_rgb) ? lv_color_hex(kEyeLilac)
                                                       : lv_color_mix(color, lv_color_hex(kEyeLilac), 70), LV_PART_MAIN);
            lv_obj_set_style_bg_grad_color(o, IsDefaultGlow(l.glow_rgb) ? lv_color_hex(0xC9A6FF)
                                                       : lv_color_mix(color, lv_color_hex(kEyeLilac), 110), LV_PART_MAIN);
            lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, LV_PART_MAIN);
            lv_obj_set_style_bg_main_stop(o, 40, LV_PART_MAIN);
            lv_obj_set_style_bg_grad_stop(o, 255, LV_PART_MAIN);
        }
    }
    if (bowl_ && (first || l.mouth_w != a.mouth_w || l.mouth_h != a.mouth_h || l.mouth_kind != a.mouth_kind)) {
        const int w = l.mouth_w, h = l.mouth_h, rim = S(3);
        const bool open = h >= S(14) && w >= S(20);
        bool show_inner = false, show_tongue = false;
        switch (l.mouth_kind) {
        case kMouthFrown:  // upside-down half-moon: the top half of an ellipse
            lv_obj_set_size(bowl_, w, 2 * h);
            lv_obj_set_pos(bowl_, 0, 0);
            break;
        case kMouthO:      // a round "o", dark inside when it is big enough
            lv_obj_set_size(bowl_, w, h);
            lv_obj_set_pos(bowl_, 0, 0);
            if (inner_ && h >= S(10) && w >= S(10)) {
                lv_obj_set_size(inner_, w - 2 * rim, h - 2 * rim);
                lv_obj_set_pos(inner_, rim, rim);
                show_inner = true;
            }
            break;
        case kMouthFlat:   // a pill: a straight, unimpressed line
            lv_obj_set_size(bowl_, w, std::max<int>(S(5), std::min<int>(h, S(8))));
            lv_obj_set_pos(bowl_, 0, std::max(0, (h - S(8)) / 2));
            break;
        default:           // half-moon smile, open with tongue when it is big enough (a grin stays closed)
            lv_obj_set_size(bowl_, w, 2 * h);
            lv_obj_set_pos(bowl_, 0, -h);
            if (l.mouth_kind == kMouthGrin) break;
            if (open && inner_) {
                lv_obj_set_size(inner_, w - 2 * rim, 2 * (h - rim));
                lv_obj_set_pos(inner_, rim, -h + rim);
                show_inner = true;
            }
            if (open && tongue_) {
                const int tw = std::max<int>(S(12), w * 44 / 100);
                const int vh = std::max<int>(S(5), h * 48 / 100);  // visible tongue height
                lv_obj_set_size(tongue_, tw, 2 * vh);
                lv_obj_set_pos(tongue_, (w - tw) / 2, h - vh - rim);
                show_tongue = true;
            }
            break;
        }
        if (inner_) { if (show_inner) lv_obj_remove_flag(inner_, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(inner_, LV_OBJ_FLAG_HIDDEN); }
        if (tongue_) { if (show_tongue) lv_obj_remove_flag(tongue_, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(tongue_, LV_OBJ_FLAG_HIDDEN); }
    }
    if (halo_[0] && halo_[1] && halo_[2] && halo_[3] &&
        (first || l.ring != a.ring || l.glow_rgb != a.glow_rgb || l.glow_w != a.glow_w || l.glow_opa != a.glow_opa ||
         l.eye_arc != a.eye_arc || l.eye_w != a.eye_w || l.eye_h != a.eye_h || l.eye_y != a.eye_y ||
         l.right_eye_h != a.right_eye_h || l.right_eye_y != a.right_eye_y || l.left_eye_x != a.left_eye_x ||
         l.right_eye_x != a.right_eye_x)) {
        // A soft coloured halo that grows with the face: the shadow glow cannot be scaled up (big software
        // shadows stall the renderer), so on bigger panels this flat disc carries the state colour.
        const bool on = l.ring && l.eye_arc == 0;
        const int g = S(4) + static_cast<int>(static_cast<float>(l.glow_w) * 0.35f);  // a soft rim, not a big blob
        const int base = std::min<int>(255, l.glow_opa * 255 / 100);
        const int16_t hs[2] = {l.eye_h, l.right_eye_h};
        const int16_t ys[2] = {l.eye_y, l.right_eye_y};
        const int16_t xs[2] = {l.left_eye_x, l.right_eye_x};
        for (int k = 0; k < 4; ++k) {
            const int i = k & 1;                     // which eye
            const int gg = k < 2 ? g : g / 2;        // outer, then inner ring
            const int opa = base * (k < 2 ? 18 : 30) / 100;
            if (!on) { lv_obj_add_flag(halo_[k], LV_OBJ_FLAG_HIDDEN); continue; }
            lv_obj_set_size(halo_[k], l.eye_w + 2 * gg, hs[i] + 2 * gg);
            lv_obj_set_pos(halo_[k], xs[i] - gg, ys[i] - gg);
            lv_obj_set_style_bg_color(halo_[k], lv_color_hex(IsDefaultGlow(l.glow_rgb) ? kRingCyan : l.glow_rgb), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(halo_[k], static_cast<lv_opa_t>(opa), LV_PART_MAIN);
            lv_obj_remove_flag(halo_[k], LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (eye_arc_[0] && eye_arc_[1] &&
        (first || l.eye_arc != a.eye_arc || l.eye_w != a.eye_w || l.left_eye_x != a.left_eye_x ||
         l.right_eye_x != a.right_eye_x)) {
        const bool arcs = l.eye_arc != 0;
        for (lv_obj_t* e : {left_eye_, right_eye_}) {
            if (arcs) lv_obj_add_flag(e, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_remove_flag(e, LV_OBJ_FLAG_HIDDEN);
        }
        const int d = l.eye_w;
        const int cy = S(kEyeCenterY);
        for (int i = 0; i < 2; ++i) {
            lv_obj_t* arc = eye_arc_[i];
            if (!arcs) { lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN); continue; }
            lv_obj_set_size(arc, d, d);
            const int x = i == 0 ? l.left_eye_x : l.right_eye_x;
            if (l.eye_arc == 1) {  // happy: the top of a circle, like ^
                lv_arc_set_angles(arc, 200, 340);
                lv_obj_set_pos(arc, x, cy - d / 2 + d / 5);
                lv_obj_set_style_arc_color(arc, lv_color_hex(kHappyArc), LV_PART_INDICATOR);
            } else {               // asleep: the bottom of a circle, like u
                lv_arc_set_angles(arc, 20, 160);
                lv_obj_set_pos(arc, x, cy - d / 2 - d / 4);
                lv_obj_set_style_arc_color(arc, lv_color_hex(kSleepArc), LV_PART_INDICATOR);
            }
            lv_obj_remove_flag(arc, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (blush_[0] && blush_[1] && (first || l.blush_visible != a.blush_visible)) {
        for (auto* b : blush_) {
            if (l.blush_visible) lv_obj_remove_flag(b, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (blush_[0] && blush_[1] &&
        (first || l.left_eye_x != a.left_eye_x || l.right_eye_x != a.right_eye_x || l.eye_w != a.eye_w ||
         l.mouth_y != a.mouth_y || l.mouth_h != a.mouth_h)) {
        const int by = l.mouth_y + l.mouth_h / 2 - S(22);
        lv_obj_set_pos(blush_[0], l.left_eye_x - S(6), by);
        lv_obj_set_pos(blush_[1], l.right_eye_x + l.eye_w - S(34), by);
    }
    if (brow_[0] && brow_[1] &&
        (first || l.brows_visible != a.brows_visible || l.brow_rot != a.brow_rot || l.brow_lift != a.brow_lift ||
         l.eye_y != a.eye_y || l.left_eye_x != a.left_eye_x || l.right_eye_x != a.right_eye_x || l.eye_w != a.eye_w)) {
        for (int i = 0; i < 2; ++i) {
            if (l.brows_visible) lv_obj_remove_flag(brow_[i], LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(brow_[i], LV_OBJ_FLAG_HIDDEN);
        }
        const int by = l.eye_y - S(17) - l.brow_lift;
        lv_obj_set_pos(brow_[0], l.left_eye_x + l.eye_w / 2 - S(15), by);
        lv_obj_set_pos(brow_[1], l.right_eye_x + l.eye_w / 2 - S(15), by);
        lv_obj_set_style_transform_rotation(brow_[0], -l.brow_rot, LV_PART_MAIN);
        lv_obj_set_style_transform_rotation(brow_[1], l.brow_rot, LV_PART_MAIN);
    }
    if (first || l.face_dx != a.face_dx || l.face_dy != a.face_dy) {
        lv_obj_align(root_, LV_ALIGN_CENTER, l.face_dx + shift_x_, l.face_dy + shift_y_);
    }
    applied_ = l;
    layout_applied_ = true;
}

void ChipoFace::FaceCentre(float* x, float* y) const {
    *x = static_cast<float>(fx_w_) / 2.0f + static_cast<float>(applied_.face_dx + shift_x_);
    *y = static_cast<float>(fx_h_) / 2.0f + static_cast<float>(applied_.face_dy + shift_y_);
}

void ChipoFace::LookAt(float nx, float ny) {
    if (!root_) return;
    gaze_nx_ = Clamp(nx, -1.0f, 1.0f);
    gaze_ny_ = Clamp(ny, -1.0f, 1.0f);
    gaze_until_ms_ = lv_tick_get() + 500;
}

void ChipoFace::StopLookAt() { gaze_until_ms_ = 0; }

void ChipoFace::SlowBlink() {
    if (!root_) return;
    slow_blink_pending_ = true;
    if (blink_phase_ == 0) next_blink_ms_ = lv_tick_get();
}

void ChipoFace::Celebrate(Burst burst, int count) {
    if (!root_) return;
    float cx, cy;
    FaceCentre(&cx, &cy);
    if (count < 1) count = 1;
    if (burst == Burst::Hearts) {
        fx_.Burst(FxKind::Heart, count, cx, cy - 10.0f, 88.0f);
        SlowBlink();
    } else {
        fx_.Burst(FxKind::Sparkle, count, cx, cy - 10.0f, 82.0f);
    }
}

void ChipoFace::Cry(uint32_t ms) {
    if (!root_) return;
    const uint32_t now = lv_tick_get();
    StartEmotion(Emotion::Sad, now, ms);
    tears_until_ms_ = now + ms;
    next_tear_ms_ = now;
}

void ChipoFace::SoreLoser() {
    if (!root_) return;
    const uint32_t now = lv_tick_get();
    tantrum_until_ms_ = now + 1800;
    next_kick_ms_ = now;
    kick_dir_ = 1;
    StartEmotion(Emotion::Grumpy, now, 1800);
}

void ChipoFace::SetDance(bool on) {
    if (!root_) return;
    if (on == dance_on_) return;
    dance_on_ = on;
    if (on) {
        const uint32_t now = lv_tick_get();
        dance_t0_ = now;
        dance_until_ms_ = now + 180000;
        next_hop_ms_ = now;
        dance_hops_ = 0;
        hop_dir_ = 1;
        idle_since_ms_ = now;
        Celebrate(Burst::Sparkles, 4);
    }
}

void ChipoFace::SetCostume(Costume costume) {
    costume_manual_ = true;
    if (costume == costume_) return;
    costume_ = costume;
    if (root_ && costume != Costume::None) Celebrate(Burst::Sparkles, 5);
}

void ChipoFace::EmitStateFx(State shown, Mood mood, uint32_t now_ms) {
    float cx, cy;
    FaceCentre(&cx, &cy);
    // Tears run from the lower rim of the eyes.
    if (now_ms < tears_until_ms_ && mood == Mood::Normal && shown != State::Sleep && shown != State::Error &&
        now_ms >= next_tear_ms_) {
        const float eye_w = static_cast<float>(applied_.eye_w);
        const float eye_h = static_cast<float>(applied_.eye_h);
        const float rx = (tear_side_ == 0) ? (kCenterX - kEyeGap / 2.0f) * fs_ - eye_w * 0.5f : (kCenterX + kEyeGap / 2.0f) * fs_ + eye_w * 0.5f;
        fx_.Spawn(FxKind::Tear, cx - kCanvasWidth * fs_ / 2.0f + rx + (tear_side_ == 0 ? -6.0f : 6.0f) * fs_,
                  cy - kCanvasHeight * fs_ / 2.0f + kEyeCenterY * fs_ + eye_h * 0.5f - 2.0f, 12.0f * fs_, 0x8FD0FF);
        tear_side_ ^= 1;
        next_tear_ms_ = now_ms + 420;
    }
    // Asleep: a Z floats up now and then.
    if (shown == State::Sleep) {
        if (next_zzz_ms_ == 0) next_zzz_ms_ = now_ms + 700;
        if (now_ms >= next_zzz_ms_ && fx_.alive(FxKind::Zzz) < 3) {
            fx_.Spawn(FxKind::Zzz, cx + 74.0f * fs_, cy - 52.0f * fs_, 24.0f * fs_, 0xC9B0FF);
            next_zzz_ms_ = now_ms + 1500;
        }
    } else {
        next_zzz_ms_ = 0;
    }
}

void ChipoFace::CreateCostumes() {
    auto flat = [](lv_obj_t* o, uint32_t color, int radius) {
        lv_obj_remove_style_all(o);
        lv_obj_set_style_bg_color(o, lv_color_hex(color), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(o, radius, LV_PART_MAIN);
        MakeInert(o);
        lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    };
    for (auto& o : hat_) o = lv_obj_create(root_);
    for (auto& o : moustache_) o = lv_obj_create(root_);
    // Santa hat above the eyes (inside the 300 x 220 face canvas).
    if (hat_[0]) { flat(hat_[0], 0xE0262B, 40); lv_obj_set_size(hat_[0], S(130), S(36)); lv_obj_set_pos(hat_[0], S(85), S(2)); }
    if (hat_[1]) { flat(hat_[1], 0xF4F4F4, 9); lv_obj_set_size(hat_[1], S(146), S(18)); lv_obj_set_pos(hat_[1], S(77), S(30)); }
    if (hat_[2]) {
        flat(hat_[2], 0xE0262B, 11);
        lv_obj_set_size(hat_[2], S(52), S(22));
        lv_obj_set_pos(hat_[2], S(176), S(8));
        lv_obj_set_style_transform_pivot_x(hat_[2], S(26), LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(hat_[2], S(11), LV_PART_MAIN);
        lv_obj_set_style_transform_rotation(hat_[2], 200, LV_PART_MAIN);
    }
    if (hat_[3]) { flat(hat_[3], 0xF4F4F4, LV_RADIUS_CIRCLE); lv_obj_set_size(hat_[3], S(26), S(26)); lv_obj_set_pos(hat_[3], S(218), S(16)); }
    // Moustache between the eyes and the mouth: two curls turned a little.
    for (int i = 0; i < kMoustachePieces; ++i) {
        lv_obj_t* o = moustache_[i];
        if (!o) continue;
        flat(o, 0xC98A4B, 10);
        lv_obj_set_size(o, S(60), S(20));
        lv_obj_set_pos(o, S(i == 0 ? 90 : 150), S(132));
        lv_obj_set_style_transform_pivot_x(o, S(30), LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(o, S(10), LV_PART_MAIN);
        lv_obj_set_style_transform_rotation(o, i == 0 ? 120 : -120, LV_PART_MAIN);  // tips curl up
    }
    hat_shown_ = moustache_shown_ = false;
    hat_bob_applied_ = -9999;
}

void ChipoFace::UpdateCostumes(uint32_t now_ms) {
    // The calendar picks the costume unless the owner chose one.
    if (!costume_manual_ && date_source_ && now_ms >= next_date_check_ms_) {
        next_date_check_ms_ = now_ms + 60000;
        int month = 0, day = 0;
        if (date_source_(&month, &day)) costume_ = CostumeForDate(month, day);
    }
    const bool want_hat = costume_ == Costume::SantaHat && shown_ != State::Sleep;
    const bool want_moustache = costume_ == Costume::Mustache;
    if (want_hat != hat_shown_) {
        for (auto* o : hat_) {
            if (!o) continue;
            if (want_hat) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
        }
        hat_shown_ = want_hat;
        hat_bob_applied_ = -9999;
    }
    if (want_moustache != moustache_shown_) {
        for (auto* o : moustache_) {
            if (!o) continue;
            if (want_moustache) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
        }
        moustache_shown_ = want_moustache;
    }
    if (hat_shown_ && hat_[3]) {  // the pompom bobs
        const int bob = Round16(2.0f * std::sin(static_cast<float>(now_ms) * 0.004f));
        if (bob != hat_bob_applied_) {
            hat_bob_applied_ = bob;
            lv_obj_set_pos(hat_[3], S(218), S(16) + Round16(bob * fs_));
        }
    }
}

void ChipoFace::SetPixelShift(int dx, int dy) {
    if (dx == shift_x_ && dy == shift_y_) return;
    shift_x_ = dx;
    shift_y_ = dy;
    if (root_ && layout_applied_) lv_obj_align(root_, LV_ALIGN_CENTER, applied_.face_dx + dx, applied_.face_dy + dy);
}

ChipoFace::DebugInfo ChipoFace::Debug() const {
    DebugInfo d{};
    d.step_period_ms = motion_period_ms_;
    d.shown_state = shown_;
    d.mood = mood_;
    d.face_dx = applied_.face_dx;
    d.face_dy = applied_.face_dy;
    d.fall_dir = fall_dir_;
    d.fell = fell_;
    // Sizes in design pixels (scale 1) so they mean the same on every panel;
    // face_scale says how big they really are on this screen.
    auto design = [this](int v) { return static_cast<int>(std::lround(static_cast<float>(v) / fs_)); };
    d.face_scale = fs_;
    d.eye_w = design(applied_.eye_w);
    d.eye_h = design(applied_.eye_h);
    d.mouth_w = design(applied_.mouth_w);
    d.mouth_h = design(applied_.mouth_h);
    d.thinking_dots_visible = dots_visible_;
    d.tilt_calibrated = tilt_.output().calibrated;
    d.tilt_angle_deg = tilt_.output().angle_deg;
    d.emotion = shown_emotion_;
    d.right_eye_h = design(applied_.right_eye_h);
    d.glow_rgb = applied_.glow_rgb;
    d.gyro_available = gyro_.output().available;
    d.gyro_spinning = gyro_.output().spinning;
    d.drive_mode = drive_mode_;
    d.side_lying = side_lying_;
    d.desk_clock = desk_clock_;
    d.glasses_progress = glasses_t_;
    d.road_visible = road_shown_;
    d.road_speed = road_speed_;
    d.wheel_angle_deg = wheel_angle_;
    d.gaze_active = gaze_on_;
    d.gaze_px_x = gaze_px_x_;
    d.gaze_px_y = gaze_px_y_;
    d.stretches = stretches_;
    d.slow_blinks = slow_blinks_;
    d.fx_hearts = fx_.alive(FxKind::Heart);
    d.fx_sparkles = fx_.alive(FxKind::Sparkle);
    d.fx_tears = fx_.alive(FxKind::Tear);
    d.fx_zzz = fx_.alive(FxKind::Zzz);
    d.crying = tears_until_ms_ != 0 && lv_tick_get() < tears_until_ms_;
    d.tantrum = tantrum_until_ms_ != 0;
    d.costume = costume_;
    d.dancing = dance_on_;
    d.dance_hops = dance_hops_;
    d.dance_level = dance_level_;
    return d;
}
