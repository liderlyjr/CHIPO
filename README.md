# CHIPO OS

CHIPO es un pequeño ordenador de sobremesa con aire de Macintosh clásico y un asistente de voz dentro. CHIPO OS es su
firmware: parte de [XiaoZhi](https://github.com/78/xiaozhi-esp32), que pone la voz, el Wi-Fi y los controladores, y le
añade una cara animada, pantallas propias, juegos, temporizadores y Home Assistant.

**Placa: Waveshare ESP32-S3 Touch AMOLED 2.16** (pantalla cuadrada de 480 × 480). Es la única placa soportada.

> **Estado: V1.0.0. Compila en GitHub Actions y pasa 29 grupos de pruebas en PC, pero todavía no se ha probado en la
> placa física.** Lo que solo se puede confirmar con ella está en [Qué falta por probar](#qué-falta-por-probar).

## Hardware

| | Waveshare ESP32-S3 Touch AMOLED 2.16 |
|---|---|
| Microcontrolador | ESP32-S3, 16 MB de flash, 8 MB de PSRAM |
| Pantalla | AMOLED 480 × 480, controlador CO5300 |
| Táctil | CST9220 |
| Audio | Códec ES8311, ADC ES7210 con dos micrófonos, **altavoz integrado** |
| Energía | AXP2101; la batería viene en la caja (capacidad sin confirmar) |
| Almacenamiento | Ranura para tarjeta TF (no la usa CHIPO) |
| Sensores | QMI8658 (acelerómetro y giroscopio, sin magnetómetro), reloj PCF85063 |
| Botones | BOOT (GPIO0), PWR (al AXP2101), botón de usuario GPIO18 |

La placa viene montada en su caja de 46 × 46 × 22,5 mm. Detalles, fuentes y lo que no está documentado:
[docs/HARDWARE_2_16.md](docs/HARDWARE_2_16.md).

## Qué hace CHIPO

- **Cara expresiva** de ojos y boca luminosos: escucha, piensa, habla, conecta, error y duerme; emociones, parpadeo y
  reacciones al movimiento (inclinar, caerse, asentir, negar, girar, marearse). [CHIPO_FACE](docs/CHIPO_FACE.md)
- **Asistente de voz** de XiaoZhi en español, con «Chipo» como palabra de activación. [WAKE_WORD](docs/WAKE_WORD.md)
- **Clima** de Open-Meteo al preguntar por el tiempo o con un toque. [CHIPO_WEATHER](docs/CHIPO_WEATHER.md)
- **BUDDY**: temporizador, pomodoro, cronómetro, alarma y modo dormir con reloj; tumbado de lado es un reloj de
  mesita. [CHIPO_BUDDY](docs/CHIPO_BUDDY.md)
- **RECORDATORIOS** por voz («recuérdame a las 18:30 sacar al perro»), guardados en el dispositivo.
  [CHIPO_REMIND](docs/CHIPO_REMIND.md)
- **TOOLS**: hora y temperatura alrededor de la cara, anillo del pomodoro, nivel de burbuja, bola 8 mágica, pizarra
  para dibujar, subtítulos y el botón GPIO18 para cambiar de herramienta. [CHIPO_TOOLS](docs/CHIPO_TOOLS.md)
- **FUN**: pantalla de inicio de neón, lámpara de colores, dados y moneda (también agitándolo). [CHIPO_FUN](docs/CHIPO_FUN.md)
- **MASCOTA**: ánimo que sube con tu atención; piedra-papel-tijera y adivina el número. [CHIPO_PET](docs/CHIPO_PET.md)
- **ANIMACIONES Y PANTALLAS**: los ojos siguen tu dedo, corazones, bostezos, lágrimas, rabietas, disfraces, baile;
  visualizador de sonido, acuario y lámpara de lava. [CHIPO_ANIMACIONES](docs/CHIPO_ANIMACIONES.md)
- **AMBIENTE**: lluvia, fuego o ruido suave de fondo. [CHIPO_AMBIENT](docs/CHIPO_AMBIENT.md)
- **DRIVE**: modo copiloto para el coche («Chipo, vámonos»). [CHIPO_DRIVE](docs/CHIPO_DRIVE.md)
- **AJUSTES** táctiles (pulsación larga): volumen, brillo, batería, Wi-Fi, versión y reinicio; protector de pantalla
  AMOLED y cara cansada con poca batería. [CHIPO_SETTINGS](docs/CHIPO_SETTINGS.md)
- **Home Assistant** por voz: luces, escenas, sensores y música (apagado hasta configurar dirección y token).
  [CHIPO_HOME_ASSISTANT](docs/CHIPO_HOME_ASSISTANT.md)
- **Siempre encendido cuando hace falta**: con algo pendiente (alarma, temporizador, recordatorio, reloj...) o
  enchufado, no se apaga solo. [HARDWARE_2_16](docs/HARDWARE_2_16.md#energía-atenuar-y-apagarse-solo)
- **Actualización automática** desde las Releases de este repositorio. [CHIPO_UPDATE](docs/CHIPO_UPDATE.md)

## Compilar

- **Sin instalar nada:** GitHub compila el firmware. Guía paso a paso: [docs/BUILD_GITHUB.md](docs/BUILD_GITHUB.md).
- **En Windows con ESP-IDF 6.1:** `scripts\build_chipo.ps1`. Guía: [docs/BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md).
- **A mano:** `python scripts/integrate_chipo.py <carpeta CHIPO> <checkout de XiaoZhi>` y después
  `idf.py set-target esp32s3 && idf.py build`.

La versión de XiaoZhi está fijada en [`XIAOZHI_REF`](XIAOZHI_REF): GitHub Actions y el script de Windows usan
exactamente la base con la que se comprobaron los parches. El artefacto de cada compilación se llama
`CHIPO-OS-Waveshare-ESP32-S3-AMOLED-2.16` e incluye `BUILD_INFO.txt` con los commits exactos de CHIPO y de XiaoZhi.

## Actualizar

Se sube el número de [`VERSION`](VERSION) y se fusiona en `main`. GitHub publica la Release con
`chipo-os-2.16.bin` y CHIPO la instala solo cuando está en reposo. El servidor de XiaoZhi no puede cambiarle el
firmware. Detalles y seguridad: [docs/CHIPO_UPDATE.md](docs/CHIPO_UPDATE.md).

## Cómo se integra con XiaoZhi

CHIPO no sustituye XiaoZhi: copia sus componentes (`components/chipo_*`) y aplica parches pequeños y comprobados a
`application.cc/.h`, `audio_service.cc`, `ota.cc`, `CMakeLists.txt` y `sdkconfig.defaults`. Si XiaoZhi cambia y un
punto de anclaje no coincide, el script se detiene en lugar de parchear a ciegas. Lista de cambios:
[docs/XIAOZHI_INTEGRATION.md](docs/XIAOZHI_INTEGRATION.md).

## Qué está comprobado

- El script de integración se aplica sin errores sobre XiaoZhi `92d400d` y, ejecutado otra vez, no cambia nada.
- 29 grupos de pruebas en PC, a 480 × 480, con AddressSanitizer y UBSan (`tests/host/run.sh`, también en GitHub
  Actions antes de cada compilación).
- El firmware completo compila con ESP-IDF 6.1 en GitHub Actions.

## Qué falta por probar

Solo es posible con la placa:

- Que arranca y la cara se ve fluida en la pantalla de 480 × 480.
- Micrófonos, altavoz, palabra de activación «Chipo» y volumen de los sonidos.
- Táctil, botón GPIO18 y sentido de los ejes del QMI8658 (ajustables en `chipo_config.h`).
- Lectura de batería, consumo y si el AXP2101 apaga la placa con el USB conectado.
- Clima, Home Assistant y actualizaciones desde tu red.

## Altavoz

La 2,16" trae altavoz integrado (confirmado por el soporte de Waveshare), así que no se usa altavoz externo. Ver
[docs/HARDWARE_2_16.md](docs/HARDWARE_2_16.md).
