#include "chipo_weather.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

WeatherKind WeatherKindFromWmo(int code) {
    switch (code) {
    case 0:
    case 1:
        return WeatherKind::Clear;
    case 2:
        return WeatherKind::PartlyCloudy;
    case 3:
        return WeatherKind::Cloudy;
    case 45:
    case 48:
        return WeatherKind::Fog;
    case 51: case 53: case 55: case 56: case 57:
    case 61: case 63: case 65: case 66: case 67:
    case 80: case 81: case 82:
        return WeatherKind::Rain;
    case 71: case 73: case 75: case 77:
    case 85: case 86:
        return WeatherKind::Snow;
    case 95: case 96: case 99:
        return WeatherKind::Thunder;
    default:
        return WeatherKind::Cloudy;
    }
}

const char* WeatherKindText(WeatherKind kind, bool is_day) {
    switch (kind) {
    case WeatherKind::Clear: return is_day ? "Soleado" : "Despejado";
    case WeatherKind::PartlyCloudy: return "Poco nublado";
    case WeatherKind::Cloudy: return "Nublado";
    case WeatherKind::Fog: return "Niebla";
    case WeatherKind::Rain: return "Lluvia";
    case WeatherKind::Snow: return "Nieve";
    case WeatherKind::Thunder: return "Tormenta";
    }
    return "";
}

int WeekdayFromDate(int year, int month, int day) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 1 || month > 12) return 0;
    if (month < 3) year -= 1;
    const int w = (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
    return w < 0 ? w + 7 : w;  // never a negative index, whatever the server sends
}

void AsciiFold(const char* in, char* out, size_t out_size) {
    if (!out || out_size == 0) return;
    // UTF-8 0xC3 0x80..0xBF = Latin-1 letters U+00C0..U+00FF.
    static const char kLatin1[] =
        "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTs"   // U+00C0..U+00DF
        "aaaaaaaceeeeiiiidnooooo/ouuuuyty";  // U+00E0..U+00FF
    size_t n = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(in ? in : ""); *p && n + 1 < out_size;) {
        if (*p < 0x80) {
            out[n++] = static_cast<char>(*p++);
        } else if (*p == 0xC3 && p[1] >= 0x80 && p[1] <= 0xBF) {
            out[n++] = kLatin1[p[1] - 0x80];
            p += 2;
        } else {
            ++p;  // any other multi-byte character: drop it (skip its continuation bytes)
            while ((*p & 0xC0) == 0x80) ++p;
        }
    }
    out[n] = '\0';
}

// ---- minimal JSON scanner ---------------------------------------------------------------------
// The two responses we read are small and have a known shape, so instead of pulling in a JSON
// library (its component name changes between ESP-IDF versions) we walk the text in place:
// no allocation, nothing to free. Every helper tolerates malformed input by returning null.

static const char* SkipWs(const char* p) {
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') ++p;
    return p;
}

// p points just after an opening quote; returns the closing quote (or the end of text).
static const char* EndOfString(const char* p) {
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) ++p;
        ++p;
    }
    return p;
}

