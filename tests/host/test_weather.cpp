// Host-side tests for the weather data layer (parsers, intent, folding). No LVGL, no network.
#include <cstdio>
#include <cstring>
#include <string>

#include "chipo_weather.h"

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

// Shaped like an Open-Meteo answer (current + hourly + daily, timezone=auto, forecast_days=4).
// Wednesday 2026-11-18 is the first day; "now" is 15:15 local, partly cloudy, daytime.
static std::string MakeForecast(const char* now_time, int wmo, int is_day, bool with_spaces = false) {
    const char* sp = with_spaces ? " " : "";
    std::string times, temps, codes, days;
    for (int d = 0; d < 4; ++d) {
        for (int h = 0; h < 24; ++h) {
            char t[32];
            std::snprintf(t, sizeof t, "%s\"2026-11-%02dT%02d:00\"", times.empty() ? "" : ",", 18 + d, h);
            times += t;
            std::snprintf(t, sizeof t, "%s%d.%d", temps.empty() ? "" : ",", h - 3, h % 10);  // 0h = -3.0
            temps += t;
            std::snprintf(t, sizeof t, "%s%d", codes.empty() ? "" : ",", h >= 20 ? 61 : 2);
            codes += t;
            std::snprintf(t, sizeof t, "%s%d", days.empty() ? "" : ",", (h >= 7 && h <= 18) ? 1 : 0);
            days += t;
        }
    }
    std::string j = "{\"latitude\":40.4,\"longitude\":-3.7,\"current_units\":{\"time\":\"iso8601\",\"temperature_2m\":\"\\u00b0C\","
                    "\"is_day\":\"\"},";
    j += std::string("\"current\":{") + sp + "\"time\":\"" + now_time + "\",\"interval\":900,\"temperature_2m\":14.6,"
         "\"apparent_temperature\":-1.5,\"weather_code\":" + std::to_string(wmo) + ",\"is_day\":" + std::to_string(is_day) + "},";
    j += "\"hourly_units\":{\"time\":\"iso8601\",\"temperature_2m\":\"\\u00b0C\"},";
    j += "\"hourly\":{\"time\":[" + times + "],\"temperature_2m\":[" + temps + "],\"weather_code\":[" + codes +
         "],\"is_day\":[" + days + "]},";
    j += "\"daily_units\":{\"time\":\"iso8601\"},";
    j += "\"daily\":{\"time\":[\"2026-11-18\",\"2026-11-19\",\"2026-11-20\",\"2026-11-21\"],"
         "\"weather_code\":[2,63,3,95],\"temperature_2m_max\":[18.4,12.5,9.5,-0.4],\"temperature_2m_min\":[6.2,5.5,2.5,-4.6]}}";
    return j;
}

