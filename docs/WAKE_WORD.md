# Prueba de palabra de activación «hola»

**Estado: configurada como experimento; todavía NO probada en la placa.**

## Configuración aplicada

- `CONFIG_USE_CUSTOM_WAKE_WORD=y`
- `CONFIG_SR_MN_EN_MULTINET7_QUANT=y`
- `CONFIG_CUSTOM_WAKE_WORD="hola"`
- `CONFIG_CUSTOM_WAKE_WORD_DISPLAY="Chipo"`
- `CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=20`

## Importante: esto no es WakeNet español

La ruta actual de CHIPO usa **MultiNet 7 inglés** como detector de palabra personalizada. Cambiar la cadena a `hola` prueba si ese motor inglés puede detectar esa pronunciación; no activa el modelo español WakeNet `wn10_es_holaesp`.

Por tanto, el resultado puede ser malo o no detectar nada. No se ha cambiado el umbral, la ganancia del micrófono ni la ruta de audio.

## Cómo probarlo

1. Compilar e instalar el firmware generado desde la rama `release/chipo-os-0.7.0`.
2. Dejar el dispositivo en reposo y decir «hola» varias veces, a distancia normal.
3. Probar también en silencio para comprobar falsas activaciones.
4. Revisar el log de arranque: debe mostrar la palabra configurada y el motor MultiNet activo.

**Criterio de resultado:** si no despierta, no concluimos que el micrófono esté mal; esta es una prueba de pronunciación sobre el modelo inglés. Si despierta pero luego no entiende lo que dices, eso es un problema distinto del reconocimiento de voz posterior.

## Siguiente paso si falla

Integrar de verdad `wn10_es_holaesp` por la ruta WakeNet/AFE, siempre que la versión de ESP-SR fijada por el firmware incluya ese asset. No basta con cambiar `CONFIG_CUSTOM_WAKE_WORD`.
