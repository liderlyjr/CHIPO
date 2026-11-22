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
DINC="-Icomponents/chipo_drive/include -Icomponents/chipo_face/include"

echo "== weather data (parsers, voice intent)"
g++ -std=c++17 -Wall -Wextra $SAN -Icomponents/chipo_weather/include tests/host/test_weather.cpp components/chipo_weather/chipo_weather_data.cpp -o "$OUT/test_weather"
"$OUT/test_weather"

echo "== update logic (versions, release parsing, URL trust)"
g++ -std=c++17 -Wall -Wextra $SAN -Icomponents/chipo_update/include tests/host/test_update.cpp components/chipo_update/chipo_update_logic.cpp -o "$OUT/test_update"
"$OUT/test_update"

echo "== gyro gestures (spin, nod, shake)"
g++ -std=c++17 -Wall -Wextra $SAN $INC tests/host/test_gyro.cpp components/chipo_face/chipo_gyro.cpp components/chipo_face/chipo_tilt.cpp -o "$OUT/test_gyro"
"$OUT/test_gyro"

echo "== CHIPO DRIVE voice commands and personality"
g++ -std=c++17 -Wall -Wextra $SAN -Icomponents/chipo_drive/include tests/host/test_drive_logic.cpp components/chipo_drive/chipo_drive_voice.cpp components/chipo_drive/chipo_drive_personality.cpp -o "$OUT/test_drive_logic"
"$OUT/test_drive_logic"

echo "== CHIPO DRIVE motion (simulated drives, hand rotations, bumps, mountings)"
g++ -std=c++17 -Wall -Wextra $SAN $DINC tests/host/test_drive_motion.cpp components/chipo_drive/chipo_drive_motion.cpp -o "$OUT/test_drive_motion"
"$OUT/test_drive_motion"

echo "== CHIPO BUDDY voice commands and time engine"
g++ -std=c++17 -Wall -Wextra $SAN -Icomponents/chipo_buddy/include -Icomponents/chipo_drive/include tests/host/test_buddy_logic.cpp components/chipo_buddy/chipo_buddy_voice.cpp components/chipo_buddy/chipo_buddy_engine.cpp components/chipo_drive/chipo_drive_voice.cpp -o "$OUT/test_buddy_logic"
"$OUT/test_buddy_logic"
g++ -std=c++17 -Wall -Wextra $SAN -Icomponents/chipo_fun/include -Icomponents/chipo_drive/include tests/host/test_fun_logic.cpp components/chipo_fun/chipo_fun_voice.cpp components/chipo_drive/chipo_drive_voice.cpp -o "$OUT/test_fun_logic"
"$OUT/test_fun_logic"

echo "== CHIPO TOOLS logic (shake, level, 8-ball, voice)"
g++ -std=c++17 -Wall -Wextra $SAN -Icomponents/chipo_tools/include -Icomponents/chipo_drive/include tests/host/test_tools_logic.cpp components/chipo_tools/chipo_tools_logic.cpp components/chipo_drive/chipo_drive_voice.cpp -o "$OUT/test_tools_logic"
"$OUT/test_tools_logic"

echo "== Home Assistant requests and spoken commands"
g++ -std=c++17 -Wall -Wextra $SAN -Icomponents/chipo_ha/include -Icomponents/chipo_drive/include tests/host/test_ha_logic.cpp components/chipo_ha/chipo_ha_logic.cpp components/chipo_drive/chipo_drive_voice.cpp -o "$OUT/test_ha_logic"
"$OUT/test_ha_logic"

