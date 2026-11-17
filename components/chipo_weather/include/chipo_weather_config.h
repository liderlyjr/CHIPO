#pragma once

/**
 * CHIPO Weather tunables. Every value can be overridden with a compiler definition
 * (e.g. -DCHIPO_WEATHER_CITY=\"Sevilla\") without touching this file.
 */

// Fixed location. Leave CHIPO_WEATHER_CITY empty to locate CHIPO by its public IP address
// (approximate: it usually gives your provider's city, which may not be yours).
// To fix the place, set the three together, e.g.
//   -DCHIPO_WEATHER_CITY=\"Sevilla\" -DCHIPO_WEATHER_LAT=37.39 -DCHIPO_WEATHER_LON=-5.99
#ifndef CHIPO_WEATHER_CITY
#define CHIPO_WEATHER_CITY ""
#endif
#ifndef CHIPO_WEATHER_LAT
#define CHIPO_WEATHER_LAT 0.0
#endif
#ifndef CHIPO_WEATHER_LON
#define CHIPO_WEATHER_LON 0.0
#endif

// How long the weather screen stays up once the data is on screen. A touch closes it earlier.
#ifndef CHIPO_WEATHER_SHOW_MS
#define CHIPO_WEATHER_SHOW_MS 20000
#endif

// Data younger than this is reused instead of downloading again.
#ifndef CHIPO_WEATHER_CACHE_MS
#define CHIPO_WEATHER_CACHE_MS (10 * 60 * 1000)
#endif

// Give up (and show an error) when the download takes longer than this.
#ifndef CHIPO_WEATHER_FETCH_TIMEOUT_MS
#define CHIPO_WEATHER_FETCH_TIMEOUT_MS 12000
#endif

// Hours between the columns of the hourly strip.
#ifndef CHIPO_WEATHER_HOUR_STEP
#define CHIPO_WEATHER_HOUR_STEP 2
#endif
