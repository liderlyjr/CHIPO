# Waveshare tools restoration audit

Date: 2026-11-27

## Factory reference supplied

The supplied Waveshare 2.16 image is:

- `ESP32-S3-Touch-AMOLED-2.16_FactoryOnly_20261030.bin`
- Official location: https://raw.githubusercontent.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16/main/firmware/ESP32-S3-Touch-AMOLED-2.16_FactoryOnly_20261030.bin
- Official SHA-256: `9cde9793955cb26dcc1d92c83b3090afc5f15f75ee8dcd077e2d0ecd481cd5ac`

Waveshare documents this as a complete merged 16 MB recovery image written at flash offset `0x0`. The 2.16 repository explicitly says that source and build instructions for the factory image are not included:
https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16/blob/main/docs/firmware.md

The GitHub file connector can identify the binary but cannot return its binary bytes; the container download path also failed. No extraction or disassembly of this exact 2.16 image has therefore been performed.

## Closest official source found

Waveshare publishes the factory firmware source for ESP32-S3-Touch-AMOLED-2.06 at commit:

`3de8684cc3b27a8b6aa920157ecf399287528181`

Relevant upstream files:

- Gravitysphere: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06/tree/3de8684cc3b27a8b6aa920157ecf399287528181/examples/factory/components/Gravitysphere
- DrawPanel: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06/tree/3de8684cc3b27a8b6aa920157ecf399287528181/examples/factory/components/draw

This is official Waveshare source for a related board generation, not proof of byte-for-byte identity with the 2.16 factory image.

A source search in the published 2.06 factory project found Gravitysphere and DrawPanel, but did not find separate Artificial Horizon or Spirit Level applications. The 2.16 factory source remains unpublished in the repository inspected.

## CHIPO changes in this branch

### Gravitysphere

Ported the published Waveshare 2.06 Gravitysphere worker behavior into CHIPO's LVGL/sensor lifecycle: 200-sample calibration, 0.1 m/s² stability window, up to five attempts with last-window fallback, conversion from CHIPO g samples to m/s², 0.05 m/s² dead zone, original acceleration signs and scale factor 8, and circular confinement using the original 22 px ball and 18 px safety margin. The visual ball uses the upstream 44 px size, pink/orange gradient and pink glow. Touch recalibrates in place of the factory BOOT key, which CHIPO reserves for tool navigation. This is a faithful adaptation of the published 2.06 source, not proof of exact 2.16 source identity.


The DrawPanel presentation and stroke behavior were corrected toward the published Waveshare 2.06 implementation:

- white drawing canvas;
- red, round 10 px strokes;
- continuous joins between touch samples instead of isolated dots;
- round-screen corner clipping;
- 1,000 retained touch points/segments;
- no CHIPO-styled Salir/Borrar overlay; the tool is left through the existing hardware-key navigation.

Implementation note: this is a CHIPO/LVGL port of the published drawing behavior, not a verbatim copy of the Brookesia `Drawpanel` class. The Brookesia application lifecycle is not part of the XiaoZhi base used by CHIPO.

## Not yet restored / not verified

- Gravitysphere: ported from published Waveshare 2.06 source; exact 2.16 source remains unpublished/unverified.
- Spirit Level: no matching original 2.16 source has been located.
- Artificial Horizon: no matching original 2.16 source has been located.
- The 2.16 factory binary has not been extracted or reverse engineered.
- No physical-device validation has been performed.

Do not describe the four tools as fully restored until these remaining items are implemented and tested on the 2.16 board.
