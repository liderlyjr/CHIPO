// CHIPO FUN on a real LVGL build: start-up screen, lamp, dice, coin. Simulated time.
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "chipo_fun.h"

static int g_failures = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_failures; std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static uint32_t g_now = 1000;
static uint32_t FakeTick() { return g_now; }
static uint8_t g_buf[368 * 40 * 4];
static void Flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

struct Rig {
    ChipoFun fun;
    std::vector<std::string> clips, notes;
    bool clips_exist = true;
    bool Start() {
        ChipoFun::Hooks h;
        h.play_clip = [this](const char* id) { if (!clips_exist) return false; clips.push_back(id); return true; };
        h.notify = [this](const char* t) { notes.push_back(t); };
        h.clock = [] { return g_now; };
        return fun.Start(lv_screen_active(), std::move(h), "0.6.0");
    }
    void Run(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 50) { g_now += 50; fun.Step(g_now); lv_timer_handler(); } }
    // Logic only, no rendering: for long simulated stretches.
    void Skip(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 50) { g_now += 50; fun.Step(g_now); } lv_timer_handler(); }
    bool Played(const char* id) const { for (auto& c : clips) if (c == id) return true; return false; }
};

int main() {
    lv_init();
    lv_tick_set_cb(FakeTick);
    lv_rand_set_seed(1234);
    lv_display_t* d = lv_display_create(368, 448);
    lv_display_set_buffers(d, g_buf, nullptr, sizeof(g_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, Flush);

    std::printf("== start-up screen\n");
    {
        Rig r;
        CHECK(r.Start(), "start");
        CHECK(r.fun.mode() == ChipoFun::Mode::Off, "nothing at first");
        r.fun.ShowBoot();
        CHECK(r.fun.mode() == ChipoFun::Mode::Boot && r.Played("fun_boot"), "boot screen and sound");
        CHECK(!r.fun.OnVoiceText("tira un dado"), "voice ignored during the start-up screen");
        r.Run(600);
        CHECK(r.fun.Debug().boot_alpha > 0.9f, "fully lit in the middle (%.2f)", r.fun.Debug().boot_alpha);
        r.Run(1200);
        CHECK(r.fun.mode() == ChipoFun::Mode::Boot, "still there at 1.8 s");
        r.Run(1000);
        CHECK(r.fun.mode() == ChipoFun::Mode::Off, "gone after ~2.6 s");
        r.fun.ShowBoot();
        r.Run(300);
        r.fun.OnTouch();
        CHECK(r.fun.mode() == ChipoFun::Mode::Off, "a touch skips it");
    }

    std::printf("== start-up screen hands over to the face\n");
    {
        int woke = 0;
        ChipoFun fun;
        ChipoFun::Hooks h;
        h.play_clip = [](const char*) { return true; };
        h.notify = [](const char*) {};
        h.boot_done = [&woke] { ++woke; };
        h.clock = [] { return g_now; };
        fun.Start(lv_screen_active(), std::move(h), "0.6.0");
        fun.ShowBoot();
        for (uint32_t t = 0; t < 3000; t += 50) { g_now += 50; fun.Step(g_now); }
        CHECK(woke == 1, "the face wakes up when the start-up screen ends (%d)", woke);
        fun.ShowBoot();
        g_now += 200;
        fun.OnTouch();
        CHECK(woke == 2, "also when a touch skips it");
        fun.OnTouch();
        CHECK(woke == 2, "a touch with nothing on screen does nothing");
    }

    std::printf("== lamp\n");
    {
        Rig r;
        r.Start();
        CHECK(!r.fun.OnVoiceText("apaga la luz"), "no lamp on: 'apaga la luz' is left to others");
        CHECK(r.fun.OnVoiceText("Chipo, luz cálida"), "warm lamp");
        CHECK(r.fun.mode() == ChipoFun::Mode::Lamp && r.fun.Debug().lamp == LampColor::Warm, "lamp on, warm");
        r.Run(500);
        CHECK(r.fun.OnVoiceText("luz azul") && r.fun.Debug().lamp == LampColor::Cool, "switch to cool");
        CHECK(r.fun.OnVoiceText("arcoíris") && r.fun.Debug().lamp == LampColor::Rainbow, "rainbow");
        std::set<uint32_t> colours;
        for (int i = 0; i < 80; ++i) { r.Run(100); colours.insert(r.fun.Debug().lamp_rgb); }
        CHECK(colours.size() >= 10, "the rainbow keeps changing (%zu colours)", colours.size());
        CHECK(r.fun.OnVoiceText("apaga la lámpara"), "voice off");
        CHECK(r.fun.mode() == ChipoFun::Mode::Off, "off");
        r.fun.OnVoiceText("luz verde");
        r.fun.OnTouch();
        CHECK(r.fun.mode() == ChipoFun::Mode::Off, "a touch turns it off");
        r.fun.OnVoiceText("modo lámpara");
        r.Skip(29u * 60u * 1000u);
        CHECK(r.fun.mode() == ChipoFun::Mode::Lamp, "still on at 29 min");
        r.Skip(90u * 1000u);
        CHECK(r.fun.mode() == ChipoFun::Mode::Off && !r.notes.empty(), "switches itself off at 30 min");
    }

    std::printf("== dice\n");
    {
        Rig r;
        r.Start();
        CHECK(r.fun.OnVoiceText("tira un dado") && r.Played("fun_roll"), "roll with sound");
        CHECK(r.fun.mode() == ChipoFun::Mode::Dice && r.fun.Debug().rolling && r.fun.Debug().dice_count == 1, "rolling one die");
        std::set<int> seen_while_rolling;
        for (int i = 0; i < 20; ++i) { r.Run(50); seen_while_rolling.insert(r.fun.Debug().dice_values[0]); }
        CHECK(seen_while_rolling.size() >= 3, "the faces shuffle while rolling");
        r.Run(600);
        CHECK(!r.fun.Debug().rolling, "stopped");
        const int v = r.fun.Debug().dice_values[0];
        CHECK(v >= 1 && v <= 6, "result 1..6 (%d)", v);
        CHECK(!r.notes.empty() && r.notes.back().find(std::to_string(v)) != std::string::npos, "text says the number (%s)", r.notes.empty() ? "-" : r.notes.back().c_str());
        r.Run(8000);
        CHECK(r.fun.mode() == ChipoFun::Mode::Off, "closes by itself");

        r.fun.OnVoiceText("lanza dos dados");
        r.Run(1400);
        CHECK(r.fun.Debug().dice_count == 2, "two dice");
        const int a = r.fun.Debug().dice_values[0], b = r.fun.Debug().dice_values[1];
        CHECK(a >= 1 && a <= 6 && b >= 1 && b <= 6, "both in range (%d,%d)", a, b);
        CHECK(r.notes.back().find(std::to_string(a + b)) != std::string::npos, "sum shown (%s)", r.notes.back().c_str());
        r.fun.OnTouch();

        std::set<int> all;
        for (int i = 0; i < 300; ++i) { r.fun.OnVoiceText("tira un dado"); r.Skip(1400); all.insert(r.fun.Debug().dice_values[0]); r.fun.OnTouch(); }
        CHECK(all.size() == 6, "all six faces come up over 300 rolls (%zu)", all.size());
    }

    std::printf("== eye colour\n");
    {
        int64_t last = -2;
        int calls = 0;
        ChipoFun fun;
        ChipoFun::Hooks h;
        std::vector<std::string> clips;
        h.play_clip = [&clips](const char* id) { clips.push_back(id); return true; };
        h.notify = [](const char*) {};
        h.set_eye_color = [&](int64_t rgb) { last = rgb; ++calls; };
        h.clock = [] { return g_now; };
        fun.Start(lv_screen_active(), std::move(h), "0.6.0");
        CHECK(fun.OnVoiceText("Chipo, pon los ojos rosas") && last == 0xFF4FA8, "pink eyes asked (%lld)", static_cast<long long>(last));
        CHECK(!clips.empty() && clips.back() == "buddy_chime", "with a chime");
        CHECK(fun.OnVoiceText("ojos verdes") && last == 0x39FF88, "green eyes");
        CHECK(fun.OnVoiceText("ojos normales") && last == -1, "back to normal");
        CHECK(calls == 3 && fun.eye_changes() == 3, "three changes (%d)", calls);
        CHECK(!fun.OnVoiceText("me duelen los ojos") && calls == 3, "talking about eyes is not a command");
        CHECK(fun.mode() == ChipoFun::Mode::Off, "no overlay is opened");
    }

    std::printf("== coin\n");
    {
        Rig r;
        r.Start();
        CHECK(r.fun.OnVoiceText("cara o cruz") && r.Played("fun_coin"), "flip with sound");
        CHECK(r.fun.mode() == ChipoFun::Mode::Coin && r.fun.Debug().rolling, "flipping");
        r.Skip(1700);
        CHECK(!r.fun.Debug().rolling && !r.notes.empty() && (r.notes.back() == "Cara" || r.notes.back() == "Cruz"), "result text (%s)", r.notes.empty() ? "-" : r.notes.back().c_str());
        r.Skip(8000);
        CHECK(r.fun.mode() == ChipoFun::Mode::Off, "closes by itself");
        int heads = 0;
        for (int i = 0; i < 200; ++i) { r.fun.OnVoiceText("tira una moneda"); r.Skip(1600); heads += r.fun.Debug().coin_heads; r.fun.OnTouch(); }
        CHECK(heads > 60 && heads < 140, "fair coin: %d heads of 200", heads);
    }

    std::printf("== offline murmur\n");
    {
        Rig r;
        int babbles = 0;
        ChipoFun::Hooks h;
        h.play_clip = [&r](const char* id) { r.clips.push_back(id); return true; };
        h.notify = [](const char*) {};
        h.babble = [&babbles](uint32_t) { ++babbles; };
        h.clock = [] { return g_now; };
        ChipoFun fun;
        CHECK(fun.Start(lv_screen_active(), std::move(h), "0.6.0"), "start");
        auto tick = [&](uint32_t ms) { for (uint32_t t = 0; t < ms; t += 1000) { g_now += 1000; fun.MurmurTick(g_now); } };
        tick(120000);
        CHECK(fun.murmur_count() == 0 && babbles == 0, "online: silence");
        fun.SetOffline(true);
        tick(7000);
        CHECK(fun.murmur_count() == 0, "not instantly");
        tick(3000);
        CHECK(fun.murmur_count() == 1 && babbles == 1, "first murmur within about 8 s (%d)", fun.murmur_count());
        const int before = fun.murmur_count();
        tick(25000);
        CHECK(fun.murmur_count() == before, "then a pause of at least 30 s");
        tick(600000);
        CHECK(fun.murmur_count() >= before + 9 && fun.murmur_count() <= before + 21, "about every 30-60 s (%d in 10 min)", fun.murmur_count() - before);
        std::set<std::string> kinds;
        for (auto& c : r.clips) { CHECK(c.rfind("murmur_", 0) == 0, "only murmur clips (%s)", c.c_str()); kinds.insert(c); }
        CHECK(kinds.size() == 4, "all four clips get used (%zu)", kinds.size());
        for (size_t i = 1; i < r.clips.size(); ++i) CHECK(r.clips[i] != r.clips[i - 1], "never the same clip twice in a row");
        fun.SetOffline(false);
        const int done = fun.murmur_count();
        tick(300000);
        CHECK(fun.murmur_count() == done, "internet is back: silence again");
        fun.SetOffline(true);
        fun.OnVoiceText("modo lámpara");
        tick(300000);
        CHECK(fun.murmur_count() == done, "no murmur over the lamp");
        fun.Stop();
        fun.MurmurTick(g_now);  // after Stop: must not crash
    }

    std::printf("== server down: me caí\n");
    {
        Rig r;
        int faces = 0;
        ChipoFun fun;
        ChipoFun::Hooks h;
        h.play_clip = [&r](const char* id) { r.clips.push_back(id); return true; };
        h.notify = [&r](const char* t) { r.notes.push_back(t); };
        h.oops = [&faces] { ++faces; };
        h.clock = [] { return g_now; };
        fun.Start(lv_screen_active(), std::move(h), "0.6.0");
        CHECK(fun.OnServerError(g_now) && r.Played("oops") && faces == 1, "reacts with sound and face");
        CHECK(!r.notes.empty() && r.notes.back() == "Me caí... uuuy, me caí", "says it in text");
        CHECK(!fun.OnServerError(g_now + 5000) && fun.oops_count() == 1, "not again within 20 s (an error storm stays calm)");
        CHECK(fun.OnServerError(g_now + 21000) && fun.oops_count() == 2, "again after 20 s");
        fun.Stop();
        CHECK(!fun.OnServerError(g_now + 60000), "after Stop: nothing");
    }

    std::printf("== no clips, ordinary speech, stop\n");
    {
        Rig r;
        r.clips_exist = false;
        r.Start();
        CHECK(!r.fun.OnVoiceText("tira un dado"), "no sound: the assistant is not silenced");
        r.Run(1500);
        CHECK(r.fun.mode() == ChipoFun::Mode::Dice && !r.notes.empty(), "but the die is rolled and the result shown");
        CHECK(!r.fun.OnVoiceText("qué tiempo hace") && !r.fun.OnVoiceText(nullptr), "ordinary speech untouched");
        r.fun.Stop();
        r.fun.Step(g_now);  // after Stop: must not crash (ASan)
    }

    if (g_failures == 0) std::printf("ALL FUN TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
