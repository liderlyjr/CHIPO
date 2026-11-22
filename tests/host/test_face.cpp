// Host-side behaviour test for ChipoFace on a real LVGL build (no display needed): fake clock,
// fake accelerometer. See tests/host/run.sh. Run under AddressSanitizer/UBSan.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

#include "chipo_face.h"

static int g_failures = 0;
#define CHECK(cond, ...)                              \
    do {                                              \
        if (!(cond)) {                                \
            ++g_failures;                             \
            std::printf("FAIL line %d: ", __LINE__);  \
            std::printf(__VA_ARGS__);                 \
            std::printf("\n");                        \
        }                                             \
    } while (0)

static uint32_t g_now = 1000;
static uint32_t FakeTick() { return g_now; }
static uint8_t g_buf[480 * 40 * 4];
static void Flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

using State = ChipoFace::State;
using Mood = ChipoFace::Mood;
using Emotion = ChipoFace::Emotion;
constexpr float kDeg = 3.14159265f / 180.0f;

// Fake accelerometer: the sensor is mounted with gravity on +Y when CHIPO stands upright.
static ImuSample g_imu{0, 1, 0};
static bool g_imu_new = false;
static void SetPose(float roll_deg, float pitch_deg) {
    g_imu = {std::sin(roll_deg * kDeg) * std::cos(pitch_deg * kDeg),
             std::cos(roll_deg * kDeg) * std::cos(pitch_deg * kDeg), std::sin(pitch_deg * kDeg)};
}

// Fake gyroscope (degrees per second); off by default, like a board without one.
static bool g_gyro_on = false;
static float g_gx = 0, g_gy = 0, g_gz = 0;
static void SetGyro(float gx, float gy, float gz) {
    g_gyro_on = true;
    g_gx = gx; g_gy = gy; g_gz = gz;
}
static void GyroOff() {
    g_gyro_on = false;
    g_gx = g_gy = g_gz = 0;
}

static void Advance(ChipoFace& f, uint32_t ms, const std::function<void()>& each = nullptr) {
    for (uint32_t t = 0; t < ms; t += 40) {
        g_now += 40;
        g_imu_new = true;
        f.Step(g_now);
        if (each) each();
    }
}

static bool HasFlag(lv_obj_t* o, lv_obj_flag_t flag) { return lv_obj_has_flag(o, flag); }

// Square wave of rotation around one sensor axis (0 = X, 1 = Y, 2 = Z).
static void GyroSwing(ChipoFace& f, int axis, float amplitude, int swings, uint32_t half_ms,
                      const std::function<void()>& each = nullptr) {
    float sign = 1;
    for (int i = 0; i < swings; ++i) {
        for (uint32_t t = 0; t < half_ms; t += 40) {
            float g[3] = {0, 0, 0};
            g[axis] = sign * amplitude;
            SetGyro(g[0], g[1], g[2]);
            Advance(f, 40, each);
        }
        sign = -sign;
    }
    SetGyro(0, 0, 0);
}

