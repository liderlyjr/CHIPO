// Host-side test of CHIPO DRIVE on a real LVGL build: voice activation, sunglasses animation,
// reactions only inside the mode, desktop behaviour unchanged. Simulated drives only.
#include <cmath>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

#include "chipo_drive.h"

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

// ---- fake car: gravity on +Y, forward along +Z, car-left along +X ----------------------------------
static std::deque<ImuSample> g_queue;
static uint32_t g_sample_t = 50000;
static uint32_t g_rng = 7;
static float Noise(float a) {
    g_rng = g_rng * 1664525u + 1013904223u;
    return (static_cast<float>((g_rng >> 8) & 0xFFFF) / 32768.0f - 1.0f) * a;
}
static void PushSamples(uint32_t ms, float fwd, float left, float yaw_dps, float up = 0) {
    for (uint32_t t = 0; t < ms; t += 20) {
        g_sample_t += 20;
        ImuSample s;
        s.has_gyro = true;
        s.t_ms = g_sample_t;
        s.x = left + Noise(0.01f);
        s.y = 1.0f + up + Noise(0.01f);
        s.z = fwd + Noise(0.01f);
        s.gx = Noise(0.5f);
        s.gy = yaw_dps + Noise(0.5f);
        s.gz = Noise(0.5f);
        g_queue.push_back(s);
    }
}
static bool Source(ImuSample* out) {
    if (g_queue.empty()) return false;
    *out = g_queue.front();
    g_queue.pop_front();
    return true;
}
static void PushTilted(uint32_t ms, float roll_deg) {  // CHIPO leaning over (held in a hand)
    for (uint32_t t = 0; t < ms; t += 20) {
        g_sample_t += 20;
        ImuSample s;
        s.has_gyro = true;
        s.t_ms = g_sample_t;
        s.x = std::sin(roll_deg * 0.0174533f);
        s.y = std::cos(roll_deg * 0.0174533f);
        g_queue.push_back(s);
    }
}

struct Harness {
    ChipoFace face;
    ChipoDrive drive;
    std::vector<std::string> clips;
    std::vector<std::string> notes;
    bool clip_exists = true;
    std::string only_clip;  // if set, only this clip exists
    bool busy = false;
    int emotions_seen_happy = 0, emotions_seen_surprised = 0;
    int min_dx = 0, max_dx = 0;
    bool saw_fell = false;
    bool saw_dizzy = false;

    void Start() {
        face.SetIdleQuirks(false);
        face.SetTiltSource(Source);
        CHECK(face.Start(lv_screen_active()), "face starts");
        ChipoDrive::Hooks h;
        h.play_clip = [this](const char* id) {
            const bool ok = clip_exists && (only_clip.empty() || only_clip == id);
            if (ok) clips.push_back(id);
            return ok;
        };
        h.notify = [this](const char* text) { notes.push_back(text); };
        h.assistant_busy = [this]() { return busy; };
        h.clock = []() { return g_now; };
        CHECK(drive.Start(&face, h), "drive starts");
    }
    void Run(uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += 40) {
            g_now += 40;
            face.Step(g_now);
            drive.Step(g_now);
            const auto d = face.Debug();
            if (d.emotion == ChipoFace::Emotion::Happy) ++emotions_seen_happy;
            if (d.emotion == ChipoFace::Emotion::Surprised) ++emotions_seen_surprised;
            if (d.fell) saw_fell = true;
            if (d.mood == ChipoFace::Mood::Dizzy) saw_dizzy = true;
            if (d.face_dx < min_dx) min_dx = d.face_dx;
            if (d.face_dx > max_dx) max_dx = d.face_dx;
        }
    }
    // Simulated trip: stand still, pull away, cruise, brake, cruise, left curve, bump.
    void Trip() {
        PushSamples(3000, 0, 0, 0);          Run(3000);
        PushSamples(2000, 0.25f, 0, 0);      Run(2000);
        PushSamples(4000, 0, 0, 0);          Run(4000);
        PushSamples(1500, -0.30f, 0, 0);     Run(1500);
        PushSamples(3000, 0, 0, 0);          Run(3000);
        const float yaw = (0.22f * 9.81f / 12.0f) * 57.29578f;
        PushSamples(4000, 0, 0.22f, yaw);    Run(4000);
        PushSamples(3000, 0, 0, 0);          Run(3000);
        PushSamples(100, 0, 0, 0, 0.55f);    Run(100);
        PushSamples(1500, 0, 0, 0);          Run(1500);
    }
};

