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
static uint8_t g_buf[368 * 40 * 4];
static void Flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

using State = ChipoFace::State;
using Mood = ChipoFace::Mood;
constexpr float kDeg = 3.14159265f / 180.0f;

// Fake accelerometer: the sensor is mounted with gravity on +Y when CHIPO stands upright.
static ImuSample g_imu{0, 1, 0};
static bool g_imu_new = false;
static void SetPose(float roll_deg, float pitch_deg) {
    g_imu = {std::sin(roll_deg * kDeg) * std::cos(pitch_deg * kDeg),
             std::cos(roll_deg * kDeg) * std::cos(pitch_deg * kDeg), std::sin(pitch_deg * kDeg)};
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

int main() {
    lv_init();
    lv_tick_set_cb(FakeTick);
    lv_display_t* d = lv_display_create(368, 448);
    lv_display_set_buffers(d, g_buf, nullptr, sizeof(g_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, Flush);

    for (int round = 0; round < 2; ++round) {
        std::printf("== round %d\n", round);
        g_now += 1000;
        SetPose(0, 0);
        auto* face = new ChipoFace();
        bool imu_on = true;
        face->SetTiltSource([&](ImuSample* s) {
            if (!imu_on || !g_imu_new) return false;
            g_imu_new = false;
            *s = g_imu;
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
        Advance(*face, 800, [&] {
            const int h = face->Debug().eye_h;
            if (h < 68 || prev < 68) { prev = h; return; }  // ignore blinks (closing, and the step that reopens); open eyes are 70+
            biggest_jump = std::max(biggest_jump, std::abs(h - prev));
            prev = h;
        });
        grew = face->Debug().eye_h >= 76;
        CHECK(grew, "listening eyes did not grow (%d)", face->Debug().eye_h);
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
        SetPose(80, 0);
        Advance(*face, 900);
        CHECK(face->Debug().face_dx * side >= 320, "should have fallen before lying down");
        Advance(*face, 3500);
        CHECK(face->Debug().mood == Mood::Dizzy && std::abs(face->Debug().face_dx) < 100,
              "lying flat should bring it back dizzy (dx %d)", face->Debug().face_dx);
        SetPose(0, 0);
        Advance(*face, 3500);
        CHECK(face->Debug().mood == Mood::Normal && std::abs(face->Debug().face_dx) <= 1, "did not recover from lying");

        // Lying down for a while: dizzy, face re-centred.
        SetPose(90, 0);
        Advance(*face, 4000);
        CHECK(face->Debug().mood == Mood::Dizzy, "not dizzy when lying");
        CHECK(face->Debug().eye_h <= 20, "dizzy eyes too open (%d)", face->Debug().eye_h);
        SetPose(0, 0);
        Advance(*face, 3000);
        CHECK(face->Debug().mood == Mood::Normal, "dizziness did not end");

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

        // ---- sleep and wake -------------------------------------------------------------------
        face->SetState(State::Listening);  // restart the idle clock
        face->SetState(State::Idle);
        Advance(*face, 119000);
        CHECK(face->Debug().shown_state == State::Idle, "fell asleep too early");
        Advance(*face, 3000);
        CHECK(face->Debug().shown_state == State::Sleep, "did not fall asleep");
        CHECK(face->Debug().eye_h <= 6, "sleeping eyes still open (%d)", face->Debug().eye_h);
        face->SetState(State::Listening);
        Advance(*face, 800);
        CHECK(face->Debug().shown_state == State::Listening && face->Debug().eye_h >= 70,
              "did not wake up");
        face->SetState(State::Idle);
        Advance(*face, 130000);
        CHECK(face->Debug().shown_state == State::Sleep, "did not sleep again");
        g_imu = {0.0f, 0.1f, 0.9f};  // knocked over: a jolt wakes it
        Advance(*face, 200);
        CHECK(face->Debug().shown_state != State::Sleep, "a jolt should wake CHIPO");
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
    }

    if (g_failures == 0) std::printf("ALL FACE TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
