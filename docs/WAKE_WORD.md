# Despertar a CHIPO diciendo «Chipo»

**Estado: la palabra `cheepo` entiende bien «Chipo» en la placa (comprobado por el usuario, 27-11-2026).** Falta medir falsas activaciones con el tiempo.

## Configuración aplicada

- `CONFIG_USE_CUSTOM_WAKE_WORD=y`
- `CONFIG_SR_MN_CN_NONE=y`
- `CONFIG_SR_MN_EN_MULTINET7_QUANT=y`
- `CONFIG_CUSTOM_WAKE_WORD="cheepo"` (una sola unidad fonética, sin separar «chi» y «po»)
- `CONFIG_CUSTOM_WAKE_WORD_DISPLAY="Chipo"`
- `CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=20` (más bajo = salta con más facilidad, más falsos avisos)

## Historial

- 0.6.7 a 0.6.9: `cheepo`.
- 0.7.0: se había cambiado a `jarvis` como experimento, sin que funcionara mejor para el usuario. Se revierte en 0.7.1.

## Notas

CHIPO usa **MultiNet 7 inglés** como detector de palabra personalizada: no es un modelo entrenado con «Chipo», así que puede fallar con ruido u otras voces. La activación y la transcripción posterior son etapas distintas.

Si en algún momento hace falta más fiabilidad: probar otras grafías (`chee po`, `cheepoh`) o integrar un modelo WakeNet entrenado.
