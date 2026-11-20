#include <cstdio>
#include <string>
#include <vector>

#include "chipo_remind.h"

static uint32_t g_now = 1000;
static uint32_t FakeTick() { return g_now; }
static uint8_t g_buf[368 * 40 * 2];
static void Flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); ++g_fail; } } while (0)

struct Rig {
    ChipoRemind remind;
    std::vector<std::string> clips, notes, emotions;
    std::string flash;  // stands for the NVS string
    int saves = 0;
    bool busy = false;
    bool wall_valid = true;
    int wh = 17, wm = 0, ws = 0;

    bool Start() {
        ChipoRemind::Hooks h;
        h.play_clip = [this](const char* id) { clips.push_back(id); return true; };
        h.notify = [this](const char* t) { notes.push_back(t); };
        h.emotion = [this](const char* e, uint32_t) { emotions.push_back(e); };
        h.assistant_busy = [this] { return busy; };
        h.wall_time = [this](ChipoRemind::Wall* w) { w->valid = wall_valid; w->hour = wh; w->minute = wm; return wall_valid; };
        h.load = [this] { return flash; };
        h.save = [this](const std::string& s) { flash = s; ++saves; };
        h.clock = [] { return g_now; };
        return remind.Start(std::move(h));
    }
    void Advance(int seconds) {
        for (int i = 0; i < seconds; ++i) {
            g_now += 1000;
            if (++ws >= 60) { ws = 0; if (++wm >= 60) { wm = 0; wh = (wh + 1) % 24; } }
            remind.Step(g_now);
        }
    }
    int Count(const std::string& clip) const { int n = 0; for (auto& c : clips) n += c == clip; return n; }
};

int main() {
    lv_init();
    lv_tick_set_cb(FakeTick);
    lv_display_t* d = lv_display_create(368, 448);
    lv_display_set_buffers(d, g_buf, nullptr, sizeof(g_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, Flush);

    std::printf("== saving, ringing, using up a one-shot\n");
    {
        Rig r;
        CHECK(r.Start(), "start");
        CHECK(r.remind.OnVoiceText("Chipo, recuérdame a las 17:01 sacar al perro"), "handled with a sound");
        CHECK(r.notes.back().find("17:01") != std::string::npos && r.notes.back().find("sacar al perro") != std::string::npos, "confirmation (%s)", r.notes.back().c_str());
        CHECK(r.saves == 1 && r.flash == "1701,0,sacar al perro\n", "saved (%s)", r.flash.c_str());
        r.Advance(60);
        CHECK(r.remind.ringing(), "rings at 17:01");
        CHECK(r.notes.back() == "Recordatorio: Sacar al perro" && r.Count("buddy_alarm") == 1, "text + alarm (%s)", r.notes.back().c_str());
        CHECK(r.saves == 2 && r.flash.empty(), "the used-up one-shot is saved away (%d)", r.saves);
        r.Advance(19);
        CHECK(r.Count("buddy_alarm") == 1, "waits 20 s between rings");
        r.Advance(2);
        CHECK(r.Count("buddy_alarm") == 2, "second ring");
        CHECK(r.remind.OnVoiceText("vale") && !r.remind.ringing(), "'vale' stops it");
        r.Advance(60);
        CHECK(r.Count("buddy_alarm") == 2, "no more rings");
    }
    std::printf("== survives a reboot\n");
    {
        Rig a;
        a.Start();
        a.remind.OnVoiceText("recuérdame todos los días a las 17:05 tomar la medicina");
        a.remind.OnVoiceText("recuérdame a las 20:00 cenar");
        const std::string flash = a.flash;
        a.remind.Stop();
        Rig b;  // a new boot reading the same flash
        b.flash = flash;
        b.Start();
        CHECK(b.remind.store().items().size() == 2, "both reminders loaded (%zu)", b.remind.store().items().size());
        CHECK(b.remind.OnVoiceText("qué recordatorios tengo") && b.notes.back().find("17:05 tomar la medicina") != std::string::npos, "list (%s)", b.notes.back().c_str());
        b.Advance(5 * 60 + 5);
        CHECK(b.remind.ringing() && b.notes.back().find("Tomar la medicina") != std::string::npos, "daily one rings after the reboot (%s)", b.notes.back().c_str());
    }
    std::printf("== waits for the assistant, and for the real clock\n");
    {
        Rig r;
        r.Start();
        r.remind.OnVoiceText("recuérdame a las 17:01 llamar");
        r.busy = true;
        r.Advance(60);
        CHECK(r.Count("buddy_alarm") == 0, "does not talk over the assistant");
        r.busy = false;
        r.Advance(2);
        CHECK(r.Count("buddy_alarm") == 1, "rings as soon as it is quiet");

        Rig n;
        n.wall_valid = false;
        n.Start();
        n.remind.OnVoiceText("recuérdame a las 17:01 llamar");
        n.Advance(120);
        CHECK(n.Count("buddy_alarm") == 0, "without a valid clock nothing rings");
        n.wall_valid = true;
        n.wh = 17; n.wm = 1; n.ws = 0;
        n.Advance(2);
        CHECK(n.Count("buddy_alarm") == 1, "rings once the time is known");
    }
    std::printf("== refusals and editing\n");
    {
        Rig r;
        r.Start();
        CHECK(r.remind.OnVoiceText("recuérdame mañana a las 8 la cita") && r.notes.back().find("hoy o todos los días") != std::string::npos, "dates refused (%s)", r.notes.back().c_str());
        CHECK(r.remind.store().items().empty() && r.saves == 0, "nothing stored");
        CHECK(!r.remind.OnVoiceText("recuérdame comprar pan"), "no time: the assistant answers");
        for (int i = 0; i < 8; ++i) r.remind.OnVoiceText(("recuérdame a las " + std::to_string(10 + i) + ":00 tarea " + std::to_string(i)).c_str());
        CHECK(r.remind.store().items().size() == 8, "eight stored (%zu)", r.remind.store().items().size());
        r.remind.OnVoiceText("recuérdame a las 23:00 otra");
        CHECK(r.notes.back().find("8 recordatorios") != std::string::npos && r.remind.store().items().size() == 8, "ninth refused (%s)", r.notes.back().c_str());
        r.remind.OnVoiceText("borra el recordatorio de las 10:00");
        CHECK(r.remind.store().items().size() == 7, "one removed");
        r.remind.OnVoiceText("borra el recordatorio de las 03:00");
        CHECK(r.notes.back().find("ningún") != std::string::npos, "nothing at that time (%s)", r.notes.back().c_str());
        r.remind.OnVoiceText("borra los recordatorios");
        CHECK(r.remind.store().items().empty() && r.flash.empty(), "all gone and saved");
        r.remind.Stop();
        CHECK(!r.remind.OnVoiceText("qué recordatorios tengo"), "stopped: nothing");
    }
    if (g_fail == 0) std::printf("ALL REMIND TESTS PASSED\n");
    return g_fail == 0 ? 0 : 1;
}
