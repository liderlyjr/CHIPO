# Waveshare tools restoration audit

Date: 2026-11-27

## Factory reference supplied and verified

The supplied Waveshare 2.16 image is:

- `ESP32-S3-Touch-AMOLED-2.16_FactoryOnly_20261030.bin`
- Official location: https://raw.githubusercontent.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16/main/firmware/ESP32-S3-Touch-AMOLED-2.16_FactoryOnly_20261030.bin
- Expected size: 16,777,216 bytes (16 MiB)
- Official SHA-256: `9cde9793955cb26dcc1d92c83b3090afc5f15f75ee8dcd077e2d0ecd481cd5ac`

The user verified the local file on Windows PowerShell. Its size and SHA-256 match the official image exactly. `esptool v5.7.0 image-info` reports ESP32-S3, 16 MB flash, DIO/80 MHz, valid checksum and validation hash, ESP-IDF v6.1, build time Oct 29 2026 18:30:14.

Waveshare documents this as a complete merged 16 MB recovery image written at flash offset `0x0`. The 2.16 repository says factory source/build instructions are not included:
https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16/blob/main/docs/firmware.md

## Binary inspection results (user-run extraction)

The user ran a Python inspection script against the verified 16 MiB image. A valid ESP-IDF partition table was found at flash offset `0x8000`. Relevant entries:

- `factory`: type `0x00`, subtype `0x00`, offset `0x00200000`, size `0x00800000` (8 MiB)
- `model`: offset `0x00110000`, size `0x000f0000`
- `storage`: offset `0x00a00000`, size `0x00600000`

The string scan extracted 35,407 printable strings. Important evidence from the factory firmware:

- At file offset `0x0020AB4C`: `./components/Gravitysphere/Gravitysphere.cpp`
- Nearby strings: `BS:Gravitysphere`, `gravitysphere`, `performLevelCalibration`, `Level set here`, `Level reset`
- At file offset `0x0020B2A0`: `Horizon`
- At file offset `0x0020BA1C`: `DrawPanel`
- At file offset `0x0020C1D4`: `./components/draw/Drawpanel.cpp`
- Nearby class symbols include the mangled C++ class names for `Gravitysphere` and `Drawpanel`.
- Other relevant string: `Invalid horizon direction threshold`.

This confirms the exact 2.16 factory application contains Gravitysphere and DrawPanel code/source-path metadata and contains Level/Horizon application labels and horizon-related implementation diagnostics. It is much stronger evidence than the earlier 2.06 source comparison. It does NOT by itself recover the original C++ source or prove visual/behavioral equivalence of the current CHIPO ports.

The script also created `factory_strings.txt` in the user's Downloads folder. No source files have yet been reconstructed from the binary. The next reverse-engineering step is to extract the `factory` partition (flash offset `0x200000`, length `0x800000`) and inspect its code, symbols, constants, and relevant strings. No firmware has been flashed.


### Additional factory-binary evidence supplied on 2026-11-27

The user supplied the contextual string extraction around the relevant offsets. This confirms and refines the earlier scan:

- Gravitysphere source-path string at full-flash offset `0x0020AB4C`, followed by `BS:Gravitysphere`, `gravitysphere`, `performLevelCalibration`, `releaseImu`, and `~Gravitysphere`.
- Gravitysphere diagnostics explicitly mention QMI8658 initialization, accelerometer range/rate, filtering, failed reads, calibration attempts with no samples, unstable samples, and fallback to the latest calibration sample set.
- Its calibration UI strings include `Keep the board level`, `Calibrating %d%%`, `Sensor read failed`, `Retrying...`, and `Tap the screen to recalibrate`.
- Motion source-path string at `0x0020B038` is `./components/Motion/Motion.cpp`. Nearby strings include `readSensor`, `startSensor`, `buildUi`, `X %+.1f`, `Y %+.1f`, `PITCH`, `Level set here`, `Level reset`, `|a| %.2f g`, `ACCEL g`, `GYRO`, `Level`, and `Horizon`.
- The Motion component also contains `Tilt to roll`, `tap to bring it back`, and `Motion sensor not found`. These are evidence of additional motion UI behavior, but do not yet establish which exact screen uses each string.
- DrawPanel source-path string at `0x0020C1D4` is `./components/draw/Drawpanel.cpp`, followed by `BS:Drawpanel` and `Create canvas buffer failed`.
- The binary contains mangled C++ class names `N13esp_brookesia4apps13GravitysphereE`, `N13esp_brookesia4apps9DrawpanelE`, and `N13esp_brookesia4apps6MotionE`. The Gravitysphere and Drawpanel plugin registrar symbols are also present.

Interpretation: Gravitysphere and DrawPanel are separate Brookesia applications in this exact 2.16 factory build. Level and Horizon appear to be screens/features implemented inside the separate `Motion` application/component, rather than separate registered apps. This is a strong structural inference from source-path and class metadata, not a complete reconstruction of their rendering or interaction logic.

No source code has been recovered from these strings alone. A disassembly/decompilation pass over the extracted 8 MiB `factory` app partition is still required to reconstruct exact behavior. The user has not flashed the image.

## Closest official source found

Waveshare publishes factory firmware source for ESP32-S3-Touch-AMOLED-2.06 at commit:

`3de8684cc3b27a8b6aa920157ecf399287528181`

Relevant upstream files:

- Gravitysphere: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06/tree/3de8684cc3b27a8b6aa920157ecf399287528181/examples/factory/components/Gravitysphere
- DrawPanel: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06/tree/3de8684cc3b27a8b6aa920157ecf399287528181/examples/factory/components/draw

This is official Waveshare source for a related board generation, not proof of byte-for-byte identity with the 2.16 factory image.

## CHIPO changes in this branch

### Gravitysphere

Ported the published Waveshare 2.06 Gravitysphere worker behavior into CHIPO's LVGL/sensor lifecycle: 200-sample calibration, 0.1 m/s² stability window, up to five attempts with last-window fallback, conversion from CHIPO g samples to m/s², 0.05 m/s² dead zone, original acceleration signs and scale factor 8, and circular confinement using the original 22 px ball and 18 px safety margin. The visual ball uses the upstream 44 px size, pink/orange gradient and pink glow. Touch recalibrates in place of the factory BOOT key, which CHIPO reserves for tool navigation. This is a faithful adaptation of the published 2.06 source, not yet proven identical to the 2.16 factory implementation.

### DrawPanel

The DrawPanel presentation and stroke behavior were corrected toward the published Waveshare 2.06 implementation:

- white drawing canvas;
- red, round 10 px strokes;
- continuous joins between touch samples instead of isolated dots;
- round-screen corner clipping;
- 1,000 retained touch points/segments;
- no CHIPO-styled Salir/Borrar overlay; the tool is left through the existing hardware-key navigation.

Implementation note: this is a CHIPO/LVGL port of the published 2.06 drawing behavior, not a verbatim copy of the Brookesia `Drawpanel` class. The Brookesia application lifecycle is not part of the XiaoZhi base used by CHIPO.

## Not yet restored / not verified

- The exact 2.16 factory binary has been authenticated and its partition table and printable strings inspected.
- Gravitysphere and DrawPanel are confirmed present in the 2.16 factory app, but their original source has not yet been recovered.
- Level and Horizon are confirmed by factory app labels and horizon-related diagnostic strings; their original implementations have not yet been recovered.
- No complete disassembly, decompilation, or source reconstruction has been performed.
- No physical-device validation has been performed.

Do not describe the four tools as fully restored until the original implementations have been recovered or faithfully reconstructed and tested on the 2.16 board.
