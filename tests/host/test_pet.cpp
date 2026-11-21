#include <cstdio>
#include <string>
#include <vector>

#include "chipo_pet.h"
#include "lvgl.h"

static uint32_t g_now = 1000;
static uint32_t FakeTick() { return g_now; }
static uint8_t g_buf[480 * 40 * 2];
static void Flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); ++g_fail; } } while (0)

struct Rig {
    ChipoPet pet;
    std::vector<std::string> clips, notes, emotions;
    int next_rand = 0;
    bool with_face_hooks = false;
    int cries = 0, tantrums = 0;
    uint32_t cry_ms = 0;
    bool clips_exist = true;
    bool Start() {
        ChipoPet::Hooks h;
        h.play_clip = [this](const char* id) { if (!clips_exist) return false; clips.push_back(id); return true; };
        h.notify = [this](const char* t) { notes.push_back(t); };
        h.emotion = [this](const char* e, uint32_t) { emotions.push_back(e); };
        if (with_face_hooks) {
            h.cry = [this](uint32_t ms) { ++cries; cry_ms = ms; };
            h.sore_loser = [this]() { ++tantrums; };
        }
        h.rand = [this](int, int) { return next_rand; };
        h.clock = [] { return g_now; };
        return pet.Start(std::move(h));
    }
    void Skip(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 30000) { g_now += 30000; pet.Step(g_now); } }
};

int main() {
    lv_init();
    lv_tick_set_cb(FakeTick);
    lv_display_t* d = lv_display_create(480, 480);
    lv_display_set_buffers(d, g_buf, nullptr, sizeof(g_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, Flush);

    std::printf("== mood answers\n");
    {
        Rig r;
        CHECK(r.Start(), "start");
        CHECK(r.pet.level() == PetLevel::Happy && r.pet.mood() == 70, "happy at start");
        CHECK(r.pet.OnVoiceText("Chipo, ¿cómo estás?"), "answered with a sound");
        CHECK(!r.notes.empty() && r.notes.back().find("feliz") != std::string::npos, "says it is happy (%s)", r.notes.empty() ? "-" : r.notes.back().c_str());
        CHECK(!r.emotions.empty() && r.emotions.back() == "loving", "and shows it");
        CHECK(!r.pet.OnVoiceText("qué hora es"), "other sentences are not ours");
        r.Skip(5u * 60u * 60u * 1000u);
        CHECK(r.pet.level() == PetLevel::Lonely, "lonely after hours alone (%d)", r.pet.mood());
        CHECK(r.pet.pouts() >= 1 && !r.emotions.empty() && r.emotions.back() == "sad", "it sulks on its face (%d)", r.pet.pouts());
        const size_t clips = r.clips.size();
        CHECK(clips == 1, "sulking makes no sound");
        r.pet.OnVoiceText("¿cómo estás?");
        CHECK(r.notes.back().find("solito") != std::string::npos, "answers honestly (%s)", r.notes.back().c_str());
        CHECK(r.pet.mood() > 30, "being asked cheers it up (%d)", r.pet.mood());
        r.pet.OnTouch(ChipoPet::Touch::Tickle);
        CHECK(r.pet.mood() > 36, "a tickle helps (%d)", r.pet.mood());
    }

    std::printf("== games through the controller\n");
    {
        Rig r;
        r.Start();
        r.next_rand = 2;  // scissors / secret number 2
        CHECK(r.pet.OnVoiceText("juguemos a piedra papel o tijera") && r.pet.game() == PetGames::Game::Rps, "rps starts");
        CHECK(r.pet.OnVoiceText("piedra") && r.notes.back().find("Ganas") != std::string::npos, "rock beats scissors (%s)", r.notes.back().c_str());
        CHECK(r.clips.back() == "fun_coin", "reveal sound");
        r.next_rand = 0;  // rock again: draw
        r.pet.OnVoiceText("piedra");
        CHECK(r.notes.back().find("Empate") != std::string::npos, "draw (%s)", r.notes.back().c_str());
        CHECK(r.pet.OnVoiceText("ya no quiero jugar") && r.pet.game() == PetGames::Game::None, "leaving the game");
        CHECK(!r.pet.OnVoiceText("piedra"), "a move without a game goes to the assistant");

        r.next_rand = 7;
        CHECK(r.pet.OnVoiceText("adivina el número") && r.pet.game() == PetGames::Game::Guess, "guess starts");
        CHECK(r.pet.OnVoiceText("tres") && r.notes.back() == "Más alto", "higher");
        CHECK(r.pet.OnVoiceText("siete") && r.notes.back().find("Acertaste") != std::string::npos && r.pet.game() == PetGames::Game::None, "found");

        r.pet.OnVoiceText("piensa un número");
        const size_t before = r.notes.size();
        r.Skip(90000);
        CHECK(r.pet.game() == PetGames::Game::None && r.notes.size() == before + 1, "an abandoned game ends by itself");
    }

    std::printf("== tears and tantrum\n");
    {
        Rig r;
        r.with_face_hooks = true;
        r.Start();
        r.Skip(20u * 60u * 60u * 1000u);  // a whole day alone
        CHECK(r.pet.level() == PetLevel::Neglected, "neglected after a day (%d)", r.pet.mood());
        CHECK(r.cries >= 1 && r.cry_ms >= 5000, "tears when very neglected (%d)", r.cries);
        // Losing makes it throw a tantrum (and no plain emotion on top).
        r.pet.OnTouch(ChipoPet::Touch::Tickle);
        r.next_rand = 2;  // scissors
        r.pet.OnVoiceText("juguemos a piedra papel o tijera");
        const size_t emo = r.emotions.size();
        r.pet.OnVoiceText("piedra");  // rock beats scissors: the owner wins
        CHECK(r.tantrums == 1 && r.emotions.size() == emo, "sore loser (%d)", r.tantrums);
        r.next_rand = 1;  // paper: CHIPO wins this time
        r.pet.OnVoiceText("piedra");
        CHECK(r.tantrums == 1, "no tantrum when it wins");
        r.pet.OnVoiceText("ya no quiero jugar");
        r.next_rand = 7;
        r.pet.OnVoiceText("adivina el número");
        r.pet.OnVoiceText("siete");
        CHECK(r.tantrums == 2, "and when the number is guessed (%d)", r.tantrums);
    }
    {
        // Without the face hooks the old behaviour stays (sleepy face when neglected).
        Rig r;
        r.Start();
        r.Skip(20u * 60u * 60u * 1000u);
        CHECK(!r.emotions.empty() && r.emotions.back() == "sleepy", "fallback to the sleepy face");
    }

    std::printf("== no sounds available\n");
    {
        Rig r;
        r.clips_exist = false;
        r.Start();
        CHECK(!r.pet.OnVoiceText("cómo estás"), "without a clip the server may still answer");
        CHECK(!r.notes.empty(), "but the text is shown");
        r.pet.Stop();
        CHECK(!r.pet.OnVoiceText("cómo estás"), "stopped: nothing");
    }
    if (g_fail == 0) std::printf("ALL PET TESTS PASSED\n");
    return g_fail == 0 ? 0 : 1;
}