echo "== IMU task lifecycle (fake FreeRTOS + I2C: Stop with a silent sensor, stuck bus, restarts)"
g++ -std=c++17 -Wall -Wextra $SAN -DESP_PLATFORM -Itests/host/esp_stub -Itests/host/stub $INC tests/host/test_imu_lifecycle.cpp components/chipo_face/chipo_imu.cpp -o "$OUT/test_imu_lifecycle" -pthread
"$OUT/test_imu_lifecycle" | grep -vE "^QMI8658 ready"

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
    -e 's/^#define LV_FONT_MONTSERRAT_32 0/#define LV_FONT_MONTSERRAT_32 1/' \
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
    tests/host/test_face.cpp components/chipo_face/chipo_face.cpp components/chipo_face/chipo_fx.cpp components/chipo_face/chipo_fx_logic.cpp components/chipo_face/chipo_audio_level.cpp components/chipo_face/chipo_tilt.cpp \
    components/chipo_face/chipo_gyro.cpp \
    "$OUT"/obj/*.o -o "$OUT/test_face"

echo "== CHIPO DRIVE on the face (voice on/off, sunglasses, reactions only in the mode)"
g++ -std=gnu++23 -O1 -g $SAN -DCHIPO_DEBUG_TILT_LOG=0 -DCHIPO_DRIVE_DEBUG_LOG=0 $LVFLAGS -Itests/host/stub $DINC -w \
    tests/host/test_drive.cpp components/chipo_face/chipo_face.cpp components/chipo_face/chipo_fx.cpp components/chipo_face/chipo_fx_logic.cpp components/chipo_face/chipo_audio_level.cpp components/chipo_face/chipo_tilt.cpp \
    components/chipo_face/chipo_gyro.cpp components/chipo_drive/chipo_drive.cpp \
    components/chipo_drive/chipo_drive_motion.cpp components/chipo_drive/chipo_drive_voice.cpp \
    components/chipo_drive/chipo_drive_personality.cpp \
    "$OUT"/obj/*.o -o "$OUT/test_drive"
"$OUT/test_drive"
"$OUT/test_face"

echo "== CHIPO BUDDY screen (timer, pomodoro, alarm, bedtime)"
g++ -std=gnu++23 -O1 -g $SAN $LVFLAGS -Itests/host/stub -Icomponents/chipo_buddy/include -Icomponents/chipo_drive/include -w \
    tests/host/test_buddy.cpp components/chipo_buddy/chipo_buddy_voice.cpp components/chipo_buddy/chipo_buddy_engine.cpp \
    components/chipo_buddy/chipo_buddy_view.cpp components/chipo_buddy/chipo_buddy.cpp components/chipo_drive/chipo_drive_voice.cpp \
    "$OUT"/obj/*.o -o "$OUT/test_buddy"
"$OUT/test_buddy"

echo "== chipo_fun (LVGL)"
g++ -std=gnu++23 -O1 -g $SAN $LVFLAGS -Itests/host/stub -Icomponents/chipo_fun/include -Icomponents/chipo_drive/include -w \
    tests/host/test_fun.cpp components/chipo_fun/*.cpp components/chipo_drive/chipo_drive_voice.cpp \
    "$OUT"/obj/*.o -o "$OUT/test_fun"
"$OUT/test_fun"

echo "== chipo_tools (LVGL): info strip, pomodoro ring, level, 8-ball, canvas, subtitles"
g++ -std=gnu++23 -O1 -g $SAN $LVFLAGS -Itests/host/stub -Icomponents/chipo_tools/include -Icomponents/chipo_drive/include -Icomponents/chipo_face/include -w \
    tests/host/test_tools.cpp components/chipo_tools/*.cpp components/chipo_drive/chipo_drive_voice.cpp \
    "$OUT"/obj/*.o -o "$OUT/test_tools"
"$OUT/test_tools"

echo "== Home Assistant controller (async call, result text, off when not configured)"
g++ -std=gnu++23 -O1 -g $SAN $LVFLAGS -Icomponents/chipo_ha/include -Icomponents/chipo_drive/include -w \
    tests/host/test_ha.cpp components/chipo_ha/chipo_ha.cpp components/chipo_ha/chipo_ha_logic.cpp components/chipo_ha/chipo_ha_state.cpp components/chipo_ha/chipo_ha_media.cpp components/chipo_drive/chipo_drive_voice.cpp \
    "$OUT"/obj/*.o -o "$OUT/test_ha"
"$OUT/test_ha"

echo "== weather screen (LVGL $(basename "$LVGL_DIR"))"
# --wrap lets the test make lv_timer_create fail (the "timer cannot be created" cases).
g++ -std=gnu++23 -O1 -g $SAN $LVFLAGS $INC -Icomponents/chipo_weather/include -w -Wl,--wrap=lv_timer_create \
    tests/host/test_weather_ui.cpp components/chipo_weather/chipo_weather_view.cpp \
    components/chipo_weather/chipo_weather_controller.cpp components/chipo_weather/chipo_weather_data.cpp \
    "$OUT"/obj/*.o -o "$OUT/test_weather_ui"
"$OUT/test_weather_ui"

echo "== chipo_ha sensors (state JSON, voice)"
g++ -std=c++17 -Wall -Wextra $SAN -Icomponents/chipo_ha/include -Icomponents/chipo_drive/include tests/host/test_ha_state.cpp components/chipo_ha/chipo_ha_state.cpp components/chipo_ha/chipo_ha_logic.cpp components/chipo_drive/chipo_drive_voice.cpp -o "$OUT/test_ha_state"
"$OUT/test_ha_state"

echo "== chipo_ha media (voice)"
g++ -std=c++17 -Wall -Wextra $SAN -Icomponents/chipo_ha/include -Icomponents/chipo_drive/include tests/host/test_ha_media.cpp components/chipo_ha/chipo_ha_media.cpp components/chipo_ha/chipo_ha_logic.cpp components/chipo_drive/chipo_drive_voice.cpp -o "$OUT/test_ha_media"
"$OUT/test_ha_media"

echo "== chipo_ambient (LVGL)"
g++ -std=gnu++23 -O1 -g $SAN $LVFLAGS -Itests/host/stub -Icomponents/chipo_ambient/include -Icomponents/chipo_drive/include -w \
    tests/host/test_ambient.cpp components/chipo_ambient/*.cpp components/chipo_drive/chipo_drive_voice.cpp \
    "$OUT"/obj/*.o -o "$OUT/test_ambient"
"$OUT/test_ambient"

echo "== chipo_pet logic"
g++ -std=c++17 -Wall -Wextra $SAN -Icomponents/chipo_pet/include -Icomponents/chipo_drive/include tests/host/test_pet_logic.cpp components/chipo_pet/chipo_pet_logic.cpp components/chipo_drive/chipo_drive_voice.cpp -o "$OUT/test_pet_logic"
"$OUT/test_pet_logic"

echo "== chipo_pet (LVGL)"
g++ -std=gnu++23 -O1 -g $SAN $LVFLAGS -Itests/host/stub -Icomponents/chipo_pet/include -Icomponents/chipo_drive/include -w \
    tests/host/test_pet.cpp components/chipo_pet/*.cpp components/chipo_drive/chipo_drive_voice.cpp \
    "$OUT"/obj/*.o -o "$OUT/test_pet"
"$OUT/test_pet"

echo "== chipo_ha_provision.py"
python3 tests/host/test_ha_provision.py

echo "== chipo_remind logic"
g++ -std=c++17 -Wall -Wextra $SAN -Icomponents/chipo_remind/include -Icomponents/chipo_buddy/include -Icomponents/chipo_drive/include tests/host/test_remind_logic.cpp components/chipo_remind/chipo_remind_logic.cpp components/chipo_buddy/chipo_buddy_voice.cpp components/chipo_drive/chipo_drive_voice.cpp -o "$OUT/test_remind_logic"
"$OUT/test_remind_logic"

echo "== chipo_remind (LVGL)"
g++ -std=gnu++23 -O1 -g $SAN $LVFLAGS -Itests/host/stub -Icomponents/chipo_remind/include -Icomponents/chipo_buddy/include -Icomponents/chipo_drive/include -w \
    tests/host/test_remind.cpp components/chipo_remind/*.cpp components/chipo_buddy/chipo_buddy_voice.cpp components/chipo_drive/chipo_drive_voice.cpp \
    "$OUT"/obj/*.o -o "$OUT/test_remind"
"$OUT/test_remind"

echo "== chipo_settings logic"
g++ -std=c++17 -Wall -Wextra $SAN -Icomponents/chipo_settings/include tests/host/test_settings_logic.cpp components/chipo_settings/chipo_settings_logic.cpp -o "$OUT/test_settings_logic"
"$OUT/test_settings_logic"

echo "== CHIPO FX logic (particles, costumes, thanks)"
g++ -std=c++17 -Wall -Wextra $SAN $INC tests/host/test_fx_logic.cpp components/chipo_face/chipo_fx_logic.cpp -o "$OUT/test_fx_logic"
"$OUT/test_fx_logic"

echo "== audio level and spectrum (dance, visualizer)"
g++ -std=c++17 -Wall -Wextra $SAN $INC tests/host/test_audio_level.cpp components/chipo_face/chipo_audio_level.cpp -o "$OUT/test_audio_level"
"$OUT/test_audio_level"

echo "== CHIPO SCENES logic (voice, bars, fish, lava)"
g++ -std=c++17 -Wall -Wextra $SAN -Icomponents/chipo_scenes/include -Icomponents/chipo_drive/include tests/host/test_scenes_logic.cpp components/chipo_scenes/chipo_scenes_logic.cpp components/chipo_drive/chipo_drive_voice.cpp -o "$OUT/test_scenes_logic"
"$OUT/test_scenes_logic"

echo "== CHIPO SCENES screens (LVGL)"
g++ -std=gnu++23 -O1 -g $SAN $LVFLAGS -Itests/host/stub -Icomponents/chipo_scenes/include -Icomponents/chipo_drive/include -w \
    tests/host/test_scenes.cpp components/chipo_scenes/*.cpp components/chipo_drive/chipo_drive_voice.cpp \
    "$OUT"/obj/*.o -o "$OUT/test_scenes"
"$OUT/test_scenes"

echo "== chipo_settings (LVGL)"
g++ -std=gnu++23 -O1 -g $SAN $LVFLAGS -Itests/host/stub -Icomponents/chipo_settings/include -w -Wl,--wrap=lv_timer_create \
    tests/host/test_settings.cpp components/chipo_settings/chipo_settings.cpp components/chipo_settings/chipo_settings_logic.cpp \
    "$OUT"/obj/*.o -o "$OUT/test_settings"
"$OUT/test_settings"
