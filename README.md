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

**V0.6.0 — CHIPO se actualiza solo desde las Releases de GitHub y el OTA de XiaoZhi ya no puede cambiarle el firmware. (V0.5.1: pantalla del clima y caída de la cara al inclinar 45°.)**

Consulta [docs/CHIPO_FACE.md](docs/CHIPO_FACE.md) para ver todas las animaciones, cómo reacciona CHIPO cuando
lo inclinas o se cae, y cómo ajustar el sentido de la caída.

Consulta [docs/CHIPO_DRIVE.md](docs/CHIPO_DRIVE.md) para el modo copiloto del coche («Chipo, vámonos»: gafas de sol,
reacciones a aceleración, frenada, curvas y baches; sin probar aún en la placa).

Consulta [docs/CHIPO_BUDDY.md](docs/CHIPO_BUDDY.md) para el temporizador, pomodoro, cronómetro, alarma y modo
dormir con reloj retro (todo por voz).

Consulta [docs/CHIPO_HOME_ASSISTANT.md](docs/CHIPO_HOME_ASSISTANT.md) para controlar luces y escenas de Home
Assistant por voz (apagado hasta configurar la dirección y el token).

Consulta [docs/CHIPO_UPDATE.md](docs/CHIPO_UPDATE.md) para actualizar CHIPO subiendo `VERSION` a GitHub.

Consulta [docs/CHIPO_WEATHER.md](docs/CHIPO_WEATHER.md) para la pantalla del clima (se abre al preguntar por el
clima o con un toque; datos de Open-Meteo, ubicación por IP o fija).

Qué se ha comprobado:

- `scripts/integrate_chipo.py` se aplica sin errores (y repetido, sin cambios) sobre XiaoZhi `aaf1eb7`.
- La pantalla del clima (lectura de datos, voz, toque, caché, errores) pasa sus pruebas en PC.
- La lógica de inclinación y la cara completa pasan sus pruebas en PC con AddressSanitizer y UBSan
  (`tests/host/run.sh`, también en GitHub Actions antes de cada compilación).
- El firmware completo se compila con ESP-IDF 6.1 en GitHub Actions.

Qué falta por confirmar, solo posible con la placa:

- Que el firmware arranca y la cara se ve fluida en la pantalla.
- Que el acelerómetro responde y que la caída va hacia el lado correcto (ver `CHIPO_TILT_MIRROR_X`).
- Que el toque abre la pantalla del clima y que la descarga funciona desde tu red.

Cada compilación de Actions guarda un `BUILD_INFO.txt` con el commit exacto de CHIPO y de XiaoZhi usados.

## Qué hace CHIPO hoy

- **Cara expresiva** hecha de ojos y boca luminosos: estados (escucha, piensa, habla, conecta, error, dormido),
  emociones, parpadeo, y reacciones al giroscopio (inclinar, caerse, asentir, negar, girar, marearse).
- **Asistente de voz** de XiaoZhi, en español, con «Chipo» como palabra de activación.
- **Pantalla del clima** (Open-Meteo) al preguntar por el tiempo o con un toque.
- **Modo DRIVE** para el coche («Chipo, vámonos»): gafas de aviador, escena de carretera y comentarios de copiloto
  según aceleración, frenada, curvas y baches.
- **BUDDY**: temporizador, pomodoro, cronómetro, alarma y modo dormir con reloj de siete segmentos; tumbado de lado se convierte en reloj de mesita.
- **Home Assistant** por voz: luces y escenas (apagado hasta configurar la dirección y el token).
- **Actualización automática** desde las Releases de GitHub.

Todo lo anterior compila en GitHub Actions y pasa sus pruebas en PC; **nada se ha probado todavía en la placa**
(llega el 10 de diciembre de 2026).

## Compilar sin instalar nada en Windows

Sigue la [guía paso a paso para compilar con GitHub](docs/BUILD_GITHUB.md). No necesitas PowerShell ni instalar ESP-IDF en tu ordenador.

## Integración con XiaoZhi

Consulta la [guía de integración](docs/XIAOZHI_INTEGRATION.md) y el adaptador de estados en integration/xiaozhi/chipo_state_map.h.

## Altavoz

No conectes un altavoz externo hasta verificar en el esquema y en la revisión física de la placa si existe un transductor integrado y qué salida de audio está disponible. ES8311 es el códec; por sí solo no demuestra que haya un altavoz instalado.