// Skips one value starting at p; returns the position just after it, or null if malformed.
static const char* SkipValue(const char* p) {
    p = SkipWs(p);
    if (*p == '"') {
        const char* e = EndOfString(p + 1);
        return *e == '"' ? e + 1 : nullptr;
    }
    if (*p == '{' || *p == '[') {
        int depth = 0;
        for (; *p; ++p) {
            if (*p == '"') {
                p = EndOfString(p + 1);
                if (!*p) return nullptr;
            } else if (*p == '{' || *p == '[') {
                ++depth;
            } else if (*p == '}' || *p == ']') {
                if (--depth == 0) return p + 1;
            }
        }
        return nullptr;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') ++p;
    return p;
}

// Value of `key` among the direct members of the object starting at `obj`.
static const char* Member(const char* obj, const char* key) {
    if (!obj) return nullptr;
    obj = SkipWs(obj);
    if (*obj != '{') return nullptr;
    const size_t key_len = std::strlen(key);
    const char* p = SkipWs(obj + 1);
    while (*p && *p != '}') {
        if (*p != '"') return nullptr;
        const char* ks = p + 1;
        const char* ke = EndOfString(ks);
        if (!*ke) return nullptr;
        p = SkipWs(ke + 1);
        if (*p != ':') return nullptr;
        p = SkipWs(p + 1);
        if (static_cast<size_t>(ke - ks) == key_len && std::strncmp(ks, key, key_len) == 0) return p;
        p = SkipValue(p);
        if (!p) return nullptr;
        p = SkipWs(p);
        if (*p == ',') p = SkipWs(p + 1);
    }
    return nullptr;
}

static const char* Element(const char* arr, int index) {
    if (!arr) return nullptr;
    arr = SkipWs(arr);
    if (*arr != '[') return nullptr;
    const char* p = SkipWs(arr + 1);
    for (int i = 0; *p && *p != ']'; ++i) {
        if (i == index) return p;
        p = SkipValue(p);
        if (!p) return nullptr;
        p = SkipWs(p);
        if (*p == ',') p = SkipWs(p + 1);
    }
    return nullptr;
}

static int ArrayLen(const char* arr) {
    int n = 0;
    while (Element(arr, n)) ++n;
    return n;
}

static bool ReadNum(const char* p, double* out) {
    if (!p) return false;
    p = SkipWs(p);
    if (!(*p == '-' || (*p >= '0' && *p <= '9'))) return false;  // rejects null, nan, inf, hex
    char* end = nullptr;
    const double v = std::strtod(p, &end);
    if (end == p) return false;
    *out = v;
    return true;
}

static bool ReadNumAt(const char* arr, int i, double* out) { return ReadNum(Element(arr, i), out); }

static bool ReadBool(const char* p, bool* out) {
    if (!p) return false;
    p = SkipWs(p);
    if (std::strncmp(p, "true", 4) == 0) { *out = true; return true; }
    if (std::strncmp(p, "false", 5) == 0) { *out = false; return true; }
    return false;
}

static int HexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Copies a JSON string as UTF-8, resolving the usual escapes. Returns false if p is not a string.
static bool ReadStr(const char* p, char* out, size_t out_size) {
    if (!p || out_size == 0) return false;
    p = SkipWs(p);
    if (*p != '"') return false;
    size_t n = 0;
    for (++p; *p && *p != '"'; ++p) {
        char c = *p;
        if (c == '\\' && p[1]) {
            ++p;
            c = *p;
            if (c == 'u') {
                unsigned cp = 0;
                int digits = 0;
                for (; digits < 4 && HexVal(p[1 + digits]) >= 0; ++digits) cp = cp * 16 + HexVal(p[1 + digits]);
                if (digits != 4) return false;
                p += 4;
                if (cp < 0x80) {
                    if (n + 1 < out_size) out[n++] = static_cast<char>(cp);
                } else if (cp < 0x800) {
                    if (n + 2 < out_size) {
                        out[n++] = static_cast<char>(0xC0 | (cp >> 6));
                        out[n++] = static_cast<char>(0x80 | (cp & 0x3F));
                    }
                }  // anything above U+07FF is dropped: the display font could not show it anyway
                continue;
            }
            if (c == 'n' || c == 't' || c == 'r') c = ' ';
        }
        if (n + 1 < out_size) out[n++] = c;
    }
    if (*p != '"') return false;
    out[n] = '\0';
    return true;
}

static int16_t Round16(double v) {
    const long r = std::lround(v);
    return static_cast<int16_t>(r > 32000 ? 32000 : (r < -32000 ? -32000 : r));
}

// "2026-11-17T15:00" -> hour 15 (or -1).
static int HourOf(const char* iso) {
    if (!iso || std::strlen(iso) < 13 || iso[10] != 'T') return -1;
    if (!std::isdigit(static_cast<unsigned char>(iso[11])) || !std::isdigit(static_cast<unsigned char>(iso[12]))) return -1;
    return (iso[11] - '0') * 10 + (iso[12] - '0');
}

bool ParseOpenMeteo(const char* json, const char* city, int hour_step, WeatherData* out) {
    if (!json || !out) return false;
    if (hour_step < 1) hour_step = 1;
    if (!SkipValue(json)) return false;  // the whole document must be there (a cut-off download is rejected)

    const char* cur = Member(json, "current");
    const char* hourly = Member(json, "hourly");
    const char* daily = Member(json, "daily");
    if (!cur || !hourly || !daily) return false;

    WeatherData d;
    double temp, feels, code, is_day;
    char cur_time[24];
    if (!ReadNum(Member(cur, "temperature_2m"), &temp) || !ReadNum(Member(cur, "apparent_temperature"), &feels) ||
        !ReadNum(Member(cur, "weather_code"), &code) || !ReadNum(Member(cur, "is_day"), &is_day) ||
        !ReadStr(Member(cur, "time"), cur_time, sizeof cur_time))
        return false;
    d.temp = Round16(temp);
    d.feels = Round16(feels);
    d.kind = WeatherKindFromWmo(static_cast<int>(code));
    d.is_day = is_day != 0;

    // ---- daily: [0] is today, [1..3] are the next days ---------------------------------------
    const char* d_time = Member(daily, "time");
    const char* d_code = Member(daily, "weather_code");
    const char* d_max = Member(daily, "temperature_2m_max");
    const char* d_min = Member(daily, "temperature_2m_min");
    double hi0, lo0;
    if (!ReadNumAt(d_max, 0, &hi0) || !ReadNumAt(d_min, 0, &lo0)) return false;
    d.hi = Round16(hi0);
    d.lo = Round16(lo0);
    static const char* kDays[] = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};
    for (int i = 0; i < kWeatherDays; ++i) {
        double hi, lo, wc;
        char date[16];
        int y, m, dd;
        if (!ReadNumAt(d_max, i + 1, &hi) || !ReadNumAt(d_min, i + 1, &lo) || !ReadNumAt(d_code, i + 1, &wc) ||
            !ReadStr(Element(d_time, i + 1), date, sizeof date) || std::sscanf(date, "%d-%d-%d", &y, &m, &dd) != 3)
            break;
        WeatherDay& day = d.days[d.day_count++];
        std::snprintf(day.name, sizeof day.name, "%s", kDays[WeekdayFromDate(y, m, dd)]);
        day.hi = Round16(hi);
        day.lo = Round16(lo);
        day.kind = WeatherKindFromWmo(static_cast<int>(wc));
    }

    // ---- hourly: start at the current hour, then every hour_step hours -----------------------
    const char* h_time = Member(hourly, "time");
    const char* h_temp = Member(hourly, "temperature_2m");
    const char* h_code = Member(hourly, "weather_code");
    const char* h_day = Member(hourly, "is_day");
    const int total = ArrayLen(h_time);
    int start = 0;
    for (int i = 0; i < total; ++i) {
        char t[24];
        if (ReadStr(Element(h_time, i), t, sizeof t) && std::strncmp(t, cur_time, 13) == 0) {
            start = i;
            break;
        }
    }
    for (int i = 0; i < kWeatherHours; ++i) {
        const int j = start + i * hour_step;
        double t, wc, dayflag;
        char stamp[24];
        if (j >= total || !ReadNumAt(h_temp, j, &t) || !ReadNumAt(h_code, j, &wc) || !ReadNumAt(h_day, j, &dayflag) ||
            !ReadStr(Element(h_time, j), stamp, sizeof stamp))
            break;
        WeatherHour& h = d.hours[d.hour_count++];
        const int hr = HourOf(stamp);
        if (i == 0) std::snprintf(h.label, sizeof h.label, "Ahora");
        else std::snprintf(h.label, sizeof h.label, "%dh", hr < 0 ? 0 : (hr > 23 ? 23 : hr));
        h.temp = Round16(t);
        h.kind = WeatherKindFromWmo(static_cast<int>(wc));
        h.is_day = dayflag != 0;
    }
    if (d.hour_count == 0 || d.day_count == 0) return false;

    AsciiFold(city ? city : "", d.city, sizeof d.city);
    d.valid = true;
    *out = d;
    return true;
}

