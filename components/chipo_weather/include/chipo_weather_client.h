#pragma once

#include "chipo_weather.h"

// ESP32 only. Blocking: locates CHIPO (fixed place from chipo_weather_config.h, else by public IP),
// downloads the forecast from Open-Meteo over HTTPS and parses it. Call it from a worker task,
// never from the LVGL task. Returns false on any failure (no Wi-Fi, timeout, bad answer).
bool ChipoWeatherFetch(WeatherData* out);
