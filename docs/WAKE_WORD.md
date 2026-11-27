# Prueba de palabra de activación «Jarvis»

**Estado: configurada como experimento; todavía NO probada en la placa.**

## Configuración aplicada

- `CONFIG_USE_CUSTOM_WAKE_WORD=y`
- `CONFIG_SR_MN_CN_NONE=y`
- `CONFIG_SR_MN_EN_MULTINET7_QUANT=y`
- `CONFIG_CUSTOM_WAKE_WORD="jarvis"`
- `CONFIG_CUSTOM_WAKE_WORD_DISPLAY="Chipo"`
- `CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=20`

## Importante: ruta de reconocimiento

CHIPO sigue usando **MultiNet 7 inglés** como detector de palabra personalizada. Se ha cambiado la palabra a `jarvis`; no se ha activado WakeNet ni se ha cargado un modelo español.

La pronunciación inglesa de «Jarvis» puede ser más compatible con el motor inglés, pero eso no garantiza que el detector la reconozca correctamente. No se ha cambiado el umbral, la ganancia del micrófono ni la ruta de audio.

## Estado de versión

La rama de desarrollo configura «Jarvis» como experimento. No se debe describir «Chipo» como palabra ya validada: MultiNet 7 inglés no garantiza que «Jarvis» se detecte bien con pronunciación española. La activación y la transcripción ASR son etapas distintas.

## Cómo probarlo

1. Compilar e instalar el firmware generado desde la rama `release/chipo-os-0.7.0`.
2. Dejar el dispositivo en reposo y decir «Jarvis» varias veces, a distancia normal.
3. Probar también en silencio para comprobar falsas activaciones.
4. Revisar el log de arranque: debe mostrar la palabra configurada y el motor MultiNet activo.

**Criterio de resultado:** si no despierta, no concluimos que el micrófono esté mal; esta es una prueba de palabra personalizada sobre el motor inglés. Si despierta pero luego no entiende lo que dices, eso es un problema distinto del reconocimiento de voz posterior.

## Siguiente paso si falla

Evaluar la integración real de un modelo WakeNet compatible o la personalización de un modelo de activación. No basta con cambiar `CONFIG_CUSTOM_WAKE_WORD`.