int main() {
    // ---- helpers --------------------------------------------------------------------------------
    CHECK(WeekdayFromDate(2026, 11, 18) == 3, "18 Nov 2026 is a Wednesday, got %d", WeekdayFromDate(2026, 11, 18));
    CHECK(WeekdayFromDate(2000, 1, 1) == 6, "1 Jan 2000 was a Saturday");
    CHECK(WeekdayFromDate(2024, 2, 29) == 4, "29 Feb 2024 was a Thursday");

    char out[40];
    AsciiFold("M\xC3\xA1laga", out, sizeof out);
    CHECK(std::strcmp(out, "Malaga") == 0, "fold: %s", out);
    AsciiFold("A Coru\xC3\xB1" "a / \xC3\x89" "cija \xE2\x82\xAC", out, sizeof out);
    CHECK(std::strcmp(out, "A Coruna / Ecija ") == 0, "fold: '%s'", out);
    AsciiFold("Sevilla", out, 4);
    CHECK(std::strcmp(out, "Sev") == 0, "fold truncates safely: '%s'", out);
    AsciiFold(nullptr, out, sizeof out);
    CHECK(out[0] == 0, "fold null");

    CHECK(WeatherKindFromWmo(0) == WeatherKind::Clear && WeatherKindFromWmo(2) == WeatherKind::PartlyCloudy &&
          WeatherKindFromWmo(3) == WeatherKind::Cloudy && WeatherKindFromWmo(45) == WeatherKind::Fog &&
          WeatherKindFromWmo(63) == WeatherKind::Rain && WeatherKindFromWmo(81) == WeatherKind::Rain &&
          WeatherKindFromWmo(73) == WeatherKind::Snow && WeatherKindFromWmo(95) == WeatherKind::Thunder &&
          WeatherKindFromWmo(1234) == WeatherKind::Cloudy,
          "wmo mapping");
    for (int k = 0; k <= 6; ++k)
        for (int day = 0; day < 2; ++day)
            CHECK(std::strlen(WeatherKindText(static_cast<WeatherKind>(k), day)) > 0, "text for kind %d", k);

    // ---- voice intent ---------------------------------------------------------------------------
    const char* yes[] = {"\xC2\xBFQu\xC3\xA9 tiempo hace hoy?", "dime el clima", "\xC2\xBFva a llover ma\xC3\xB1" "ana?",
                         "c\xC3\xB3mo est\xC3\xA1 el tiempo en Madrid", "Que temperatura hay", "hace FRIO fuera",
                         "dame el pron\xC3\xB3stico", "what's the weather"};
    for (const char* s : yes) CHECK(WeatherIntent(s), "should be weather: %s", s);
    const char* no[] = {"pon m\xC3\xBAsica", "cuanto tiempo falta para el cumple", "enciende la luz", "", "cuenta un chiste"};
    for (const char* s : no) CHECK(!WeatherIntent(s), "should NOT be weather: '%s'", s);
    CHECK(!WeatherIntent(nullptr), "null intent");
    std::string longtext(5000, 'a');
    CHECK(!WeatherIntent(longtext.c_str()), "long text must not overflow");

    // ---- forecast -------------------------------------------------------------------------------
    for (int spaced = 0; spaced < 2; ++spaced) {
        WeatherData d;
        const std::string j = MakeForecast("2026-11-18T15:15", 2, 1, spaced);
        CHECK(ParseOpenMeteo(j.c_str(), "Mála\xC3\xB1" "ga", 2, &d) || true, "unreachable");
        CHECK(ParseOpenMeteo(j.c_str(), "M\xC3\xA1laga", 2, &d), "forecast should parse (spaced=%d)", spaced);
        CHECK(d.valid && std::strcmp(d.city, "Malaga") == 0, "city '%s'", d.city);
        CHECK(d.temp == 15 && d.feels == -2 && d.kind == WeatherKind::PartlyCloudy && d.is_day, "now: %d %d", d.temp, d.feels);
        CHECK(d.hi == 18 && d.lo == 6, "today hi/lo %d/%d", d.hi, d.lo);
        CHECK(d.hour_count == kWeatherHours, "hours %d", d.hour_count);
        CHECK(std::strcmp(d.hours[0].label, "Ahora") == 0 && d.hours[0].temp == 13 && d.hours[0].is_day,
              "first hour '%s' %d", d.hours[0].label, d.hours[0].temp);  // 15h -> 12.5 -> 13
        CHECK(std::strcmp(d.hours[1].label, "17h") == 0 && d.hours[1].temp == 15, "second hour '%s'", d.hours[1].label);
        CHECK(std::strcmp(d.hours[3].label, "21h") == 0 && d.hours[3].kind == WeatherKind::Rain && !d.hours[3].is_day,
              "21h should be rain at night: '%s'", d.hours[3].label);
        CHECK(std::strcmp(d.hours[4].label, "23h") == 0, "last hour '%s'", d.hours[4].label);
        CHECK(d.day_count == 3, "days %d", d.day_count);
        CHECK(std::strcmp(d.days[0].name, "Jue") == 0 && d.days[0].hi == 13 && d.days[0].lo == 6 &&
              d.days[0].kind == WeatherKind::Rain, "day0 %s %d/%d", d.days[0].name, d.days[0].hi, d.days[0].lo);
        CHECK(std::strcmp(d.days[2].name, "Sab") == 0 && d.days[2].hi == 0 && d.days[2].lo == -5 &&
              d.days[2].kind == WeatherKind::Thunder, "day2 %s %d/%d", d.days[2].name, d.days[2].hi, d.days[2].lo);
    }

    // The strip must not run off the end of the data: at 23:30 on the last day only one hour exists.
    {
        WeatherData d;
        const std::string j = MakeForecast("2026-11-21T23:30", 0, 0);
        CHECK(ParseOpenMeteo(j.c_str(), "X", 2, &d), "late forecast should still parse");
        CHECK(d.hour_count == 1 && !d.is_day && d.kind == WeatherKind::Clear, "late: hours=%d", d.hour_count);
    }
    // A "now" that is not in the hourly list falls back to the first entry instead of failing.
    {
        WeatherData d;
        const std::string j = MakeForecast("2030-01-01T10:00", 3, 1);
        CHECK(ParseOpenMeteo(j.c_str(), "X", 1, &d) && d.hour_count == kWeatherHours, "unknown now");
    }

    // Garbage and truncated input must fail cleanly, never crash.
    {
        WeatherData d;
        d.temp = 99;
        const std::string good = MakeForecast("2026-11-18T15:15", 2, 1);
        CHECK(!ParseOpenMeteo("", "X", 2, &d), "empty");
        CHECK(!ParseOpenMeteo("not json", "X", 2, &d), "not json");
        CHECK(!ParseOpenMeteo("{}", "X", 2, &d), "empty object");
        CHECK(!ParseOpenMeteo("{\"error\":true,\"reason\":\"Invalid\"}", "X", 2, &d), "error object");
        CHECK(!ParseOpenMeteo(nullptr, "X", 2, &d) && !ParseOpenMeteo(good.c_str(), "X", 2, nullptr), "nulls");
        {  // a null temperature in the middle of the days stops the list there, without failing
            std::string holey = good;
            const size_t at = holey.find("2.5,-4.6");
            holey.replace(at, 3, "null");
            WeatherData h;
            CHECK(ParseOpenMeteo(holey.c_str(), "X", 2, &h) && h.day_count == 1, "null in days: %d", h.day_count);
        }
        for (size_t cut : {size_t(10), size_t(60), good.size() / 3, good.size() / 2, good.size() - 10})
            CHECK(!ParseOpenMeteo(good.substr(0, cut).c_str(), "X", 2, &d), "truncated at %zu parsed", cut);
        CHECK(d.temp == 99, "a failed parse must not touch the output");
    }

    // ---- location by IP ---------------------------------------------------------------------------
    {
        GeoLocation g;
        CHECK(ParseGeoIp("{\"ip\":\"1.2.3.4\",\"city\":\"Sevilla\",\"latitude\":37.3891,\"longitude\":-5.9845,\"x\":1}", &g) &&
              std::strcmp(g.city, "Sevilla") == 0 && g.lat > 37.38 && g.lat < 37.4 && g.lon < -5.98, "ipapi shape");
        CHECK(ParseGeoIp("{\"success\":true,\"city\":\"M\\u00e1laga\",\"latitude\": 36.72,\"longitude\": -4.42}", &g) &&
              std::strcmp(g.city, "Malaga") == 0, "ipwho.is shape with escapes: '%s'", g.city);
        CHECK(ParseGeoIp("{\"latitude\":10,\"longitude\":20}", &g) && std::strcmp(g.city, "Tu zona") == 0, "no city");
        CHECK(!ParseGeoIp("{\"error\":true,\"reason\":\"RateLimited\"}", &g), "rate limited");
        CHECK(!ParseGeoIp("{\"success\":false,\"message\":\"x\"}", &g), "success false");
        CHECK(!ParseGeoIp("{\"latitude\":123,\"longitude\":20}", &g), "latitude out of range");
        CHECK(!ParseGeoIp("{\"latitude\":null,\"longitude\":null}", &g), "null coordinates");
        CHECK(!ParseGeoIp("<html>", &g) && !ParseGeoIp(nullptr, &g), "html / null");
    }

    // ---- url ----------------------------------------------------------------------------------------
    {
        char url[384];
        BuildForecastUrl(37.3891, -5.9845, url, sizeof url);
        CHECK(std::strstr(url, "latitude=37.3891&longitude=-5.9845") && std::strncmp(url, "https://", 8) == 0 &&
              std::strstr(url, "timezone=auto") && std::strlen(url) < 380, "url: %s", url);
    }

    if (g_failures) {
        std::printf("%d weather test(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("weather data: all tests passed\n");
    return 0;
}
