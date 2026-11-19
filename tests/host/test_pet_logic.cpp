#include <cstdio>
#include <deque>

#include "chipo_pet_logic.h"

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); ++g_fail; } } while (0)

static void Is(const char* text, PetIntent want, bool active = false, int value = -99) {
    const PetCommand c = ParsePetCommand(text, active);
    if (c.intent != want || (value != -99 && c.value != value)) {
        std::printf("FAIL: \"%s\" -> %d/%d (wanted %d/%d)\n", text ? text : "(null)", static_cast<int>(c.intent), c.value, static_cast<int>(want), value);
        ++g_fail;
    }
}

int main() {
    std::printf("== mood\n");
    {
        PetMood m;
        m.Reset(0);
        const uint32_t kMin = 60u * 1000u;
        CHECK(m.Value(0) == 70 && m.Level(0) == PetLevel::Happy, "starts happy");
        CHECK(m.Value(30 * kMin) == 70, "no decay in the first 30 min");
        CHECK(m.Value(30 * kMin + 6 * kMin) == 69, "-1 after 6 more min (%d)", m.Value(36 * kMin));
        CHECK(m.Level(2 * 60 * kMin) == PetLevel::Fine, "fine after 2 h (%d)", m.Value(2 * 60 * kMin));
        CHECK(m.Level(4 * 60 * kMin) == PetLevel::Lonely, "lonely after 4 h (%d)", m.Value(4 * 60 * kMin));
        CHECK(m.Level(8 * 60 * kMin) == PetLevel::Neglected, "neglected after 8 h (%d)", m.Value(8 * 60 * kMin));
        CHECK(m.Value(1000u * 60 * kMin) == 0, "never below 0");
        m.Attention(8 * 60 * kMin, 8);
        CHECK(m.Value(8 * 60 * kMin) >= 8 && m.Value(8 * 60 * kMin) <= 20, "attention lifts it from the floor");
        m.Reset(0);
        for (int i = 0; i < 20; ++i) m.Attention(i * 1000u, 6);
        CHECK(m.Value(20000) == 100, "capped at 100");
    }
    std::printf("== pout\n");
    {
        PetMood m;
        m.Reset(0);
        const uint32_t kMin = 60u * 1000u;
        CHECK(!m.ShouldPout(10 * kMin), "happy: no pout");
        uint32_t t = 4 * 60 * kMin;  // lonely
        CHECK(m.ShouldPout(t), "first sulk when lonely");
        CHECK(!m.ShouldPout(t + 5 * kMin), "not again within 15 min");
        CHECK(m.ShouldPout(t + 16 * kMin), "again after 15 min");
        m.Attention(t + 17 * kMin, 8);
        CHECK(!m.ShouldPout(t + 18 * kMin), "no sulk after being petted");
    }
    std::printf("== voice\n");
    Is("Chipo, ¿cómo estás?", PetIntent::HowAreYou);
    Is("cómo andas", PetIntent::HowAreYou);
    Is("qué tal estás", PetIntent::HowAreYou);
    Is("estás triste", PetIntent::HowAreYou);
    Is("cómo está el tiempo hoy", PetIntent::None);
    Is("cómo te llamas", PetIntent::None);
    Is("juguemos a piedra papel o tijera", PetIntent::RpsStart);
    Is("piedra, papel o tijera", PetIntent::RpsStart);
    Is("adivina el número", PetIntent::GuessStart);
    Is("piensa un número", PetIntent::GuessStart);
    Is("piedra", PetIntent::RpsMove, true, 0);
    Is("papel", PetIntent::RpsMove, true, 1);
    Is("tijeras", PetIntent::RpsMove, true, 2);
    Is("piedra", PetIntent::None, false);
    Is("siete", PetIntent::GuessNumber, true, 7);
    Is("el 3", PetIntent::GuessNumber, true, 3);
    Is("es el diez", PetIntent::GuessNumber, true, 10);
    Is("quince", PetIntent::None, true);  // not a number we know
    Is("12", PetIntent::GuessNumber, true, 0);
    Is("siete", PetIntent::None, false);
    Is("ya no quiero jugar", PetIntent::StopGame, true);
    Is("basta", PetIntent::StopGame, true);
    Is("ya no quiero jugar", PetIntent::None, false);
    Is("programa un temporizador de cinco minutos", PetIntent::None, true);
    Is("", PetIntent::None);
    Is(nullptr, PetIntent::None);

    std::printf("== rock paper scissors\n");
    {
        std::deque<int> seq{2, 0, 1, 1, 0};
        PetGames g([&](int, int) { int v = seq.front(); seq.pop_front(); return v; });
        PetCommand start; start.intent = PetIntent::RpsStart;
        CHECK(g.Handle(start, 0).handled && g.game() == PetGames::Game::Rps, "game on");
        PetCommand mv; mv.intent = PetIntent::RpsMove;
        mv.value = 0;  // rock vs scissors: the owner wins
        GameReply r = g.Handle(mv, 1000);
        CHECK(r.text.find("Ganas tú") != std::string::npos && g.rps_user() == 1, "owner wins: %s", r.text.c_str());
        mv.value = 2;  // scissors vs rock: CHIPO wins
        r = g.Handle(mv, 2000);
        CHECK(r.text.find("Gano yo") != std::string::npos && g.rps_chipo() == 1, "chipo wins: %s", r.text.c_str());
        mv.value = 1;  // paper vs paper
        r = g.Handle(mv, 3000);
        CHECK(r.text.find("Empate") != std::string::npos, "draw: %s", r.text.c_str());
        mv.value = 1;  // paper vs paper? seq says 1 -> draw again; then rock(0) vs paper(1)... check score text
        r = g.Handle(mv, 4000);
        CHECK(r.text.find("(1-1)") != std::string::npos, "score shown: %s", r.text.c_str());
        PetGames::Game before = g.game();
        CHECK(before == PetGames::Game::Rps, "still playing");
        CHECK(!g.Tick(30000), "30 s idle: still on");
        CHECK(g.Tick(70000) && g.game() == PetGames::Game::None, "ends after 60 s idle");
        CHECK(!g.Handle(mv, 71000).handled, "move without game is ignored");
    }
    std::printf("== guess the number\n");
    {
        PetGames g([](int, int) { return 7; });
        PetCommand start; start.intent = PetIntent::GuessStart;
        CHECK(g.Handle(start, 0).handled && g.guess_secret() == 7, "thinking of 7");
        PetCommand n; n.intent = PetIntent::GuessNumber;
        n.value = 3; CHECK(g.Handle(n, 1000).text == "Más alto", "higher");
        n.value = 9; CHECK(g.Handle(n, 2000).text == "Más bajo", "lower");
        n.value = 0; CHECK(g.Handle(n, 3000).text.find("1 al 10") != std::string::npos, "out of range does not use a try");
        n.value = 7;
        GameReply r = g.Handle(n, 4000);
        CHECK(r.text == "¡Acertaste en 3 intentos!" && g.game() == PetGames::Game::None, "found it: %s", r.text.c_str());
        g.Handle(start, 5000);
        n.value = 1;
        for (int i = 0; i < 4; ++i) g.Handle(n, 6000 + i);
        r = g.Handle(n, 7000);
        CHECK(r.text.find("Era el 7") != std::string::npos && g.game() == PetGames::Game::None, "five tries then reveal: %s", r.text.c_str());
        g.Handle(start, 8000);
        PetCommand stop; stop.intent = PetIntent::StopGame;
        CHECK(g.Handle(stop, 9000).handled && g.game() == PetGames::Game::None, "stop");
        CHECK(!g.Handle(stop, 9500).handled, "stop without a game is ignored");
    }
    if (g_fail == 0) std::printf("ALL PET LOGIC TESTS PASSED\n");
    return g_fail == 0 ? 0 : 1;
}
