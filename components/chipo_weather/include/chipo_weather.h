#pragma once

#include <cstddef>
#include <cstdint>

/**
 * CHIPO Weather: data model, parsers and voice intent. Pure C++ (plus cJSON for the parsers),
 * so it builds and is tested on a PC. No LVGL and no network here.
 *
 * Data source: Open-Meteo (https://open-meteo.com), free, no API key.
 */

enum class WeatherKind : uint8_t { Clear, PartlyCloudy, Cloudy, Fog, Rain, Snow, Thunder };

constexpr int kWeatherHours = 5;
constexpr int kWeatherDays = 3;

struct WeatherHour {
    char label[8];  // "Ahora", "17h"
    int16_t temp;
    WeatherKind kind;
    bool is_day;
};

struct WeatherDay {
    char name[8];  // "Mie"
    int16_t lo, hi;
    WeatherKind kind;
};

struct WeatherData {
    bool valid = false;
    char city[40] = "";  // ASCII only (accents folded): the on-screen font has no accents
    int16_t temp = 0, feels = 0, hi = 0, lo = 0;
    WeatherKind kind = WeatherKind::Cloudy;
    bool is_day = true;
    int hour_count = 0;
    WeatherHour hours[kWeatherHours] = {};
    int day_count = 0;
    WeatherDay days[kWeatherDays] = {};
};

struct GeoLocation {
    double lat = 0, lon = 0;
    char city[40] = "";
};

// WMO weather code (Open-Meteo "weather_code") -> icon family.
WeatherKind WeatherKindFromWmo(int code);
// Spanish description, ASCII only ("Poco nublado").
const char* WeatherKindText(WeatherKind kind, bool is_day);

// Open-Meteo /v1/forecast response (see chipo_weather_client for the exact query).
bool ParseOpenMeteo(const char* json, const char* city, int hour_step, WeatherData* out);
// ipapi.co/json response. Returns false on errors such as {"error":true,...}.
bool ParseGeoIp(const char* json, GeoLocation* out);

// "Sevilla" -> "Sevilla", "Málaga" -> "Malaga": the display font only has ASCII.
void AsciiFold(const char* in, char* out, size_t out_size);

// 0 = Sunday ... 6 = Saturday.
int WeekdayFromDate(int year, int month, int day);

// True when a sentence recognised from the user's voice is asking about the weather.
bool WeatherIntent(const char* text);

// Builds the Open-Meteo URL for a place.
void BuildForecastUrl(double lat, double lon, char* out, size_t out_size);
