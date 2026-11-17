#!/usr/bin/env bash
# Host-side tests (no board, no ESP-IDF).
#   tests/host/run.sh                         -> tilt maths only (needs just g++)
#   LVGL_DIR=/path/to/lvgl tests/host/run.sh  -> also builds LVGL and runs the face behaviour test
# Use the LVGL version XiaoZhi uses (9.5.x): git clone --depth 1 --branch v9.5.0 https://github.com/lvgl/lvgl
set -euo pipefail
cd "$(dirname "$0")/../.."
OUT="$(mktemp -d)"
SAN="-fsanitize=address,undefined"
INC="-Icomponents/chipo_face/include"

echo "== weather data (parsers, voice intent)"
g++ -std=c++17 -Wall -Wextra $SAN -Icomponents/chipo_weather/include tests/host/test_weather.cpp components/chipo_weather/chipo_weather_data.cpp -o "$OUT/test_weather"
"$OUT/test_weather"

echo "== update logic (versions, release parsing, URL trust)"
g++ -std=c++17 -Wall -Wextra $SAN -Icomponents/chipo_update/include tests/host/test_update.cpp components/chipo_update/chipo_update_logic.cpp -o "$OUT/test_update"
"$OUT/test_update"

echo "== tilt maths"
g++ -std=c++17 -Wall -Wextra $SAN $INC tests/host/test_tilt.cpp components/chipo_face/chipo_tilt.cpp -o "$OUT/test_tilt"
"$OUT/test_tilt"

if [ -z "${LVGL_DIR:-}" ]; then
    echo "(LVGL_DIR not set: skipping the face behaviour test)"
    exit 0
fi

echo "== face behaviour (LVGL $(basename "$LVGL_DIR"))"
mkdir -p "$OUT/stub" "$OUT/obj"
# Turn the template on (its first guard reads `#if 0 /* Set this to "1" to enable content */`) and
# compile in the two Montserrat sizes the weather screen uses.
sed -e 's/^#if 0 \/\* *Set this to "1" to enable content *\*\//#if 1 \/* enabled *\//' \
    -e 's/^#define LV_FONT_MONTSERRAT_20 0/#define LV_FONT_MONTSERRAT_20 1/' \
    -e 's/^#define LV_FONT_MONTSERRAT_24 0/#define LV_FONT_MONTSERRAT_24 1/' \
    "$LVGL_DIR/lv_conf_template.h" > "$OUT/stub/lv_conf.h"
grep -q '^#if 1 /\* enabled' "$OUT/stub/lv_conf.h" || { echo "could not enable lv_conf template"; exit 1; }
LVFLAGS="-DLV_CONF_INCLUDE_SIMPLE -I$OUT/stub -I$LVGL_DIR"
# Software renderer core only: no GPU/OS back-ends, no SIMD variants, no optional libs.
find "$LVGL_DIR/src" -name '*.c' \
    | grep -v -E "/(drivers|others|demos|examples)/" \
    | grep -v -E "/libs/" \
    | grep -v -E "/draw/(sw/blend/(neon|helium)|nema_gfx|vg_lite|dma2d|nxp|renesas|opengles|eve|espressif|sdl|arm2d)/|stdlib/(micropython|rtthread|uefi)|helium|neon" \
    > "$OUT/sources.txt"
find "$LVGL_DIR/src/libs/bin_decoder" "$LVGL_DIR/src/draw/convert" -name '*.c' 2>/dev/null \
    | grep -v -E "helium|neon" >> "$OUT/sources.txt" || true
sort -u "$OUT/sources.txt" -o "$OUT/sources.txt"
JOBS="$(nproc 2>/dev/null || echo 2)"
xargs -a "$OUT/sources.txt" -P "$JOBS" -I{} sh -c 'gcc -w -O1 '"$SAN"' '"$LVFLAGS"' -c "$1" -o "'"$OUT"'/obj/$(echo "$1" | md5sum | cut -c1-12).o"' _ {}
g++ -std=gnu++23 -O1 -g $SAN -DCHIPO_DEBUG_TILT_LOG=0 $LVFLAGS -Itests/host/stub $INC -w \
    tests/host/test_face.cpp components/chipo_face/chipo_face.cpp components/chipo_face/chipo_tilt.cpp \
    "$OUT"/obj/*.o -o "$OUT/test_face"
"$OUT/test_face"

echo "== weather screen (LVGL $(basename "$LVGL_DIR"))"
g++ -std=gnu++23 -O1 -g $SAN $LVFLAGS $INC -Icomponents/chipo_weather/include -w \
    tests/host/test_weather_ui.cpp components/chipo_weather/chipo_weather_view.cpp \
    components/chipo_weather/chipo_weather_controller.cpp components/chipo_weather/chipo_weather_data.cpp \
    "$OUT"/obj/*.o -o "$OUT/test_weather_ui"
"$OUT/test_weather_ui"
