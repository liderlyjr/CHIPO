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

**V0.3.0 — módulo visual e integración automática publicados.**

El repositorio incluye un componente LVGL chipo_face, integración con XiaoZhi y un flujo de GitHub Actions para compilar el firmware. La compilación y el funcionamiento en la placa deben confirmarse con una ejecución real de Actions y una prueba física.

## Compilar sin instalar nada en Windows

Sigue la [guía paso a paso para compilar con GitHub](docs/BUILD_GITHUB.md). No necesitas PowerShell ni instalar ESP-IDF en tu ordenador.

## Integración con XiaoZhi

Consulta la [guía de integración](docs/XIAOZHI_INTEGRATION.md) y el adaptador de estados en integration/xiaozhi/chipo_state_map.h.

## Altavoz

No conectes un altavoz externo hasta verificar en el esquema y en la revisión física de la placa si existe un transductor integrado y qué salida de audio está disponible. ES8311 es el códec; por sí solo no demuestra que haya un altavoz instalado.
