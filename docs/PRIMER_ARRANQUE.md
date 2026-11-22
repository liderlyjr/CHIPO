# Primer arranque de CHIPO

Guía para el día que llegue la placa. Nada de CHIPO OS se ha probado todavía en la Waveshare 2,16": esta guía sirve
también para ir comprobando, punto por punto, qué funciona y qué no.

## 1. Antes de que llegue (opcional)

- Crear la cuenta en [xiaozhi.me](https://xiaozhi.me) y entrar en la **consola**. Las guías de terceros que he
  consultado dicen que el registro pide un **número de móvil** con código de verificación; no está comprobado que
  acepte un móvil español.
- Crear un agente y pegarle la personalidad de [XIAOZHI_AGENTE.md](XIAOZHI_AGENTE.md).
- Tener el firmware descargado: artefacto `CHIPO-OS-Waveshare-ESP32-S3-AMOLED-2.16` del último CI verde en `main`
  (ver [BUILD_GITHUB.md](BUILD_GITHUB.md)).

Los nombres exactos de los menús de la web pueden haber cambiado: manda lo que veas en pantalla antes que esta guía.

## 2. Grabar el firmware

Sigue [BUILD_GITHUB.md](BUILD_GITHUB.md), apartado «Grabar con ESPTool-JS» (cinco archivos, direcciones `0x0`,
`0x8000`, `0xd000`, `0x20000` y `0x800000`), o `scripts\build_chipo.ps1` si compilas en Windows. Si la placa no entra
en modo de grabación, mantén **BOOT** al conectarla.

## 3. Primer encendido

1. Conecta la placa por USB-C (con la batería puesta si ya la tienes).
2. Debe aparecer la cara de CHIPO. Si la pantalla queda negra o con ruido, pasa al apartado 6 y manda el registro.
3. CHIPO pedirá configurar el **Wi-Fi** (XiaoZhi abre su propia red para hacerlo desde el móvil; el nombre de la red y
   los pasos los muestra la pantalla o el asistente).
4. Al conectarse, CHIPO dirá y mostrará un **código de activación**.
5. En la consola de xiaozhi.me, dentro del agente que quieres usar: **Añadir dispositivo** y escribir el código. Si ya
   no lo oyes, apaga y enciende CHIPO: lo repite al arrancar.
6. Habla con él: di «Chipo» y pregunta algo.

El idioma de la voz (reconocimiento y síntesis) se elige en la web, en el agente. Los textos de la pantalla ya vienen en
español desde el firmware.

## 4. Qué comprobar, por orden

Marca cada punto y apunta lo que falle. Los de arriba condicionan a los de abajo.

| # | Comprobación | Si falla, mira |
|---|---|---|
| 1 | Arranca y aparece la cara, sin parpadeos raros | registro serie (apartado 6) |
| 2 | La cara se mueve con fluidez (parpadeo, mirada) | `docs/CHIPO_FACE.md` |
| 3 | El táctil responde: la mirada sigue el dedo; pulsación larga abre Ajustes | `docs/CHIPO_SETTINGS.md` |
| 4 | Se conecta al Wi-Fi y da el código | registro serie |
| 5 | El altavoz suena (sonido de arranque / respuesta) y el volumen se ajusta | `docs/HARDWARE_2_16.md` |
| 6 | Los micrófonos te oyen: la cara cambia a «escuchando» | `docs/WAKE_WORD.md` |
| 7 | La palabra «Chipo» lo despierta (si no, probar a hablar más cerca o con otra pronunciación) | `docs/WAKE_WORD.md` |
| 8 | Responde por voz en español, y mueve la boca mientras habla | agente en la web |
| 9 | Batería: porcentaje y estado de carga razonables | `docs/HARDWARE_2_16.md` |
| 10 | Inclinar, agitar y girar: reacciona en el sentido correcto (si está invertido, ver `chipo_config.h`) | `docs/CHIPO_FACE.md` |
| 11 | Botón de usuario (GPIO18): cambia entre nivel, bola 8 y pizarra | `docs/CHIPO_TOOLS.md` |
| 12 | Clima, recordatorios y temporizador por voz | `docs/CHIPO_WEATHER.md`, `CHIPO_REMIND.md`, `CHIPO_BUDDY.md` |
| 13 | Con el USB conectado no se apaga solo; con batería se atenúa a los 60 s y se apaga a los 5 min | `docs/HARDWARE_2_16.md` |

Anota también cuánto tarda en contestar desde que terminas de hablar: el servidor de XiaoZhi está lejos y no se ha medido.

## 5. Home Assistant (después)

Solo cuando lo básico funcione. Ver [CHIPO_HOME_ASSISTANT.md](CHIPO_HOME_ASSISTANT.md): se configura la dirección y un
token de acceso; nunca se escriben en el código.

## 6. Si algo falla: qué enviar

Lo más útil es el **registro serie** desde el arranque:

- Con ESP-IDF 6.1 en Windows: `idf.py -p COMx monitor` (cambia `COMx` por el puerto de la placa en el Administrador de
  dispositivos). Para salir, `Ctrl+]`.
- Sin instalar nada: la consola serie de ESPTool-JS (el mismo sitio que se usa para grabar), a 115200.

Al terminar de arrancar CHIPO imprime un **resumen** de unas pocas líneas (busca `boot summary` en el registro): versión,
motivo del reinicio (encendido, pánico, brownout...), memoria libre (interna y PSRAM) y qué módulos arrancaron y
cuáles **FAILED**. Si algo no funciona, copia primero ese bloque. Esto aún no se ha visto en la placa real.

Copia el texto desde que se enciende hasta que ocurre el fallo y, si es visual, una foto de la pantalla. Con eso se
puede corregir sin adivinar.

Lo que Waveshare documenta de la placa, y lo que no, está en [HARDWARE_2_16.md](HARDWARE_2_16.md). El sentido de los
ejes del sensor de movimiento (QMI8658) no está confirmado: si CHIPO reacciona al revés al inclinarlo, se corrige con
`CHIPO_TILT_MIRROR_X` en [chipo_config.h](../components/chipo_face/include/chipo_config.h) (y `CHIPO_LEVEL_MIRROR_X`/`_Y` para el nivel
de burbuja, en `chipo_tools.cpp`) y no hace falta tocar nada más.
