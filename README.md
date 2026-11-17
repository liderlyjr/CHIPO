# CHIPO OS

CHIPO es un asistente de voz DIY basado en el firmware de XiaoZhi y adaptado a la placa **Waveshare ESP32-S3 Touch AMOLED 1.8 V2**.

## Objetivo

Construir una experiencia e interfaz propia llamada CHIPO OS aprovechando los controladores oficiales de XiaoZhi para la placa.

## Hardware objetivo

- Waveshare ESP32-S3 Touch AMOLED 1.8 V2
- ESP32-S3
- AMOLED 368 × 448
- Controlador de pantalla CO5300
- Audio ES8311
- Gestión de energía AXP2101
- Micrófono integrado

No se deben inventar pines ni sustituir controladores de la placa.

## Base de firmware

- Proyecto original: https://github.com/78/xiaozhi-esp32
- Perfil de placa: esp32-s3-touch-amoled-1.8-v2
- Compilación recomendada: GitHub Actions con ESP-IDF 6.1
- Grabación posterior: Espressif ESPTool-JS en el navegador

## Estado

**V0.3.1 — cara más viva y flujo de compilación reproducible.**

El repositorio incluye un componente LVGL `chipo_face`, la integración con XiaoZhi y un flujo de GitHub Actions para compilar el firmware.

Qué se ha comprobado (17/11/2026):

- `scripts/integrate_chipo.py` se aplica sin errores sobre XiaoZhi (commit `aaf1eb7`) y el perfil de placa `..._1_8_v2` existe en su Kconfig.
- `chipo_face` compila sin avisos contra LVGL 9.5.0 (la versión que usa XiaoZhi) y se ha ejecutado en un LVGL de prueba en PC, con AddressSanitizer y UBSan, recorriendo todos los estados, parpadeos y `Stop()`: sin errores ni fugas.

Qué falta por confirmar:

- Una ejecución completa de Actions (compilación del firmware con ESP-IDF 6.1).
- Una prueba física en la placa.

Cada compilación de Actions guarda un `BUILD_INFO.txt` con el commit exacto de CHIPO y de XiaoZhi usados.

## Compilar sin instalar nada en Windows

Sigue la [guía paso a paso para compilar con GitHub](docs/BUILD_GITHUB.md). No necesitas PowerShell ni instalar ESP-IDF en tu ordenador.

## Integración con XiaoZhi

Consulta la [guía de integración](docs/XIAOZHI_INTEGRATION.md) y el adaptador de estados en integration/xiaozhi/chipo_state_map.h.

## Altavoz

No conectes un altavoz externo hasta verificar en el esquema y en la revisión física de la placa si existe un transductor integrado y qué salida de audio está disponible. ES8311 es el códec; por sí solo no demuestra que haya un altavoz instalado.