bool ParseGeoIp(const char* json, GeoLocation* out) {
    if (!json || !out) return false;
    if (!SkipValue(json)) return false;
    bool err = false;
    if (ReadBool(Member(json, "error"), &err) && err) return false;
    double lat, lon;
    if (!ReadNum(Member(json, "latitude"), &lat) || !ReadNum(Member(json, "longitude"), &lon) ||
        std::fabs(lat) > 90.0 || std::fabs(lon) > 180.0)
        return false;
    GeoLocation g;
    g.lat = lat;
    g.lon = lon;
    char raw[48] = "";
    ReadStr(Member(json, "city"), raw, sizeof raw);  // optional
    AsciiFold(raw, g.city, sizeof g.city);
    if (!g.city[0]) std::snprintf(g.city, sizeof g.city, "Tu zona");
    *out = g;
    return true;
}

bool WeatherIntent(const char* text) {
    if (!text) return false;
    char low[256];
    size_t n = 0;
    for (; text[n] && n + 1 < sizeof low; ++n) {
        const unsigned char c = static_cast<unsigned char>(text[n]);
        low[n] = c < 0x80 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
    }
    low[n] = '\0';
    static const char* kWords[] = {
        "clima",       "pronostico",   "pron\xC3\xB3stico", "temperatura",  "llover",      "lloviendo",
        "lluvia",      "llueve",       "paraguas",           "nevar",        "nevando",     "tiempo hace",
        "tiempo va",   "tiempo hoy",   "tiempo de hoy",      "tiempo manana", "tiempo ma\xC3\xB1" "ana",
        "el tiempo en", "est\xC3\xA1 el tiempo", "esta el tiempo", "hace calor", "hace frio", "hace fr\xC3\xAD" "o",
        "weather",
    };
    for (const char* w : kWords) {
        if (std::strstr(low, w)) return true;
    }
    return false;
}

void BuildForecastUrl(double lat, double lon, char* out, size_t out_size) {
    std::snprintf(out, out_size,
                  "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
                  "&current=temperature_2m,apparent_temperature,weather_code,is_day"
                  "&hourly=temperature_2m,weather_code,is_day"
                  "&daily=weather_code,temperature_2m_max,temperature_2m_min"
                  "&timezone=auto&forecast_days=4",
                  lat, lon);
}