int main() {
    lv_init();
    lv_tick_set_cb(FakeTick);
    lv_display_t* d = lv_display_create(480, 480);
    lv_display_set_buffers(d, g_buf, nullptr, sizeof(g_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, Flush);

    for (int round = 0; round < 2; ++round) {
        std::printf("== round %d\n", round);
        g_now += 1000;
        SetPose(0, 0);
        auto* face = new ChipoFace();
        face->SetIdleQuirks(false);  // the random idle winks have their own test at the end
        bool imu_on = true;
        face->SetTiltSource([&](ImuSample* s) {
            if (!imu_on || !g_imu_new) return false;
            g_imu_new = false;
            *s = g_imu;
            if (g_gyro_on) {
                s->has_gyro = true;
                s->gx = g_gx; s->gy = g_gy; s->gz = g_gz;
            }
            return true;
        });
        CHECK(face->Start(lv_screen_active()), "Start failed");
        CHECK(face->Start(lv_screen_active()), "second Start should be a no-op");

        // ---- idle, blinking, nothing clickable ------------------------------------------------
        int min_eye = 999, max_eye = 0, blinks = 0;
        bool was_closed = false;
        Advance(*face, 14000, [&] {
            auto dbg = face->Debug();
            min_eye = std::min(min_eye, dbg.eye_h);
            max_eye = std::max(max_eye, dbg.eye_h);
            const bool closed = dbg.eye_h < 30;
            if (closed && !was_closed) ++blinks;
            was_closed = closed;
        });
        CHECK(min_eye <= 10, "eyes never closed (min %d)", min_eye);
        CHECK(max_eye >= 68 && max_eye <= 72, "open eye height %d", max_eye);
        CHECK(blinks >= 2 && blinks <= 10, "blinks in 14 s: %d", blinks);
        CHECK(face->Debug().shown_state == State::Idle && face->Debug().mood == Mood::Normal,
              "idle expected");
        CHECK(face->Debug().tilt_calibrated, "tilt did not calibrate while idle");
        CHECK(face->Debug().face_dx == 0 && face->Debug().face_dy == 0, "face should rest at centre");

        lv_obj_t* root = lv_obj_get_child(lv_screen_active(), -1);
        CHECK(root && !HasFlag(root, LV_OBJ_FLAG_CLICKABLE), "face root swallows touches");
        for (uint32_t i = 0; root && i < lv_obj_get_child_count(root); ++i) {
            CHECK(!HasFlag(lv_obj_get_child(root, i), LV_OBJ_FLAG_CLICKABLE), "child %u clickable", i);
        }

        // ---- smooth transitions (never snaps) -------------------------------------------------
        face->SetState(State::Listening);
        int prev = face->Debug().eye_h;
        int biggest_jump = 0;
        bool grew = false;
        int blink_cooldown = 0;
        int tallest = 0;
        Advance(*face, 800, [&] {
            const int h = face->Debug().eye_h;
            tallest = std::max(tallest, h);
            if (h < 68) blink_cooldown = 8;  // a blink: ignore it and the 8 steps (320 ms) while the eyes reopen
            if (blink_cooldown > 0) { --blink_cooldown; prev = h; return; }
            if (prev < 68) { prev = h; return; }  // open eyes are 70+
            biggest_jump = std::max(biggest_jump, std::abs(h - prev));
            prev = h;
        });
        grew = tallest >= 76;  // (a blink may land on the last step, so look at the tallest the eyes got)
        CHECK(grew, "listening eyes did not grow (%d)", tallest);
        CHECK(biggest_jump <= 6, "expression snapped: %d px in one step", biggest_jump);

        // ---- thinking is driven by SetThinking, and cleared by Speaking -----------------------
        face->SetThinking(true);
        Advance(*face, 400);
        CHECK(face->Debug().shown_state == State::Thinking, "not thinking");
        CHECK(face->Debug().thinking_dots_visible, "dots hidden while thinking");
        face->SetState(State::Speaking);
        int mh_min = 999, mh_max = 0;
        Advance(*face, 2000, [&] {
            mh_min = std::min(mh_min, face->Debug().mouth_h);
            mh_max = std::max(mh_max, face->Debug().mouth_h);
        });
        CHECK(face->Debug().shown_state == State::Speaking, "not speaking");
        CHECK(!face->Debug().thinking_dots_visible, "dots still visible while speaking");
        CHECK(mh_max - mh_min >= 10, "mouth did not move while speaking (%d..%d)", mh_min, mh_max);
        face->SetState(State::Listening);
        Advance(*face, 600);
        CHECK(face->Debug().shown_state == State::Listening, "thinking came back by itself");

        // Thinking gives up if the server never answers.
        face->SetThinking(true);
        Advance(*face, 26000);
        CHECK(face->Debug().shown_state == State::Listening, "thinking did not time out");
        // ... and is ignored while speaking.
        face->SetState(State::Speaking);
        face->SetThinking(true);
        Advance(*face, 200);
        CHECK(face->Debug().shown_state == State::Speaking, "thinking overrode speaking");
        face->SetState(State::Idle);
        Advance(*face, 1500);

        // ---- tilt: lean, fall, relief ---------------------------------------------------------
        SetPose(0, 0);
        Advance(*face, 1500);
        SetPose(20, 0);
        Advance(*face, 1500);
        const int lean_dx = face->Debug().face_dx;
        CHECK(std::abs(lean_dx) >= 25 && std::abs(lean_dx) <= 70, "lean dx %d", lean_dx);
        CHECK(face->Debug().mood == Mood::Normal, "a gentle lean should not panic");
        SetPose(-20, 0);
        Advance(*face, 1500);
        CHECK(face->Debug().face_dx * lean_dx < 0, "lean other side did not flip dx");
        SetPose(0, 0);
        Advance(*face, 1500);
        CHECK(std::abs(face->Debug().face_dx) <= 1, "face did not return to centre");

        // Spring overshoot: going from centre to a lean overshoots the final position.
        SetPose(20, 0);
        int overshoot_peak = 0;
        Advance(*face, 1500, [&] { overshoot_peak = std::max(overshoot_peak, std::abs(face->Debug().face_dx)); });
        CHECK(overshoot_peak > std::abs(face->Debug().face_dx), "no wobble (peak %d final %d)",
              overshoot_peak, face->Debug().face_dx);
        SetPose(0, 0);
        Advance(*face, 1500);

        // Sudden tip over: startled at once, mouth opens wide, eyes grow.
        SetPose(60, 0);
        Advance(*face, 120);
        CHECK(face->Debug().mood == Mood::Startled, "no startle on sudden fall");
        Advance(*face, 800);
        CHECK(face->Debug().mood == Mood::Startled, "should keep panicking while tilted 60 deg");
        CHECK(face->Debug().mouth_h >= 26 && face->Debug().eye_h >= 82, "panic face too mild (%d,%d)",
              face->Debug().mouth_h, face->Debug().eye_h);
        SetPose(0, 0);
        Advance(*face, 400);
        CHECK(face->Debug().mood == Mood::Relieved, "no relief after standing up");
        Advance(*face, 1500);
        CHECK(face->Debug().mood == Mood::Normal, "relief did not end");

        // ---- past 45 degrees the face slips off the screen towards the side it leans to -------------
        const int side = lean_dx > 0 ? 1 : -1;  // the side a +roll lean pushes the face to
        SetPose(38, 0);
        Advance(*face, 2000);
        CHECK(!face->Debug().fell && std::abs(face->Debug().face_dx) < 100 && face->Debug().fall_dir == 0,
              "38 deg must still be on screen (dx %d)", face->Debug().face_dx);
        SetPose(0, 0);
        Advance(*face, 2000);
        SetPose(50, 0);
        Advance(*face, 320);  // the sensor reading is smoothed over ~80 ms, so give it a moment to pass 45 degrees
        CHECK(face->Debug().fell && face->Debug().fall_dir == side, "should start slipping towards the lean side");
        CHECK(face->Debug().face_dx * side > 50 && face->Debug().face_dx * side < 330, "not mid-slide (dx %d)",
              face->Debug().face_dx);
        Advance(*face, 1200);
        CHECK(face->Debug().face_dx * side >= 320, "face did not leave the screen (dx %d)", face->Debug().face_dx);
        CHECK(face->Debug().face_dy > 20, "it should drop while it falls (dy %d)", face->Debug().face_dy);
        Advance(*face, 3000);
        CHECK(face->Debug().face_dx * side >= 320, "face came back while still tilted");
        SetPose(0, 0);
        Advance(*face, 250);
        CHECK(!face->Debug().fell && face->Debug().face_dx * side < 320, "face did not start coming back");
        Advance(*face, 2500);
        CHECK(std::abs(face->Debug().face_dx) <= 1 && std::abs(face->Debug().face_dy) <= 1, "face did not settle back (%d,%d)",
              face->Debug().face_dx, face->Debug().face_dy);
        CHECK(face->Debug().mood == Mood::Normal, "relief did not end after coming back");

        // The other way.
        SetPose(-50, 0);
        Advance(*face, 1500);
        CHECK(face->Debug().fall_dir == -side && face->Debug().face_dx * side <= -320, "did not fall the other way (dx %d)",
              face->Debug().face_dx);
        SetPose(0, 0);
        Advance(*face, 3000);

        // Tipped towards or away from the viewer: no side to slip to, so it only panics.
        SetPose(0, 60);
        Advance(*face, 1500);
        CHECK(face->Debug().fell && face->Debug().fall_dir == 0 && std::abs(face->Debug().face_dx) < 100 &&
              face->Debug().mood == Mood::Startled, "forward tip should panic without sliding (dx %d)",
              face->Debug().face_dx);
        SetPose(0, 0);
        Advance(*face, 3000);

        // Falls off, then ends up lying flat: it comes back, dizzy.
        // (the opposite side to the desk-clock one: lying on that side makes a clock, see below)
        SetPose(-80, 0);
        Advance(*face, 900);
        CHECK(face->Debug().face_dx * side <= -320, "should have fallen before lying down");
        Advance(*face, 3500);
        CHECK(face->Debug().mood == Mood::Dizzy && std::abs(face->Debug().face_dx) < 100,
              "lying flat should bring it back dizzy (dx %d)", face->Debug().face_dx);
        SetPose(0, 0);
        Advance(*face, 3500);
        CHECK(face->Debug().mood == Mood::Normal && std::abs(face->Debug().face_dx) <= 1, "did not recover from lying");

        // Lying down for a while: dizzy, face re-centred.
        SetPose(-90, 0);
        Advance(*face, 4000);
        CHECK(face->Debug().mood == Mood::Dizzy, "not dizzy when lying");
        CHECK(face->Debug().eye_h <= 20, "dizzy eyes too open (%d)", face->Debug().eye_h);
        SetPose(0, 0);
        Advance(*face, 3000);
        CHECK(face->Debug().mood == Mood::Normal, "dizziness did not end");

        // ---- desk clock: lying on its left side (+roll in this fake mounting) ---------------------------
        {
            int clock_events_on = 0, clock_events_off = 0;
            face->SetDeskClockObserver([&](bool on, int side_down) { CHECK(side_down == -1, "side reported"); (on ? clock_events_on : clock_events_off)++; });
            SetPose(90, 0);
            Advance(*face, 1200);
            CHECK(face->Debug().side_lying && !face->Debug().desk_clock, "on its side but the clock is not up yet");
            CHECK(face->Debug().face_dx * side >= 320 || face->Debug().face_dx * side <= -320, "the face fell off and stays gone (dx %d)", face->Debug().face_dx);
            CHECK(face->Debug().mood != Mood::Dizzy, "no dizziness while lying on the clock side");
            Advance(*face, 7000);
            CHECK(!face->Debug().desk_clock && clock_events_on == 0, "not before 10 s");
            Advance(*face, 3500);
            CHECK(face->Debug().desk_clock && clock_events_on == 1, "clock after ~10 s");
            CHECK(std::abs(face->Debug().face_dx) >= 320, "the face is still off the screen");
            Advance(*face, 30000);
            CHECK(face->Debug().shown_state != State::Sleep && clock_events_on == 1, "no sleep, no repeated events");
            SetPose(0, 0);
            Advance(*face, 400);
            CHECK(!face->Debug().desk_clock && clock_events_off == 1, "clock goes away when stood up");
            CHECK(face->Debug().mood == Mood::Dizzy, "'uy, me mareé' when it comes back");
            Advance(*face, 1500);
            CHECK(std::abs(face->Debug().face_dx) < 100, "the face is back");
            Advance(*face, 4500);
            CHECK(face->Debug().mood != Mood::Dizzy, "dizziness ends");

            // Picked up before 10 s: no clock, no dizziness from it.
            SetPose(90, 0);
            Advance(*face, 5000);
            SetPose(0, 0);
            Advance(*face, 400);
            CHECK(clock_events_on == 1 && !face->Debug().side_lying, "a short lie never became a clock");
            Advance(*face, 4000);

            // The other side, flat on the back or upside down: never a clock.
            SetPose(-90, 0);
            Advance(*face, 13000);
            CHECK(!face->Debug().desk_clock && !face->Debug().side_lying, "the other side is not the clock side");
            SetPose(0, 0);
            Advance(*face, 4000);
            face->SetDeskClockObserver(nullptr);
        }

        // Free fall: startle straight away.
        g_imu = {0.01f, 0.02f, 0.01f};
        Advance(*face, 80);
        CHECK(face->Debug().mood == Mood::Startled, "free fall should startle");
        SetPose(0, 0);
        Advance(*face, 3000);

        // Error wins over panic.
        face->SetState(State::Error);
        SetPose(60, 0);
        Advance(*face, 1000);
        CHECK(face->Debug().shown_state == State::Error && face->Debug().mood == Mood::Normal,
              "error should win over panic");
        SetPose(0, 0);
        face->SetState(State::Idle);
        Advance(*face, 3000);

        // ---- the IMU disappearing must not break anything -------------------------------------
        imu_on = false;
        SetPose(60, 0);
        Advance(*face, 1000);
        CHECK(face->IsRunning(), "face stopped when the IMU went away");
        imu_on = true;
        SetPose(0, 0);
        Advance(*face, 3000);

        // ---- leaning towards the viewer slumps the face down ---------------------------------
        SetPose(0, 25);
        Advance(*face, 1500);
        CHECK(face->Debug().face_dy >= 8, "pitch did not slump the face (dy %d)", face->Debug().face_dy);
        CHECK(std::abs(face->Debug().face_dx) <= 2, "pitch moved the face sideways");
        SetPose(0, 0);
        Advance(*face, 1500);

        // ---- gyroscope: nod = yes, shake = no, spin = wheee, a long spin = dizzy -------------------
        face->SetState(State::Idle);
        SetPose(0, 0);
        SetGyro(0, 0, 0);
        Advance(*face, 1500);
        CHECK(face->Debug().gyro_available, "gyroscope samples were not seen");
        CHECK(face->Debug().emotion == Emotion::None, "no emotion expected at rest");
        {
            // The test sensor has gravity on +Y, so Y is the vertical axis and X the horizontal one.
            bool happy = false;
            int squint_max = 0, mouth_w_max = 0;
            GyroSwing(*face, 0, 220, 5, 160, [&] { happy = happy || face->Debug().emotion == Emotion::Happy; });
            CHECK(happy, "nodding did not make CHIPO happy");
            Advance(*face, 500);
            Advance(*face, 400, [&] {
                squint_max = std::max(squint_max, face->Debug().eye_h);
                mouth_w_max = std::max(mouth_w_max, face->Debug().mouth_w);
            });
            CHECK(face->Debug().emotion == Emotion::Happy, "happy ended too soon");
            CHECK(squint_max <= 44, "happy eyes do not squint (%d)", squint_max);
            CHECK(mouth_w_max >= 56, "happy mouth not wide (%d)", mouth_w_max);
            Advance(*face, 3000);
            CHECK(face->Debug().emotion == Emotion::None, "happy did not end");

            bool no = false;
            GyroSwing(*face, 1, 220, 5, 160, [&] { no = no || face->Debug().emotion == Emotion::No; });
            Advance(*face, 300, [&] { no = no || face->Debug().emotion == Emotion::No; });
            CHECK(no, "shaking did not make CHIPO say no");
            Advance(*face, 3500);
            CHECK(face->Debug().emotion == Emotion::None, "no did not end");

            // A short spin: wheee, but not dizzy.
            SetGyro(0, 0, 300);
            Advance(*face, 1000);
            CHECK(face->Debug().gyro_spinning && face->Debug().emotion == Emotion::Whee,
                  "spinning did not make CHIPO whee (spin %d, emotion %d)", face->Debug().gyro_spinning,
                  static_cast<int>(face->Debug().emotion));
            SetGyro(0, 0, 0);
            Advance(*face, 1500);
            CHECK(!face->Debug().gyro_spinning && face->Debug().mood == Mood::Normal,
                  "a short spin must not leave CHIPO dizzy");
            Advance(*face, 2000);
            CHECK(face->Debug().emotion == Emotion::None, "whee did not end");

            // A long spin: dizzy afterwards, then back to normal.
            SetGyro(0, 0, -300);  // the other way round: direction does not matter
            Advance(*face, 3500);
            SetGyro(0, 0, 0);
            Advance(*face, 800);
            CHECK(face->Debug().mood == Mood::Dizzy, "a long spin should leave CHIPO dizzy");
            Advance(*face, 6000);
            CHECK(face->Debug().mood == Mood::Normal, "dizziness after the spin did not end");
            GyroOff();
            Advance(*face, 500);
            CHECK(face->Debug().emotion == Emotion::None && !face->Debug().gyro_spinning, "state after gyro off");
        }

        // ---- emotions ------------------------------------------------------------------------------
        {
            SetPose(0, 0);
            Advance(*face, 1500);
            const Emotion all[] = {Emotion::Happy,      Emotion::Laugh,    Emotion::Wink,
                                   Emotion::Love,       Emotion::Sad,      Emotion::Tired,
                                   Emotion::Suspicious, Emotion::Surprised, Emotion::Whee,
                                   Emotion::No,         Emotion::Grumpy};
            for (Emotion e : all) {
                face->ShowEmotion(e, 1500);
                Advance(*face, 1000);
                const auto d = face->Debug();
                CHECK(d.emotion == e, "emotion %d not shown (got %d)", static_cast<int>(e),
                      static_cast<int>(d.emotion));
                CHECK(d.mood == Mood::Normal && d.shown_state == State::Idle, "emotion %d changed state", static_cast<int>(e));
                CHECK(d.eye_w >= 50 && d.eye_h >= 4 && d.right_eye_h >= 4 && d.mouth_w >= 10 && d.mouth_h >= 4,
                      "emotion %d gave a broken face (%d,%d,%d,%d,%d)", static_cast<int>(e), d.eye_w, d.eye_h,
                      d.right_eye_h, d.mouth_w, d.mouth_h);
                Advance(*face, 2500);
                CHECK(face->Debug().emotion == Emotion::None, "emotion %d did not end", static_cast<int>(e));
            }

            // Shapes and colours.
            auto max_over = [&](uint32_t ms, const std::function<int(const ChipoFace::DebugInfo&)>& value) {
                int best = -9999;
                Advance(*face, ms, [&] { best = std::max(best, value(face->Debug())); });
                return best;
            };
            face->ShowEmotion(Emotion::Wink, 4000);
            Advance(*face, 600);
            CHECK(max_over(600, [](const ChipoFace::DebugInfo& d) { return d.eye_h - d.right_eye_h; }) >= 40,
                  "a wink closes only the right eye");
            face->ShowEmotion(Emotion::Suspicious, 4000);
            Advance(*face, 600);
            CHECK(max_over(600, [](const ChipoFace::DebugInfo& d) { return d.eye_h - d.right_eye_h; }) >= 15,
                  "suspicion narrows the right eye");
            face->ShowEmotion(Emotion::Surprised, 4000);
            Advance(*face, 600);
            CHECK(max_over(600, [](const ChipoFace::DebugInfo& d) { return d.eye_h; }) >= 80, "surprise opens the eyes wide");
            face->ShowEmotion(Emotion::Love, 4000);
            Advance(*face, 900);
            auto rgb = face->Debug().glow_rgb;
            CHECK(((rgb >> 16) & 0xFF) >= 0xE0 && ((rgb >> 8) & 0xFF) <= 0x90, "love should glow pink (%06X)",
                  static_cast<unsigned>(rgb));
            face->ShowEmotion(Emotion::Grumpy, 4000);
            Advance(*face, 900);
            rgb = face->Debug().glow_rgb;
            CHECK(((rgb >> 16) & 0xFF) >= 0xE0 && ((rgb >> 8) & 0xFF) <= 0x70, "grumpy should glow red (%06X)",
                  static_cast<unsigned>(rgb));
            face->ShowEmotion(Emotion::None, 0);
            Advance(*face, 2500);
            rgb = face->Debug().glow_rgb;
            CHECK(((rgb >> 16) & 0xFF) <= 0x80 && (rgb & 0xFF) >= 0xF0, "glow did not go back to blue (%06X)",
                  static_cast<unsigned>(rgb));
            CHECK(face->Debug().right_eye_h >= 60 || face->Debug().right_eye_h <= 6, "right eye not back to normal");

            // The owner's eye colour replaces the blue, but never an emotion's own colour.
            face->SetEyeTint(0xFF4FA8);
            Advance(*face, 2500);
            rgb = face->Debug().glow_rgb;
            CHECK(((rgb >> 16) & 0xFF) >= 0xE0 && ((rgb >> 8) & 0xFF) <= 0x60 && (rgb & 0xFF) >= 0x80,
                  "tinted eyes should glow pink (%06X)", static_cast<unsigned>(rgb));
            face->ShowEmotion(Emotion::Happy, 4000);
            Advance(*face, 1200);
            rgb = face->Debug().glow_rgb;
            CHECK(((rgb >> 16) & 0xFF) >= 0xE0 && ((rgb >> 8) & 0xFF) >= 0xC0 && (rgb & 0xFF) <= 0x90,
                  "happy keeps its gold (%06X)", static_cast<unsigned>(rgb));
            face->ShowEmotion(Emotion::None, 0);
            face->ClearEyeTint();
            Advance(*face, 2500);
            rgb = face->Debug().glow_rgb;
            CHECK(((rgb >> 16) & 0xFF) <= 0x80 && (rgb & 0xFF) >= 0xF0, "blue again without tint (%06X)",
                  static_cast<unsigned>(rgb));

            // While speaking the mouth keeps following the voice.
            face->SetState(State::Speaking);
            face->ShowEmotion(Emotion::Happy, 5000);
            Advance(*face, 800);
            int mouth_lo = 999, mouth_hi = 0, eye_hi = 0;
            Advance(*face, 1500, [&] {
                mouth_lo = std::min(mouth_lo, face->Debug().mouth_h);
                mouth_hi = std::max(mouth_hi, face->Debug().mouth_h);
                eye_hi = std::max(eye_hi, face->Debug().eye_h);
            });
            CHECK(mouth_hi - mouth_lo >= 10, "mouth stopped moving while speaking with an emotion (%d..%d)", mouth_lo, mouth_hi);
            CHECK(eye_hi <= 44 && face->Debug().emotion == Emotion::Happy, "emotion lost while speaking (%d)", eye_hi);
            face->SetState(State::Idle);

            // Hidden while thinking, in error, and when startled.
            face->ShowEmotion(Emotion::Happy, 8000);
            face->SetThinking(true);
            Advance(*face, 600);
            CHECK(face->Debug().shown_state == State::Thinking && face->Debug().emotion == Emotion::None,
                  "emotion shown over thinking");
            face->SetThinking(false);
            Advance(*face, 600);
            CHECK(face->Debug().emotion == Emotion::Happy, "emotion did not come back after thinking");
            face->SetState(State::Error);
            Advance(*face, 600);
            CHECK(face->Debug().shown_state == State::Error && face->Debug().emotion == Emotion::None,
                  "emotion shown over an error");
            face->SetState(State::Idle);
            face->ShowEmotion(Emotion::Love, 8000);
            SetPose(60, 0);
            Advance(*face, 600);
            CHECK(face->Debug().mood == Mood::Startled && face->Debug().emotion == Emotion::None,
                  "emotion shown over a startle");
            SetPose(0, 0);
            Advance(*face, 9000);
            CHECK(face->Debug().mood == Mood::Normal && face->Debug().emotion == Emotion::None, "did not settle after startle");

            // Names the assistant uses.
            CHECK(ChipoFace::EmotionFromName("happy") == Emotion::Happy, "happy");
            CHECK(ChipoFace::EmotionFromName("LAUGHING") == Emotion::Laugh, "LAUGHING");
            CHECK(ChipoFace::EmotionFromName("Winking") == Emotion::Wink, "Winking");
            CHECK(ChipoFace::EmotionFromName("loving") == Emotion::Love, "loving");
            CHECK(ChipoFace::EmotionFromName("crying") == Emotion::Sad, "crying");
            CHECK(ChipoFace::EmotionFromName("angry") == Emotion::Grumpy, "angry");
            CHECK(ChipoFace::EmotionFromName("sleepy") == Emotion::Tired, "sleepy");
            CHECK(ChipoFace::EmotionFromName("confused") == Emotion::Suspicious, "confused");
            CHECK(ChipoFace::EmotionFromName("shocked") == Emotion::Surprised, "shocked");
            CHECK(ChipoFace::EmotionFromName("neutral") == Emotion::None, "neutral");
            CHECK(ChipoFace::EmotionFromName("") == Emotion::None, "empty name");
            CHECK(ChipoFace::EmotionFromName(nullptr) == Emotion::None, "null name");
            CHECK(ChipoFace::EmotionFromName("happyhappyhappyhappyhappyhappy") == Emotion::None, "overlong name");
            face->ShowEmotionByName("angry", 3000);
            Advance(*face, 600);
            CHECK(face->Debug().emotion == Emotion::Grumpy, "ShowEmotionByName(angry)");
            face->ShowEmotionByName("neutral");  // unknown or neutral names leave things as they are
            face->ShowEmotionByName(nullptr);
            Advance(*face, 200);
            CHECK(face->Debug().emotion == Emotion::Grumpy, "neutral name changed the emotion");
            Advance(*face, 4000);
        }

        // ---- sleep and wake -------------------------------------------------------------------
        face->SetState(State::Listening);  // restart the idle clock
        face->SetState(State::Idle);
        Advance(*face, 119000);
        CHECK(face->Debug().shown_state == State::Idle, "fell asleep too early");
        Advance(*face, 3000);
        CHECK(face->Debug().shown_state == State::Sleep, "did not fall asleep");
        CHECK(face->Debug().eye_h <= 6, "sleeping eyes still open (%d)", face->Debug().eye_h);
        CHECK(face->Debug().step_period_ms > 40 && face->Debug().step_period_ms <= 100,
              "asleep it should step more slowly, within the 0.1 s dt clamp (%u ms)", face->Debug().step_period_ms);
        face->SetState(State::Listening);
        Advance(*face, 800);
        CHECK(face->Debug().step_period_ms == 40, "awake it must animate at 25 steps per second (%u ms)",
              face->Debug().step_period_ms);
        CHECK(face->Debug().shown_state == State::Listening && face->Debug().eye_h >= 70,
              "did not wake up");
        face->SetState(State::Idle);
        Advance(*face, 130000);
        CHECK(face->Debug().shown_state == State::Sleep, "did not sleep again");
        g_imu = {0.0f, 0.1f, 0.9f};  // knocked over: a jolt wakes it
        Advance(*face, 200);
        CHECK(face->Debug().shown_state != State::Sleep, "a jolt should wake CHIPO");
        CHECK(face->Debug().step_period_ms == 40, "after the jolt it animates at full rate (%u ms)", face->Debug().step_period_ms);
        SetPose(0, 0);
        Advance(*face, 3000);

        // ---- the real LVGL timer path --------------------------------------------------------
        for (int i = 0; i < 100; ++i) {
            g_now += 40;
            lv_timer_handler();
        }

        // Stop in the middle of an animation, twice, then destroy.
        face->Stop();
        face->Stop();
        CHECK(!face->IsRunning(), "still running after Stop");
        lv_timer_handler();
        delete face;

        // ---- idle winks, sleepiness before sleeping, emotions wake CHIPO (a face with no sensor) -----
        {
            g_now += 1000;
            ChipoFace q;
            CHECK(q.Start(lv_screen_active()), "second face did not start");
            q.SetIdleQuirks(false);
            bool any = false;
            Advance(q, 95000, [&] { any = any || q.Debug().emotion != Emotion::None; });
            CHECK(!any, "an emotion appeared by itself with idle quirks off");
            Advance(q, 10000);
            CHECK(q.Debug().emotion == Emotion::Tired && q.Debug().shown_state == State::Idle,
                  "CHIPO should look sleepy before falling asleep (emotion %d)", static_cast<int>(q.Debug().emotion));
            Advance(q, 18000);
            CHECK(q.Debug().shown_state == State::Sleep && q.Debug().emotion == Emotion::None, "did not fall asleep");
            q.ShowEmotion(Emotion::Happy, 2000);
            Advance(q, 600);
            CHECK(q.Debug().shown_state == State::Idle && q.Debug().emotion == Emotion::Happy,
                  "an emotion should wake CHIPO up");
            q.Stop();
        }
        {
            g_now += 1000;
            ChipoFace q;
            CHECK(q.Start(lv_screen_active()), "third face did not start");
            int seen = 0;
            bool odd = false;
            Emotion last = Emotion::None;
            Advance(q, 95000, [&] {
                const Emotion e = q.Debug().emotion;
                if (e != Emotion::None && last == Emotion::None) ++seen;
                if (e != Emotion::None && e != Emotion::Happy && e != Emotion::Wink) odd = true;
                last = e;
            });
            CHECK(seen >= 1 && seen <= 4, "idle quirks seen in 95 s: %d", seen);
            CHECK(!odd, "idle quirks must only be winks and smiles");
            Advance(q, 30000);
            CHECK(q.Debug().shown_state == State::Sleep, "idle quirks kept CHIPO awake");
            q.Stop();
        }
    }

    // ---- Lote A: finger gaze, hearts and sparkles, slow loving blink, yawn and stretch ---------------------------------
    {
        std::printf("== gaze, hearts, sparkles, slow blink, stretch\n");
        g_now += 1000;
        ChipoFace f;
        f.SetIdleQuirks(false);
        CHECK(f.Start(lv_screen_active()), "fx face did not start");
        Advance(f, 400);
        CHECK(!f.Debug().gaze_active, "gaze active without a finger");
        Advance(f, 800, [&] { f.LookAt(1.0f, 0.0f); });
        CHECK(f.Debug().gaze_active, "gaze not active while touched");
        CHECK(f.Debug().gaze_px_x > 10.0f, "pupils did not follow the finger: %.1f", f.Debug().gaze_px_x);
        Advance(f, 600, [&] { f.LookAt(-1.0f, -1.0f); });
        CHECK(f.Debug().gaze_px_x < -10.0f && f.Debug().gaze_px_y < -6.0f, "gaze did not move to the other corner");
        f.LookAt(5.0f, 5.0f);  // out of range is clamped
        Advance(f, 400);
        CHECK(f.Debug().gaze_px_x <= 14.5f && f.Debug().gaze_px_y <= 9.5f, "gaze not clamped");
        Advance(f, 1500);
        CHECK(!f.Debug().gaze_active, "gaze kept following after the finger left");
        CHECK(std::fabs(f.Debug().gaze_px_x) < 1.0f, "pupils did not come back: %.1f", f.Debug().gaze_px_x);
        // StopLookAt lets go at once.
        f.LookAt(1, 0);
        Advance(f, 80);
        f.StopLookAt();
        Advance(f, 80);
        CHECK(!f.Debug().gaze_active, "StopLookAt did not let go");
        // Not while startled: the fright wins.
        // Hearts and sparkles fade out by themselves.
        f.Celebrate(ChipoFace::Burst::Hearts, 5);
        Advance(f, 120);
        CHECK(f.Debug().fx_hearts >= 3, "hearts did not appear: %d", f.Debug().fx_hearts);
        Advance(f, 1200);
        CHECK(f.Debug().slow_blinks >= 1, "hearts did not bring a slow loving blink");
        Advance(f, 3500);
        CHECK(f.Debug().fx_hearts == 0, "hearts never faded: %d", f.Debug().fx_hearts);
        f.Celebrate(ChipoFace::Burst::Sparkles, 6);
        Advance(f, 120);
        CHECK(f.Debug().fx_sparkles >= 3, "sparkles did not appear: %d", f.Debug().fx_sparkles);
        Advance(f, 3000);
        CHECK(f.Debug().fx_sparkles == 0, "sparkles never faded");
        f.Celebrate(ChipoFace::Burst::Hearts, 50);  // more than the pool: just fills it
        Advance(f, 40);
        CHECK(f.Debug().fx_hearts <= FxEngine::kMax, "pool overflow");
        // Yawn and stretch: once at the start of the tired look and again at every yawn.
        Advance(f, 5000);
        const int before = f.Debug().stretches;
        f.ShowEmotion(Emotion::Tired, 11000);
        Advance(f, 200);
        CHECK(f.Debug().stretches == before + 1, "no stretch at the first yawn (%d -> %d)", before, f.Debug().stretches);
        Advance(f, 5400);
        CHECK(f.Debug().stretches == before + 2, "no stretch at the second yawn (%d)", f.Debug().stretches);
        f.Stop();
        f.Celebrate(ChipoFace::Burst::Hearts);  // after Stop: harmless
        f.LookAt(0, 0);
    }
    {
        // Idle slow blinks appear by themselves (and are only blinks, not emotions).
        g_now += 1000;
        ChipoFace q;
        CHECK(q.Start(lv_screen_active()), "slow blink face did not start");
        q.SetIdleQuirks(true);
        Advance(q, 95000);
        CHECK(q.Debug().slow_blinks >= 1, "no slow loving blink in 95 s of idle");
        CHECK(q.Debug().slow_blinks <= 3, "too many slow blinks: %d", q.Debug().slow_blinks);
        q.Stop();
    }

    // ---- Lote B: tears, sleep with Zzz, costumes, tantrum -----------------------------------------------------------
    {
        std::printf("== tears, tantrum, zzz, costumes\n");
        g_now += 1000;
        ChipoFace f;
        f.SetIdleQuirks(false);
        CHECK(f.Start(lv_screen_active()), "lote B face did not start");
        Advance(f, 400);
        CHECK(!f.Debug().crying, "crying by itself");
        f.Cry(3000);
        Advance(f, 1500);
        CHECK(f.Debug().crying && f.Debug().emotion == Emotion::Sad, "no sad face while crying");
        CHECK(f.Debug().fx_tears >= 1, "no tears running");
        Advance(f, 2500);
        CHECK(!f.Debug().crying && f.Debug().fx_tears == 0, "tears never stopped");
        // Sore loser: angry first, then tears.
        f.SoreLoser();
        Advance(f, 400);
        CHECK(f.Debug().tantrum && f.Debug().emotion == Emotion::Grumpy, "no tantrum");
        bool shook = false;
        for (int i = 0; i < 40; ++i) { Advance(f, 40); shook = shook || std::abs(f.Debug().face_dx) >= 3; }
        CHECK(shook, "the face did not shake during the tantrum");
        Advance(f, 1800);
        CHECK(!f.Debug().tantrum && f.Debug().crying, "the tantrum did not end in tears");
        Advance(f, 4000);
        CHECK(!f.Debug().crying, "tears after the tantrum never ended");
        // Costumes by voice and by date.
        Advance(f, 2000);
        CHECK(f.Debug().costume == Costume::None, "costume out of nowhere");
        f.SetCostume(Costume::SantaHat);
        Advance(f, 100);
        CHECK(f.Debug().costume == Costume::SantaHat && f.Debug().fx_sparkles >= 1, "hat not put on with sparkles");
        f.SetCostume(Costume::Sunglasses);
        Advance(f, 1500);
        CHECK(f.Debug().glasses_progress > 0.9f && !f.Debug().drive_mode, "sunglasses did not slide on as a costume");
        f.SetCostume(Costume::None);
        Advance(f, 1500);
        CHECK(f.Debug().glasses_progress < 0.05f && f.Debug().costume == Costume::None, "costume not taken off");
        f.Stop();
    }
    {
        // The calendar: Santa hat in December unless the owner took it off.
        g_now += 1000;
        ChipoFace f;
        f.SetIdleQuirks(false);
        int month = 12, day = 24;
        bool clock_ok = false;
        f.SetDateSource([&](int* m, int* d) { if (!clock_ok) return false; *m = month; *d = day; return true; });
        CHECK(f.Start(lv_screen_active()), "date face did not start");
        Advance(f, 1000);
        CHECK(f.Debug().costume == Costume::None, "hat before the clock is set");
        clock_ok = true;
        Advance(f, 61000);
        CHECK(f.Debug().costume == Costume::SantaHat, "no Santa hat on 24 Dec");
        month = 3; day = 3;
        Advance(f, 61000);
        CHECK(f.Debug().costume == Costume::None, "hat kept in March");
        month = 12; day = 25;
        Advance(f, 61000);
        CHECK(f.Debug().costume == Costume::SantaHat, "hat missing on 25 Dec");
        f.SetCostume(Costume::None);  // "quitate el gorro": stays off
        Advance(f, 61000);
        CHECK(f.Debug().costume == Costume::None, "the owner's choice was overridden by the date");
        f.Stop();
    }
    {
        // Asleep: Zzz float up, breathing moves the face a little, they stop when CHIPO wakes up.
        g_now += 1000;
        ChipoFace f;
        CHECK(f.Start(lv_screen_active()), "sleep face did not start");
        f.SetIdleQuirks(false);
        Advance(f, 130000);
        CHECK(f.Debug().shown_state == State::Sleep, "did not fall asleep");
        Advance(f, 3000);
        CHECK(f.Debug().fx_zzz >= 1 && f.Debug().fx_zzz <= 3, "no Zzz while asleep: %d", f.Debug().fx_zzz);
        int lo = 99, hi = -99;
        Advance(f, 4200, [&] { lo = std::min(lo, f.Debug().face_dy); hi = std::max(hi, f.Debug().face_dy); });
        CHECK(hi - lo >= 2, "no breathing movement (%d..%d)", lo, hi);
        CHECK(hi - lo <= 6, "breathing too big (%d..%d)", lo, hi);
        f.SetState(State::Listening);
        Advance(f, 6000);
        CHECK(f.Debug().shown_state == State::Listening && f.Debug().fx_zzz == 0, "Zzz kept floating after waking up");
        f.Stop();
    }

    // ---- Lote C: dance ---------------------------------------------------------------------------------------------
    {
        std::printf("== dance\n");
        g_now += 1000;
        ChipoFace f;
        f.SetIdleQuirks(false);
        float level = 0;
        f.SetLevelSource([&] { return level; });
        CHECK(f.Start(lv_screen_active()), "dance face did not start");
        Advance(f, 400);
        CHECK(!f.dancing() && !f.Debug().dancing, "dancing by itself");
        f.SetDance(true);
        Advance(f, 300);
        CHECK(f.dancing() && f.Debug().emotion == Emotion::Dance, "no dance face");
        // Silence: sways gently, no beat hops.
        int lo = 999, hi = -999;
        Advance(f, 4000, [&] { lo = std::min(lo, f.Debug().face_dx); hi = std::max(hi, f.Debug().face_dx); });
        CHECK(hi - lo >= 20, "no sway in silence (%d..%d)", lo, hi);
        CHECK(f.Debug().dance_hops == 0, "hopped without sound (%d)", f.Debug().dance_hops);
        const int mouth_quiet = f.Debug().mouth_h;
        // A beat: loud bursts 500 ms apart.
        int hops0 = f.Debug().dance_hops;
        for (int beat = 0; beat < 8; ++beat) {
            level = 0.9f; Advance(f, 160);
            level = 0.05f; Advance(f, 360);
        }
        CHECK(f.Debug().dance_hops - hops0 >= 5, "hops follow the beat (%d)", f.Debug().dance_hops - hops0);
        level = 0.9f;
        Advance(f, 400);
        CHECK(f.Debug().mouth_h > mouth_quiet + 8, "mouth did not open with the sound (%d -> %d)", mouth_quiet, f.Debug().mouth_h);
        CHECK(f.Debug().dance_level > 0.6f, "level not followed");
        // Steady loud sound is not a beat: at most the first hop.
        level = 0.9f;
        Advance(f, 6000);
        hops0 = f.Debug().dance_hops;
        Advance(f, 3000);
        CHECK(f.Debug().dance_hops - hops0 <= 1, "hopped on a steady sound (%d)", f.Debug().dance_hops - hops0);
        // Glow colour cycles.
        uint32_t c0 = f.Debug().glow_rgb, c1 = c0;
        Advance(f, 1200);
        c1 = f.Debug().glow_rgb;
        CHECK(c0 != c1, "glow colour does not change while dancing");
        // Stops on command and CHIPO goes back to its normal face.
        f.SetDance(false);
        level = 0;
        Advance(f, 1500);
        CHECK(!f.dancing() && f.Debug().emotion == Emotion::None, "still dancing after stop");
        // Stays awake while dancing, ends by itself after 3 minutes.
        f.SetDance(true);
        Advance(f, 150000);
        CHECK(f.Debug().shown_state != State::Sleep && f.dancing(), "fell asleep dancing");
        Advance(f, 40000);
        CHECK(!f.dancing(), "dance did not end after 3 minutes");
        f.Stop();
        f.SetDance(true);  // after Stop: harmless
    }

    if (g_failures == 0) std::printf("ALL FACE TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
