# CHIPO OS

CHIPO es un asistente de voz DIY basado en el firmware de XiaoZhi y adaptado a las placas **Waveshare ESP32-S3 Touch AMOLED 2.16** (480 × 480, la que se está usando ahora) y **1.8 V2** (368 × 448).

## Objetivo

Construir una experiencia e interfaz propia llamada CHIPO OS aprovechando los controladores oficiales de XiaoZhi para la placa.

## Hardware objetivo

| | 2.16" (480 × 480) | 1.8" V2 (368 × 448) |
|---|---|---|
| Microcontrolador | ESP32-S3 | ESP32-S3 |
| Pantalla AMOLED | 480 × 480, controlador CO5300 | 368 × 448, controlador CO5300 |
| Táctil | CST9220 | CST820 |
| Audio | ES8311 + ES7210 (dos micrófonos), altavoz integrado | ES8311, micrófono integrado |
| Gestión de energía | AXP2101 | AXP2101 |
| Sensor de movimiento | QMI8658 | QMI8658 |

Hardware de la 2,16" con caja, altavoz y batería: [docs/HARDWARE_2_16.md](docs/HARDWARE_2_16.md).

No se deben inventar pines ni sustituir controladores de la placa.

## Base de firmware

- Proyecto original: https://github.com/78/xiaozhi-esp32
- Perfiles de placa: `esp32-s3-touch-amoled-1.8-v2` (por defecto al integrar con el script) y `esp32-s3-touch-amoled-2.16`
- **Elegir la 2,16" (480×480)** al integrar:
  `python scripts/integrate_chipo.py <CHIPO> <xiaozhi> --board=2.16` (sin `--board` = 1,8" V2). En Windows: `build_chipo.ps1 -Board 2.16` (por defecto) o `-Board 1.8-v2`.
  El CI compila las dos placas (artefacto `CHIPO-OS-Waveshare-ESP32-S3-AMOLED-2.16`).
  Diferencias que absorbe XiaoZhi: pantalla 480×480 con margen de esquinas, táctil CST9220, ES8311 + ES7210
  (doble micro). Los ajustes se centran en pantallas mayores y la caída de la cara usa el ancho real.
  **Sin probar en la placa física.** La cara, los ajustes, el clima, las escenas, el drive y el reloj de mesa se escalan a la pantalla (ver `docs/CHIPO_FACE.md`).
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
- **FUN** (AMOLED): pantalla de inicio de neón, lámpara de colores, dados y moneda por voz; halo de los ojos más intenso. Ver [docs/CHIPO_FUN.md](docs/CHIPO_FUN.md).
- **AJUSTES**: menú táctil en neón (pulsación larga) con volumen, brillo, batería, Wi-Fi, versión y reinicio; indicador de batería, protector de pantalla AMOLED y toques en la cara. Ver [docs/CHIPO_SETTINGS.md](docs/CHIPO_SETTINGS.md).
- **MASCOTA**: ánimo que sube con tu atención y baja si lo ignoras; "¿cómo estás?", piedra-papel-tijera y adivina el número por voz. Ver [docs/CHIPO_PET.md](docs/CHIPO_PET.md).
- **RECORDATORIOS**: "recuérdame a las 18:30 sacar al perro" (una vez o a diario), guardados en la memoria del dispositivo; y lectura de sensores de Home Assistant. Ver [docs/CHIPO_REMIND.md](docs/CHIPO_REMIND.md) y [docs/CHIPO_HOME_ASSISTANT.md](docs/CHIPO_HOME_ASSISTANT.md).
- **AMBIENTE**: lluvia, fuego o ruido suave de fondo por voz (se apaga a los 30 min) y música a través de un reproductor de Home Assistant. Ver [docs/CHIPO_AMBIENT.md](docs/CHIPO_AMBIENT.md).
- **ANIMACIONES EXTRA**: los ojos siguen tu dedo, corazones y chispas al acariciarlo o decirle "gracias", parpadeo lento de cariño, bostezo con estirón antes de dormir, lágrimas si lo ignoras mucho, rabieta si pierde un juego, Zzz y respiración al dormir, disfraces (gorro de Papá Noel del 1 dic al 6 ene, gafas de sol, bigote) y baile al ritmo del sonido. Ver [docs/CHIPO_ANIMACIONES.md](docs/CHIPO_ANIMACIONES.md).
- **PANTALLAS**: visualizador de sonido (8 bandas reales), acuario y lámpara de lava, por voz. Ver [docs/CHIPO_ANIMACIONES.md](docs/CHIPO_ANIMACIONES.md).
- **TOOLS**: franja de hora y temperatura, anillo de pomodoro, nivel, bola 8, pizarra, subtítulos y agitar para dados/moneda. Ver [docs/CHIPO_TOOLS.md](docs/CHIPO_TOOLS.md).
- **Home Assistant** por voz: luces y escenas (apagado hasta configurar la dirección y el token).
- **Actualización automática** desde las Releases de GitHub.

Todo lo anterior compila en GitHub Actions y pasa sus pruebas en PC; **nada se ha probado todavía en la placa**
(llega el 10 de diciembre de 2026).

## Compilar sin instalar nada en Windows

Sigue la [guía paso a paso para compilar con GitHub](docs/BUILD_GITHUB.md). No necesitas PowerShell ni instalar ESP-IDF en tu ordenador.

## Integración con XiaoZhi

Consulta la [guía de integración](docs/XIAOZHI_INTEGRATION.md) y el adaptador de estados en integration/xiaozhi/chipo_state_map.h.

## Altavoz

La Waveshare 2.16" trae **altavoz integrado** (confirmado por el soporte de Waveshare, ver [docs/HARDWARE_2_16.md](docs/HARDWARE_2_16.md)), así que no se usa altavoz externo. En la 1,8" V2 no se ha verificado: no conectes un altavoz externo hasta confirmarlo en el esquema y en la placa. ES8311 es el códec; por sí solo no demuestra que haya un altavoz instalado. El sonido real no se ha probado en ninguna de las dos placas.
