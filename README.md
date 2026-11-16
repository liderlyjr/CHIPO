# CHIPO OS

CHIPO es un asistente de voz DIY basado en el firmware de XiaoZhi y adaptado a la placa **Waveshare ESP32-S3 Touch AMOLED 1.8 V2**.

## Objetivo de este repositorio

Construir una capa propia de experiencia e interfaz llamada CHIPO OS sin duplicar los controladores que ya proporciona XiaoZhi para la placa.

## Hardware objetivo

- Waveshare ESP32-S3 Touch AMOLED 1.8 V2
- ESP32-S3
- AMOLED 368 × 448
- Controlador de pantalla CO5300
- Audio ES8311
- Gestión de energía AXP2101
- Micrófono integrado

El soporte de placa se debe tomar del perfil oficial existente en XiaoZhi. No se deben inventar pines ni sustituir controladores de la placa.

## Base de firmware

- Proyecto original: https://github.com/78/xiaozhi-esp32
- Perfil de placa: `esp32-s3-touch-amoled-1.8-v2`
- Entorno: Windows + ESP-IDF
- La versión de ESP-IDF debe seguir los requisitos de la revisión actual de XiaoZhi. La documentación de esta primera entrega toma como referencia ESP-IDF 6.0.1 mínimo / rama 6.1 recomendada; verificar de nuevo antes de compilar.

## Estado

**V0.2.1 — módulo visual y guía de integración publicados.**

Incluye el componente LVGL `chipo_face` con ojos, boca, parpadeo y estados visuales. Todavía no es un firmware completo ni un binario listo para flashear. La integración en la pantalla principal y la compilación física son los siguientes pasos.

## Integración con XiaoZhi

Consulta [la guía de integración](docs/XIAOZHI_INTEGRATION.md) y el adaptador de estados en `integration/xiaozhi/chipo_state_map.h`.

## Altavoz

No conectar un altavoz externo hasta verificar en el esquema y en la revisión física de la placa si existe transductor integrado y qué salida de audio está disponible. ES8311 es el códec, no prueba por sí solo que haya un altavoz instalado.