int main() {
    lv_init();
    lv_tick_set_cb(FakeTick);
    lv_display_t* d = lv_display_create(368, 448);
    lv_display_set_buffers(d, g_buf, nullptr, sizeof(g_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, Flush);

    // ---- desktop mode: a whole trip changes nothing -------------------------------------------------
    {
        std::printf("== desktop mode ignores driving movements\n");
        Harness h;
        h.Start();
        h.Run(200);
        CHECK(!h.drive.active() && !h.face.DriveMode(), "starts in desktop mode");
        CHECK(h.face.Debug().glasses_progress == 0.0f, "no glasses before the mode starts");
        h.Trip();
        CHECK(h.clips.empty() && h.notes.empty(), "no drive sounds or texts in desktop mode");
        CHECK(h.emotions_seen_happy == 0 && h.emotions_seen_surprised == 0, "no driving expressions in desktop mode (%d,%d)", h.emotions_seen_happy, h.emotions_seen_surprised);
        CHECK(h.face.Debug().glasses_progress == 0.0f, "still no glasses");
        // The existing tilt reaction still works in desktop mode: lean CHIPO right over.
        PushTilted(2500, 70.0f);
        h.Run(2500);
        CHECK(h.saw_fell, "desktop mode: tipping CHIPO over still makes it slip (fall reaction kept)");
        g_queue.clear();
    }

    // ---- voice activation, sunglasses, reactions, voice deactivation -----------------------------------------
    {
        std::printf("== car mode\n");
        g_queue.clear();
        Harness h;
        h.Start();
        h.Run(1000);
        CHECK(!h.drive.OnVoiceText("Chipo, qué tiempo hace"), "ordinary speech is not a command");
        CHECK(!h.drive.active(), "ordinary speech does not start the mode");

        CHECK(h.drive.OnVoiceText("Chipo, vámonos"), "activation answered by CHIPO itself (clip played)");
        CHECK(h.drive.active() && h.face.DriveMode(), "mode is on");
        CHECK(!h.clips.empty() && h.clips[0] == "drive_start", "brum-brum line played on activation");
        CHECK(!h.drive.OnVoiceText("vámonos"), "saying it again while on is left to the assistant");

        h.Run(40);
        const float first = h.face.Debug().glasses_progress;
        CHECK(first > 0.0f && first < 0.15f, "glasses do not appear at once (%.2f)", first);
        h.Run(400);
        const float mid = h.face.Debug().glasses_progress;
        CHECK(mid > first && mid < 0.9f, "glasses slide progressively (%.2f)", mid);
        h.Run(1200);
        CHECK(h.face.Debug().glasses_progress >= 0.999f, "glasses end on the eyes");
        h.Run(30000);
        CHECK(h.face.Debug().glasses_progress >= 0.999f, "and stay while driving");

        h.min_dx = h.max_dx = 0;
        h.Trip();
        CHECK(h.emotions_seen_happy > 0, "acceleration makes CHIPO excited");
        CHECK(h.emotions_seen_surprised > 0, "braking / bump makes CHIPO surprised");
        CHECK(h.min_dx < -15, "left curve leans the face (%d..%d)", h.min_dx, h.max_dx);
        CHECK(h.drive.motion().forward_known, "forward learned during the trip");

        // In the car, holding the device far over does not trigger the desktop fall.
        h.saw_fell = false;
        h.saw_dizzy = false;
        PushTilted(2500, 70.0f);
        h.Run(2500);
        CHECK(!h.saw_fell && !h.saw_dizzy, "no fall / dizzy panic in car mode");
        g_queue.clear();

        // Quiet while the assistant is talking.
        const size_t clips_before = h.clips.size();
        h.busy = true;
        for (int i = 0; i < 6; ++i) {
            PushSamples(100, 0, 0, 0, 0.7f);
            h.Run(100);
            PushSamples(4000, 0, 0, 0);
            h.Run(4000);
        }
        CHECK(h.clips.size() == clips_before, "no comments while the assistant is busy");
        h.busy = false;

        CHECK(h.drive.OnVoiceText("Chipo, sensibilidad suave"), "sensitivity command acknowledged");
        CHECK(h.drive.sensitivity() == DriveSensitivity::Soft, "sensitivity changed");
        CHECK(h.drive.OnVoiceText("sensibilidad intensa") && h.drive.sensitivity() == DriveSensitivity::Intense, "intense");

        CHECK(h.drive.OnVoiceText("Chipo, desactiva el modo conducción"), "deactivation answered by CHIPO");
        CHECK(!h.drive.active() && !h.face.DriveMode(), "mode is off");
        CHECK(h.clips.back() == "drive_end", "goodbye line played");
        h.Run(40);
        CHECK(h.face.Debug().glasses_progress < 1.0f, "glasses start sliding off");
        h.Run(1500);
        CHECK(h.face.Debug().glasses_progress == 0.0f, "glasses are gone, desktop appearance back");
        CHECK(!h.drive.OnVoiceText("desactiva el modo conducción"), "deactivating while off is ignored");

        // Back in desktop mode nothing reacts again.
        const size_t clips_after = h.clips.size();
        h.emotions_seen_happy = h.emotions_seen_surprised = 0;
        h.Trip();
        CHECK(h.clips.size() == clips_after, "no comments after leaving the mode");
        CHECK(h.emotions_seen_happy == 0, "no driving expressions after leaving the mode");
        g_queue.clear();
    }

    // ---- missing clips: text instead, and the server keeps its answer -----------------------------------------
    {
        std::printf("== no clips recorded\n");
        Harness h;
        h.clip_exists = false;
        h.Start();
        h.Run(500);
        CHECK(!h.drive.OnVoiceText("Chipo, vámonos"), "without a clip the assistant is not silenced");
        CHECK(h.drive.active(), "but the mode is on");
        CHECK(!h.notes.empty() && h.notes[0].find("Brum") != std::string::npos, "text shown instead of the sound");
        h.drive.Stop();  // Stop while the mode is on (leak / dangling check under ASan)
        CHECK(!h.drive.active(), "stopped");
    }

    // ---- only the engine sound exists: it plays, the assistant keeps its voice -----------------------------
    {
        std::printf("== engine sound only\n");
        Harness h;
        h.only_clip = "drive_start_sfx";
        h.Start();
        h.Run(500);
        CHECK(!h.drive.OnVoiceText("Chipo, vámonos"), "engine sound only: the assistant answers by voice");
        CHECK(h.clips.size() == 1 && h.clips[0] == "drive_start_sfx", "the engine sound played");
        CHECK(!h.notes.empty(), "and the text is shown");
        h.Run(1500);
        CHECK(h.face.Debug().glasses_progress >= 0.999f, "glasses on");
    }

    if (g_failures == 0) std::printf("ALL DRIVE CONTROLLER TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
