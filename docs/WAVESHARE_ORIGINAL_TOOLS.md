# Waveshare original tools — source audit (2026-11-27)

## Request

Restore the board's original Level, Artificial Horizon, Gravity Ball / Gravitysphere, and DrawPanel tools in CHIPO. Preserve their original appearance and behavior; do not replace them with a CHIPO redesign.

## What was verified

### Target board: ESP32-S3-Touch-AMOLED-2.16

Official repository: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16

The public `main` tree currently publishes factory firmware binaries (including the 2026-10-30 image) but does not publish the factory application source tree. The available factory binary is not source code and cannot be safely merged into the CHIPO source overlay as individual applications.

### Related official source: ESP32-S3-Touch-AMOLED-2.06

Official repository: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06

Factory source at commit `3de8684cc3b27a8b6aa920157ecf399287528181` includes:

- `examples/factory/components/Gravitysphere/Gravitysphere.cpp`
- `examples/factory/components/Gravitysphere/Gravitysphere.hpp`
- `examples/factory/components/draw/Drawpanel.cpp`
- `examples/factory/components/draw/Drawpanel.hpp`

The DrawPanel source is Apache-2.0 and uses a white screen, red continuous strokes of 10 px, and a 1,000-segment cap. Gravitysphere uses the QMI8658 sensor and the Espressif Brookesia application lifecycle.

Important: the 2.06 board is a different product/display (410 x 502) from the target 2.16 board (480 x 480). These files are useful official reference implementations, but they are not verified as the exact 2.16 factory applications. Do not label them as such.

### Level and Artificial Horizon

No original Level or Artificial Horizon application source was found in the public 2.06 factory source search. The public 2.16 repository likewise does not expose its factory application source. CHIPO's current Level and Horizon code in `components/chipo_tools/chipo_tools.cpp` is a CHIPO implementation, not verified Waveshare factory source.

## Integration boundary

CHIPO is integrated as an overlay on XiaoZhi ESP-IDF and uses its own `ChipoTools` LVGL component. Waveshare's official Gravitysphere and DrawPanel are Brookesia `systems::phone::App` applications. They depend on the Brookesia app manager/lifecycle and, for Gravitysphere, the board BSP/QMI8658 API. They cannot be dropped into the current CHIPO component unchanged without bringing in the Brookesia application framework or writing an adapter.

An adapter can preserve the original drawing and sensor logic, but it is still an integration layer and must not be misrepresented as a byte-for-byte unchanged factory app. The 2.16 Level and Horizon cannot be restored exactly without their actual source or a verifiable reference capture/firmware extraction.

## Current status

- No CHIPO tool implementation was changed by this audit.
- No version number was changed.
- No release or OTA was published.
- The stable `main` branch was not modified.
- Work remains on `fix/restore-original-waveshare-tools`.

## Required to finish an exact 2.16 restoration

Obtain the 2.16 factory application source (or a source archive from Waveshare). If source is unavailable, obtain clear screen recordings/photos and interaction details for all four tools from the 2.16 factory firmware; that would support a faithful visual/behavioral recreation, but it would not be the original source.

## Validation still required

Any implementation must pass the ESP-IDF firmware build and host tests, then be tested on the actual 2.16 board for sensor orientation/calibration, touch behavior, round-screen clipping, tool switching, exit/back behavior, and repeated open/close lifecycle. A successful compile alone does not prove factory equivalence or hardware behavior.


## Follow-up reverse engineering and adapter correction (2026-11-27)

The authenticated 2.16 factory binary has now been partially disassembled. The Motion/IMU path checks the QMI8658 identity byte (`0x05`) and writes the CTRL1/CTRL2/CTRL3/CTRL7 configuration values recorded in `WAVESHARE_TOOLS_RESTORATION.md`. The low-level register setup is not yet conclusively tied to the Gravitysphere instance, so it must not be copied into the Gravitysphere app as an assumed setting.

The CHIPO Gravitysphere adapter was also corrected to integrate each raw accelerometer sample once, rather than repeatedly applying the latest UI-smoothed value on every LVGL tick. A host regression test covers the no-new-sample case. This follows the published 2.06 Gravitysphere worker more closely, but it does not establish exact equivalence with the 2.16 factory app.

The earlier statement that no implementation changed applied to the initial audit snapshot; subsequent commits on this branch now include the sampling correction and its regression test. The branch remains a draft PR and no release/OTA or physical-device validation has occurred.
