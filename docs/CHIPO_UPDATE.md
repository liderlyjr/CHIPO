# Actualizar CHIPO desde GitHub

CHIPO se actualiza solo desde las **Releases de GitHub** de este repositorio. XiaoZhi ya no puede cambiarle el firmware.

## Cómo actualizas tú

1. Subes tus cambios a `main` (merge del PR).
2. Subes el número en `VERSION` (`CHIPO_OS_VERSION=0.6.1`, por ejemplo). **Sin subir ese número no se publica nada.**
3. GitHub Actions compila la **Waveshare 2,16" (480×480)** y, si todo sale bien, crea la Release `v0.6.1` con el archivo `chipo-os.bin`. La Release lleva siempre el firmware de la 2,16": es la placa en uso.
4. CHIPO, conectado a Wi-Fi, mira GitHub unos 90 s después de arrancar y luego cada 6 horas. Si hay una versión **mayor** que la suya y está en reposo (sin conversación), la descarga, la graba y se reinicia.

Resumen: `VERSION` más alto + merge a `main` = CHIPO se actualiza solo.

## Qué necesita

- **La primera vez** hay que grabar a mano (ESPTool-JS, como siempre) una versión que ya incluya el actualizador (0.6.0 o posterior). Antes de eso, CHIPO no sabe actualizarse.
- Que el repositorio sea público (lo es) y que exista al menos una Release.
- Wi-Fi. Sin Wi-Fi no pasa nada, simplemente no mira.

## Por qué XiaoZhi no puede tumbar a CHIPO

`scripts/integrate_chipo.py` aplica tres protecciones:

1. `ota.cc`: la respuesta del servidor de XiaoZhi nunca marca firmware nuevo (ni con `force`).
2. `Application::UpgradeFirmware()`: solo graba una dirección que el actualizador de CHIPO haya autorizado justo antes. Esto bloquea también la herramienta MCP `self.upgrade_firmware`.
3. `PROJECT_VER` se sustituye por la versión de CHIPO (`VERSION`), así que el dispositivo ya no se presenta como "2.6.0".

Los **assets** de XiaoZhi (temas, fuentes, palabras de activación) siguen pudiendo actualizarse: no son firmware.

## Seguridad

- La descarga se hace por HTTPS verificando el certificado (bundle de ESP-IDF).
- Solo se acepta una URL de `github.com/liderlyjr/CHIPO/releases/` y su redirección a un servidor de GitHub (`*.githubusercontent.com`).
- No se instala nunca una versión igual o menor, ni una etiqueta con formato raro (`0.6.0-beta`).
- **El firmware no va firmado.** Quien pueda publicar Releases en el repositorio puede cambiar el firmware de CHIPO: protege tu cuenta de GitHub con 2FA.
- Tras grabar, el arranque normal marca la versión como válida; si la nueva no arranca, ESP-IDF vuelve a la anterior (rollback de XiaoZhi).
- Si falla 3 veces la misma versión, deja de intentarlo hasta el siguiente arranque.

## Ajustes (`components/chipo_update/include/chipo_update_config.h`)

`CHIPO_UPDATE_ENABLED`, `CHIPO_UPDATE_AUTO_INSTALL` (0 = solo avisar en el log), `CHIPO_UPDATE_FIRST_CHECK_MS`, `CHIPO_UPDATE_INTERVAL_MS`, `CHIPO_UPDATE_REPO`, `CHIPO_UPDATE_ASSET`.

## Sin probar en la placa

Los tests de PC cubren comparación de versiones, lectura de la Release, validación de URL y la autorización. Lo que **no** está probado en hardware: la descarga real desde tu red, la redirección de GitHub, el aviso en pantalla durante la grabación (es el de XiaoZhi) y el reinicio posterior.

## Nota sobre la Release v0.6.0
La `v0.6.0` publicada antes de este cambio se compiló para la 1,8" V2, no para la 2,16". CHIPO solo instala versiones **mayores** que la suya, así que una placa con 0.6.0 nunca la descarga. A partir del siguiente número de `VERSION`, la Release es de la 2,16".
