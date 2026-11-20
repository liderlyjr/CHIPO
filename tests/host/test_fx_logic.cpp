// Pure logic of CHIPO FX: particle engine, costume by date, costume and "gracias" voice parsing.
#include <cmath>
#include <cstdio>

#include "chipo_fx_logic.h"

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

int main() {
    unsigned seed = 7;
    FxEngine e([&](int lo, int hi) { seed = (seed * 1103515245u + 12345u) & 0x7fffffffu; return lo + (hi > lo ? static_cast<int>(seed % static_cast<unsigned>(hi - lo + 1)) : 0); });

    std::printf("== engine\n");
    CHECK(e.alive() == 0, "starts empty");
    CHECK(e.Burst(FxKind::Heart, 5, 100, 100, 50) == 5 && e.alive(FxKind::Heart) == 5, "five hearts");
    const float y0 = e.at(0).y;
    e.Step(0.5f);
    CHECK(e.at(0).y < y0 - 15.0f, "hearts rise (%.1f -> %.1f)", y0, e.at(0).y);
    CHECK(FxEngine::Alpha(e.at(0)) > 0.9f, "heart visible mid-life");
    for (int i = 0; i < 100; ++i) e.Step(0.05f);
    CHECK(e.alive() == 0, "hearts die by themselves");
    // Pool limit: extra particles are skipped, never crash.
    CHECK(e.Burst(FxKind::Sparkle, 40, 0, 0, 10) == FxEngine::kMax, "burst is capped by the pool");
    CHECK(e.Spawn(FxKind::Tear, 0, 0, 10, 0) == -1, "full pool refuses");
    e.Clear();
    CHECK(e.alive() == 0, "clear");
    // Tears fall and speed up.
    const int t = e.Spawn(FxKind::Tear, 50, 50, 12, 0x9FD4FF);
    e.Step(0.2f);
    const float y1 = e.at(t).y;
    e.Step(0.2f);
    CHECK(e.at(t).y - y1 > y1 - 50.0f, "tear accelerates");
    // Zzz drifts up and right and grows.
    e.Clear();
    const int z = e.Spawn(FxKind::Zzz, 50, 50, 14, 0xFFFFFF);
    const float s0 = FxEngine::Scale(e.at(z));
    for (int i = 0; i < 20; ++i) e.Step(0.05f);
    CHECK(e.at(z).y < 45 && e.at(z).x > 50, "zzz drifts up and right");
    CHECK(FxEngine::Scale(e.at(z)) > s0, "zzz grows");
    // Alpha: dead particles are invisible, fades in and out.
    FxParticle p;
    CHECK(FxEngine::Alpha(p) == 0, "dead = invisible");
    p.alive = true; p.life = 2; p.age = 0;
    CHECK(FxEngine::Alpha(p) < 0.05f, "fades in");
    p.age = 1;
    CHECK(FxEngine::Alpha(p) > 0.99f, "full in the middle");
    p.age = 1.95f;
    CHECK(FxEngine::Alpha(p) < 0.2f, "fades out");

    std::printf("== costume by date\n");
    CHECK(CostumeForDate(12, 1) == Costume::SantaHat && CostumeForDate(12, 31) == Costume::SantaHat, "december");
    CHECK(CostumeForDate(1, 6) == Costume::SantaHat && CostumeForDate(1, 7) == Costume::None, "until Reyes");
    CHECK(CostumeForDate(11, 30) == Costume::None && CostumeForDate(6, 15) == Costume::None, "other days");

    std::printf("== costume voice\n");
    auto put = [](const char* t, Costume c) { auto r = ParseCostumeCommand(t); return r.matched && r.costume == c; };
    auto off = [](const char* t) { auto r = ParseCostumeCommand(t); return r.matched && r.costume == Costume::None; };
    CHECK(put("ponte el gorro de Papá Noel", Costume::SantaHat), "gorro");
    CHECK(put("Ponte el gorro de navidad", Costume::SantaHat), "gorro navidad");
    CHECK(put("ponte las gafas de sol", Costume::Sunglasses), "gafas");
    CHECK(put("ponte un bigote", Costume::Mustache), "bigote");
    CHECK(off("quítate el gorro"), "quitar gorro");
    CHECK(off("quítate las gafas"), "quitar gafas");
    CHECK(off("quítate el disfraz"), "quitar disfraz");
    CHECK(!ParseCostumeCommand("ponte cómodo").matched, "ponte comodo is not a costume");
    CHECK(!ParseCostumeCommand("quítate de en medio").matched, "quitate de en medio");
    CHECK(!ParseCostumeCommand("qué hora es").matched && !ParseCostumeCommand(nullptr).matched, "unrelated");
    CHECK(!ParseCostumeCommand("ponme el gorro en la cabeza de mi hijo y luego vamos al parque").matched, "too long");

    std::printf("== thanks\n");
    CHECK(ChipoIsThanks("Gracias") && ChipoIsThanks("muchas gracias chipo") && ChipoIsThanks("te quiero"), "thanks");
    CHECK(ChipoIsThanks("eres un amor") && ChipoIsThanks("Eres genial"), "kind words");
    CHECK(!ChipoIsThanks("qué tiempo hace") && !ChipoIsThanks("") && !ChipoIsThanks(nullptr), "not thanks");
    CHECK(!ChipoIsThanks("dime el significado de la palabra gracias en inglés por favor que quiero saber"), "long sentence ignored");

    if (g_failures == 0) std::printf("ALL FX LOGIC TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
