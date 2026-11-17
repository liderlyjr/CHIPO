# Despertar a CHIPO diciendo "Chipo"

**Estado: configurado y se compila, pero NO probado en la placa.**

- Hasta la versión 0.4.0 el aviso por voz estaba desactivado (XiaoZhi lo trae apagado).
- Ahora se activa una palabra personalizada con el modelo MultiNet inglés de ESP-SR.
  La palabra se escribe como suena: `chee po` (ajuste `CONFIG_CUSTOM_WAKE_WORD` en `scripts/integrate_chipo.py`).
- Sensibilidad: `CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=20` (más bajo = salta con más facilidad, más falsos avisos).

## Por qué hay que probarlo

Espressif no tiene un modelo entrenado con "Chipo". Esto reconoce una palabra sin entrenar, así que puede fallar con tu voz
o con ruido. En el monitor serie, al arrancar, se imprime la lista de palabras activas.

Si no funciona bien, hay dos salidas:
1. Probar otras grafías: `chi po`, `chee poh`, `cheepo`.
2. Usar un modelo ya entrenado de Espressif (mucho más fiable), por ejemplo "Hi ESP" o "Jarvis".

Dime cómo responde en la placa y lo ajusto.
