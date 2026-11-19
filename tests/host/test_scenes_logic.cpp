#include <cmath>
#include <cstdio>

#include "chipo_scenes_logic.h"

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++g_failures; std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static unsigned g_seed = 5;
static int Rnd(int lo, int hi) { g_seed = g_seed * 1103515245u + 12345u; return lo + static_cast<int>((g_seed >> 8) % static_cast<unsigned>(hi - lo + 1)); }

int main() {
    std::printf("== scene voice\n");
    auto on = [](const char* t, SceneKind k, bool active = false) { auto c = ParseSceneCommand(t, active); return c.matched && !c.off && c.kind == k; };
    auto off = [](const char* t, SceneKind k, bool active = true) { auto c = ParseSceneCommand(t, active); return c.matched && c.off && c.kind == k; };
    CHECK(on("pon el visualizador", SceneKind::Visualizer), "visualizador");
    CHECK(on("Chipo, ponme el ecualizador por favor", SceneKind::Visualizer), "ecualizador");
    CHECK(on("muéstrame el acuario", SceneKind::Aquarium) && on("acuario", SceneKind::Aquarium), "acuario");
    CHECK(on("enciende la lámpara de lava", SceneKind::Lava), "lava");
    CHECK(on("quiero ver los peces", SceneKind::Aquarium), "peces");
    CHECK(off("quita el acuario", SceneKind::Aquarium), "quitar acuario");
    CHECK(off("apaga la lámpara de lava", SceneKind::Lava, false), "apagar lava");
    CHECK(off("cierra eso", SceneKind::None) && off("quita eso", SceneKind::None), "close what is open");
    CHECK(!ParseSceneCommand("cierra eso", false).matched, "nothing to close when no screen is open");
    CHECK(!ParseSceneCommand("apaga la luz", true).matched, "the light is not ours");
    CHECK(!ParseSceneCommand("qué es un acuario y cómo se cuida uno", false).matched, "a question is not an order");
    CHECK(!ParseSceneCommand("enciende la lámpara", false).matched, "the plain lamp belongs to FUN");
    CHECK(!ParseSceneCommand("", false).matched && !ParseSceneCommand(nullptr, false).matched, "empty");

    std::printf("== dance voice\n");
    CHECK(ParseDanceCommand("baila", false) == DanceIntent::On, "baila");
    CHECK(ParseDanceCommand("Chipo ponte a bailar", false) == DanceIntent::On, "ponte a bailar");
    CHECK(ParseDanceCommand("bailemos", false) == DanceIntent::On, "bailemos");
    CHECK(ParseDanceCommand("para de bailar", true) == DanceIntent::Off, "para de bailar");
    CHECK(ParseDanceCommand("no bailes más", true) == DanceIntent::Off, "no bailes");
    CHECK(ParseDanceCommand("quieto", true) == DanceIntent::Off, "quieto while dancing");
    CHECK(ParseDanceCommand("quieto", false) == DanceIntent::None, "quieto when not dancing");
    CHECK(ParseDanceCommand("sabes bailar", false) == DanceIntent::None, "a question");
    CHECK(ParseDanceCommand("qué tiempo hace", false) == DanceIntent::None, "unrelated");

    std::printf("== bars\n");
    BarsModel bars;
    float loud[8] = {1, 0.8f, 0.6f, 0.4f, 0.2f, 0.1f, 0.05f, 0};
    for (int i = 0; i < 10; ++i) bars.Update(loud, 0.04f, 0);
    CHECK(bars.bar[0] > 0.8f && bars.bar[3] > 0.3f && bars.bar[7] < 0.05f, "bars follow the bands (%.2f %.2f %.2f)", bars.bar[0], bars.bar[3], bars.bar[7]);
    const float top = bars.bar[0];
    float silent[8] = {};
    bars.Update(silent, 0.04f, 0);
    CHECK(bars.bar[0] > 0.3f && bars.bar[0] < top, "bars fall slowly (%.2f)", bars.bar[0]);
    CHECK(bars.peak[0] >= bars.bar[0], "peak caps stay above the bars");
    for (int i = 0; i < 60; ++i) bars.Update(silent, 0.04f, 0);
    CHECK(bars.bar[0] < 0.05f && bars.peak[0] < 0.7f, "bars and peaks come down (%.2f %.2f)", bars.bar[0], bars.peak[0]);
    BarsModel idle;
    for (int i = 0; i < 50; ++i) idle.Update(nullptr, 0.04f, i * 0.04f);
    CHECK(idle.bar[0] > 0.01f && idle.bar[0] < 0.2f, "idle wave stays low but alive (%.2f)", idle.bar[0]);

    std::printf("== aquarium\n");
    AquariumModel aq;
    aq.Init(368, 448, Rnd);
    bool turned = false, inside = true;
    int dir0[AquariumModel::kFish];
    for (int i = 0; i < AquariumModel::kFish; ++i) dir0[i] = aq.fish[i].dir;
    for (int i = 0; i < 4000; ++i) {
        aq.Update(0.04f, Rnd);
        for (int k = 0; k < AquariumModel::kFish; ++k) {
            if (aq.fish[k].dir != dir0[k]) turned = true;
            if (aq.fish[k].x < -5 || aq.fish[k].x > 373 || aq.fish[k].y < 20 || aq.fish[k].y > 430) inside = false;
        }
        for (const Bubble& b : aq.bubble) if (b.y < -30 || b.y > 480 || b.x < 0 || b.x > 368) inside = false;
    }
    CHECK(turned, "fish never turned around");
    CHECK(inside, "a fish or bubble left the tank");
    bool facing = true;
    for (const Fish& f : aq.fish) facing = facing && ((f.vx > 0) == (f.dir > 0));
    CHECK(facing, "fish face the way they swim");

    std::printf("== lava\n");
    LavaModel lava;
    lava.Init(368, 448, Rnd);
    bool in_lamp = true;
    float miny = 999, maxy = -999;
    for (int i = 0; i < 6000; ++i) {
        lava.Update(0.04f);
        for (const LavaBlob& b : lava.blob) {
            if (b.x < 0 || b.x > 368 || b.y < 0 || b.y > 448) in_lamp = false;
            miny = std::fmin(miny, b.y); maxy = std::fmax(maxy, b.y);
        }
    }
    CHECK(in_lamp, "a blob left the lamp");
    CHECK(maxy - miny > 250, "blobs do not travel up and down (%.0f)", maxy - miny);
    CHECK(lava.blob[0].hue >= 0 && lava.blob[0].hue <= 1.0f, "hue stays in range");

    if (g_failures == 0) std::printf("ALL SCENES LOGIC TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
