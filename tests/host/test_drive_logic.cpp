// Host tests for the pure logic of CHIPO DRIVE: voice commands and personality.
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include "chipo_drive_personality.h"
#include "chipo_drive_voice.h"

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

static void Expect(const char* text, DriveIntent want) {
    const DriveIntent got = ParseDriveIntent(text);
    CHECK(got == want, "\"%s\" -> %d, wanted %d", text, static_cast<int>(got), static_cast<int>(want));
}

int main() {
    // ---- normalising ----------------------------------------------------------------------------
    CHECK(DriveNormalizeText("¡Vámonos, CHIPÓ!") == " vamonos  chipo ", "normalised: [%s]", DriveNormalizeText("¡Vámonos, CHIPÓ!").c_str());
    CHECK(DriveNormalizeText("Año Ñu") == "ano nu", "n with tilde");
    CHECK(DriveNormalizeText(nullptr).empty(), "null text");

    // ---- activation: "Chipo, vámonos" -----------------------------------------------------------------
    Expect("Chipo, vámonos", DriveIntent::Activate);
    Expect("Chipo vámonos.", DriveIntent::Activate);
    Expect("¡Vámonos!", DriveIntent::Activate);          // the wake word started the conversation
    Expect("vámonos chipo", DriveIntent::Activate);
    Expect("Vamonos", DriveIntent::Activate);
    Expect("Oye Chipo, vámonos ya", DriveIntent::Activate);
    Expect("chee po vámonos", DriveIntent::Activate);
    Expect("CHIPO DRIVE", DriveIntent::Activate);
    Expect("Chipo, activa el modo conducción", DriveIntent::Activate);
    Expect("activa el modo coche", DriveIntent::Activate);
    // ---- things that are NOT activation --------------------------------------------------------------------
    Expect("vámonos a comer a casa de mi madre", DriveIntent::None);
    Expect("Chipo, qué tiempo hace", DriveIntent::None);
    Expect("Chipo, vámonos de vacaciones a la playa mañana", DriveIntent::None);
    Expect("vamos a ver", DriveIntent::None);
    Expect("", DriveIntent::None);
    Expect("   ", DriveIntent::None);
    Expect("modo conducción", DriveIntent::None);             // no verb: not a command
    Expect("el modo conducción del coche de mi vecino es muy raro y no lo entiendo", DriveIntent::None);

    // ---- deactivation -------------------------------------------------------------------------------------------
    Expect("Chipo, desactiva el modo conducción", DriveIntent::Deactivate);
    Expect("desactiva el modo coche", DriveIntent::Deactivate);
    Expect("Chipo, vuelve al modo escritorio", DriveIntent::Deactivate);
    Expect("Chipo, sal del modo coche", DriveIntent::Deactivate);
    Expect("sal del modo conducción", DriveIntent::Deactivate);
    Expect("desactiva el modo conducción", DriveIntent::Deactivate);
    CHECK(ParseDriveIntent("desactiva el modo conducción") != DriveIntent::Activate, "deactivate must not be read as activate");

    // ---- sensitivity ----------------------------------------------------------------------------------------------------
    Expect("Chipo, sensibilidad suave", DriveIntent::SensitivitySoft);
    Expect("sensibilidad normal", DriveIntent::SensitivityNormal);
    Expect("sensibilidad intensa", DriveIntent::SensitivityIntense);
    Expect("sensibilidad alta", DriveIntent::SensitivityIntense);
    Expect("sensibilidad", DriveIntent::None);

    // ---- personality: pacing -------------------------------------------------------------------------------------------------
    {
        // Every phrase has an id, text, and a unique id; every kind with comments has at least 2 phrases.
        int n = 0;
        const DrivePhrase* p = DrivePhrases(&n);
        std::set<std::string> ids;
        int per_kind[6] = {0};
        for (int i = 0; i < n; ++i) {
            CHECK(p[i].id && p[i].text && std::strlen(p[i].text) > 3, "phrase %d complete", i);
            CHECK(ids.insert(p[i].id).second, "unique id %s", p[i].id);
            ++per_kind[static_cast<int>(p[i].kind)];
            CHECK(DriveTextForClip(p[i].id) == p[i].text || std::strcmp(DriveTextForClip(p[i].id), p[i].text) == 0, "text lookup");
        }
        for (int k = 0; k < 6; ++k) CHECK(per_kind[k] >= 2, "kind %d has at least two phrases (%d)", k, per_kind[k]);
        CHECK(DriveTextForClip(kDriveClipStart) && std::strstr(DriveTextForClip(kDriveClipStart), "Brum"), "start line says brum");
        CHECK(DriveTextForClip("nope") == nullptr, "unknown clip");
    }
    for (int level = 0; level < 3; ++level) {
        const DriveSensitivity s = static_cast<DriveSensitivity>(level);
        DrivePersonality pers(0xBEEF + level);
        pers.SetSensitivity(s);
        uint32_t now = 100000;
        pers.Begin(now);
        // The first 20 s of a drive are always quiet, whatever happens.
        int early = 0;
        for (int i = 0; i < 40; ++i) {
            now += 400;
            early += pers.OnEvent(DriveEventKind::Abrupt, 1.0f, now, true).say != nullptr;
        }
        CHECK(early == 0, "quiet during the first seconds (%d)", early);

        // Ten minutes of constant events: must never chatter.
        int spoke = 0;
        uint32_t last = 0, min_gap = 0xFFFFFFFF;
        std::string last_id;
        int repeats = 0;
        for (uint32_t t = 0; t < 600000; t += 500) {
            now = 100000 + 20000 + t;
            const DriveEventKind k = static_cast<DriveEventKind>((t / 500) % 5);
            const DriveReaction r = pers.OnEvent(k, 1.0f, now, true);
            if (r.say) {
                ++spoke;
                if (last) min_gap = now - last < min_gap ? now - last : min_gap;
                last = now;
                if (last_id == r.say->id) ++repeats;
                last_id = r.say->id;
            }
        }
        std::printf("   sensitivity %d: %d comments in 10 min of constant events, min gap %u ms\n", level, spoke, min_gap);
        CHECK(spoke >= 1, "it does comment sometimes (%d)", spoke);
        CHECK(spoke <= 8, "at most 4 comments per 5 minutes (%d in 10)", spoke);
        CHECK(min_gap >= DriveSpeechGapMs(s), "minimum silence respected (%u)", min_gap);
        CHECK(repeats == 0, "never the same phrase twice in a row (%d)", repeats);

        // Never while the assistant is busy.
        int busy_spoke = 0;
        for (uint32_t t = 0; t < 120000; t += 500) {
            busy_spoke += pers.OnEvent(DriveEventKind::Abrupt, 1.0f, now + 700000 + t, false).say != nullptr;
        }
        CHECK(busy_spoke == 0, "silent while the assistant is busy (%d)", busy_spoke);
    }
    {
        // Looks: the reaction is always there, even when quiet.
        DrivePersonality pers(7);
        pers.Begin(0);
        CHECK(pers.OnEvent(DriveEventKind::Brake, 0.5f, 1000, true).look == DriveLook::Surprised, "brake -> surprised");
        CHECK(pers.OnEvent(DriveEventKind::Accelerate, 0.5f, 1000, true).look == DriveLook::Excited, "accelerate -> excited");
        CHECK(pers.OnEvent(DriveEventKind::Abrupt, 1.0f, 1000, true).look == DriveLook::Dizzy, "strong abrupt -> dizzy");
        CHECK(pers.OnEvent(DriveEventKind::Abrupt, 0.4f, 1000, true).look == DriveLook::Surprised, "mild abrupt -> surprised");
    }
    {
        // Calm remarks: rare, only when calm, only when nothing else was said recently.
        DrivePersonality pers(99);
        pers.Begin(0);
        int calm = 0;
        for (uint32_t t = 0; t < 900000; t += 1000) calm += pers.Tick(t, true, true).say != nullptr;
        std::printf("   calm remarks in 15 min: %d\n", calm);
        CHECK(calm >= 2 && calm <= 8, "a few calm remarks in 15 minutes (%d)", calm);
        DrivePersonality pers2(99);
        pers2.Begin(0);
        int not_calm = 0;
        for (uint32_t t = 0; t < 900000; t += 1000) not_calm += pers2.Tick(t, false, true).say != nullptr;
        CHECK(not_calm == 0, "no calm remarks while things are happening (%d)", not_calm);
    }

    if (g_failures == 0) std::printf("ALL DRIVE LOGIC TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
