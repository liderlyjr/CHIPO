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


## Diagnóstico de la placa Waveshare 2.16 (26-11-2026)

El log serie confirma que el detector se inicializa con:

- Modelo MultiNet: `mn7_en` (inglés).
- Palabra configurada: `chee po`.
- Umbral: `0.20` (`CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=20`).
- Comando activo impreso por MultiNet: `pm Pb`.

Esto explica por qué no debemos bajar el umbral a ciegas: el modelo inglés no está entrenado con «CHIPO» y su salida fonética interna no coincide de forma clara con el nombre. Un umbral menor podría aumentar las falsas activaciones sin resolver el reconocimiento.

**Próxima corrección segura:** comparar variantes de grafía con el mismo umbral y con pruebas de voz reales, revisar si hay un modelo español compatible en los assets del firmware y medir activaciones falsas con silencio/ruido ambiente. Solo cambiar el modelo o el umbral después de esa comparación. La detección con BOOT confirma que la ruta manual de micrófono funciona, pero no valida el detector en reposo.

El aviso `wakenet model not found` aparece porque esta configuración utiliza MultiNet como detector personalizado y no un modelo WakeNet; por sí solo no demuestra que el detector esté apagado. La prueba física de «CHIPO» sigue pendiente.
