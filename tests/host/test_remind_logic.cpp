#include <cstdio>

#include "chipo_remind_logic.h"

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); ++g_fail; } } while (0)

static RemindCommand P(const char* t, bool ringing = false) { return ParseRemindCommand(t, ringing); }

int main() {
    std::printf("== adding by voice\n");
    {
        auto c = P("Chipo, recuérdame a las 18:30 sacar al perro");
        CHECK(c.intent == RemindIntent::Add && c.reminder.hour == 18 && c.reminder.minute == 30 && !c.reminder.daily && c.reminder.text == "sacar al perro",
              "basic (%d %d:%d '%s')", static_cast<int>(c.intent), c.reminder.hour, c.reminder.minute, c.reminder.text.c_str());
        c = P("recuérdame a las seis y media de la tarde llamar a mamá");
        CHECK(c.intent == RemindIntent::Add && c.reminder.hour == 18 && c.reminder.minute == 30 && c.reminder.text == "llamar a mama", "words + pm (%d:%d '%s')", c.reminder.hour, c.reminder.minute, c.reminder.text.c_str());
        c = P("recuérdame todos los días a las 8 tomar la medicina");
        CHECK(c.intent == RemindIntent::Add && c.reminder.daily && c.reminder.hour == 8 && c.reminder.text == "tomar la medicina", "daily before (%d '%s')", c.reminder.daily, c.reminder.text.c_str());
        c = P("recuérdame a las 7 de la mañana todos los días hacer ejercicio");
        CHECK(c.intent == RemindIntent::Add && c.reminder.daily && c.reminder.hour == 7 && c.reminder.text == "hacer ejercicio", "daily after, am (%d:%d '%s')", c.reminder.hour, c.reminder.minute, c.reminder.text.c_str());
        c = P("avísame a las 9 y cuarto de la reunión");
        CHECK(c.intent == RemindIntent::Add && c.reminder.hour == 9 && c.reminder.minute == 15 && c.reminder.text == "reunion",
              "quarter past (%d:%d '%s')", c.reminder.hour, c.reminder.minute, c.reminder.text.c_str());
        c = P("recuérdame a las 20:00");
        CHECK(c.intent == RemindIntent::Add && c.reminder.text == "recordatorio", "no text: generic");
        c = P("recuérdame mañana a las 8 la cita");
        CHECK(c.intent == RemindIntent::AddNeedsDate, "dates are refused honestly");
        c = P("recuérdame el viernes a las 8 la cita");
        CHECK(c.intent == RemindIntent::AddNeedsDate, "weekday refused");
        CHECK(P("recuérdame en diez minutos sacar la pizza").intent == RemindIntent::None, "durations belong to BUDDY");
        CHECK(P("recuérdame comprar pan").intent == RemindIntent::None, "no time: the assistant");
        CHECK(P("a las 8 tengo clase").intent == RemindIntent::None, "no verb");
        CHECK(P("").intent == RemindIntent::None && P(nullptr).intent == RemindIntent::None, "empty");
    }
    std::printf("== list / clear / dismiss\n");
    {
        CHECK(P("qué recordatorios tengo").intent == RemindIntent::List, "list");
        CHECK(P("dime mis recordatorios").intent == RemindIntent::List, "list 2");
        CHECK(P("borra los recordatorios").intent == RemindIntent::ClearAll, "clear all");
        CHECK(P("cancela todos los recordatorios").intent == RemindIntent::ClearAll, "clear all 2");
        auto c = P("borra el recordatorio de las 18:30");
        CHECK(c.intent == RemindIntent::ClearAt && c.hour == 18 && c.minute == 30, "clear at");
        CHECK(P("vale", true).intent == RemindIntent::Dismiss && P("gracias", true).intent == RemindIntent::Dismiss, "dismiss while ringing");
        CHECK(P("vale", false).intent == RemindIntent::None, "not ringing: nothing");
    }
    std::printf("== store and persistence\n");
    {
        RemindStore s;
        Reminder a; a.hour = 18; a.minute = 30; a.text = "sacar al perro";
        Reminder b; b.hour = 8; b.minute = 0; b.daily = true; b.text = "tomar la medicina, con agua";
        CHECK(s.Add(a) && s.Add(b), "added");
        CHECK(s.Add(a) && s.items().size() == 2, "same reminder is not duplicated");
        const std::string saved = s.Serialize();
        RemindStore t;
        t.Load(saved);
        CHECK(t.items().size() == 2 && t.items()[0].text == "sacar al perro" && t.items()[1].daily && t.items()[1].text == "tomar la medicina, con agua", "round trip (%s)", saved.c_str());
        t.Load("junk\n0900,1,ok\n2599,0,bad hour\n1230,x,bad flag\n\n0830,0,good\n");
        CHECK(t.items().size() == 2 && t.items()[0].text == "ok" && t.items()[1].text == "good", "malformed lines skipped (%zu)", t.items().size());
        RemindStore full;
        for (int i = 0; i < 8; ++i) { Reminder r; r.hour = i; r.text = "x"; full.Add(r); }
        Reminder extra; extra.hour = 12; extra.text = "y";
        CHECK(!full.Add(extra), "full at 8");
        CHECK(full.RemoveAt(3, 0) == 1 && full.Add(extra), "room after removing");
        CHECK(RemindListText({}) == "No tienes recordatorios", "empty list text");
        CHECK(RemindListText(s.items()).find("18:30 sacar al perro") != std::string::npos, "list text");
        CHECK(RemindConfirmation(b).find("todos los días a las 08:00") != std::string::npos, "confirmation");
    }
    std::printf("== firing\n");
    {
        RemindStore s;
        Reminder once; once.hour = 18; once.minute = 30; once.text = "perro";
        Reminder daily; daily.hour = 18; daily.minute = 31; daily.daily = true; daily.text = "pastilla";
        s.Add(once); s.Add(daily);
        CHECK(s.Due(false, 18, 30).empty(), "clock not valid yet: nothing");
        CHECK(s.Due(true, 18, 29).empty(), "before");
        auto f = s.Due(true, 18, 30);
        CHECK(f.size() == 1 && f[0].text == "perro", "fires at 18:30");
        CHECK(s.Due(true, 18, 30).empty(), "once per minute");
        CHECK(s.items().size() == 1, "one-shot removed");
        f = s.Due(true, 18, 31);
        CHECK(f.size() == 1 && f[0].text == "pastilla" && s.items().size() == 1, "daily stays");
        // Next day.
        CHECK(s.Due(true, 18, 30).empty(), "18:30 again: the one-shot is gone");
        f = s.Due(true, 18, 31);
        CHECK(f.size() == 1, "daily fires again");
        // Late tick: catches up a few minutes, but not a clock jump.
        RemindStore c;
        Reminder r; r.hour = 10; r.minute = 2; r.text = "late";
        c.Add(r);
        c.Due(true, 10, 0);
        f = c.Due(true, 10, 4);
        CHECK(f.size() == 1, "a tick that arrives 4 min late still fires it");
        RemindStore j;
        Reminder k; k.hour = 12; k.minute = 0; k.text = "jump";
        j.Add(k);
        j.Due(true, 9, 0);
        CHECK(j.Due(true, 15, 0).empty() && j.items().size() == 1, "a clock jump does not fire or lose the reminder");
        // Around midnight.
        RemindStore m;
        Reminder mid; mid.hour = 0; mid.minute = 1; mid.text = "mid"; mid.daily = true;
        m.Add(mid);
        m.Due(true, 23, 59);
        CHECK(m.Due(true, 0, 1).size() == 1, "catch-up across midnight");
    }
    if (g_fail == 0) std::printf("ALL REMIND LOGIC TESTS PASSED\n");
    return g_fail == 0 ? 0 : 1;
}
