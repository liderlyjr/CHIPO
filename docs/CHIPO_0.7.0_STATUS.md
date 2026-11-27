# CHIPO OS 0.7.0 — seguimiento de correcciones

Rama: `release/chipo-os-0.7.0`

Este documento separa cambios aplicados de tareas pendientes. No equivale a una validación en la placa.

## Aplicado

- **Nivel:** se amplió el rango de lectura. La burbuja ahora alcanza su recorrido completo a 30° (antes 10°) y sigue mostrando ángulo/burbuja hasta 35° de inclinación respecto a la horizontal. A 40° se considera fuera del rango. Se añadieron pruebas para 10°, 30° y 40°. El umbral de nivelado (0,6°) no se modificó.
- **Gravitysphere:** se corrigió el intercambio de ejes en `GravityBallFromAccel()` para seguir la implementación pública de Waveshare 2.06: `x -= accelY * 8` y `y += accelX * 8`. Se actualizaron los tests de mapeo. Esto resuelve el fallo concreto de CI que esperaba que la inclinación X moviese la bola en vertical.
- **DrawPanel:** se desactivaron explícitamente en el canvas las banderas LVGL de scroll, burbujeo de gestos y encadenamiento de scroll horizontal/vertical. El objetivo es impedir que el lienzo se desplace mientras se mantiene un trazo. Pendiente confirmar el comportamiento táctil en la placa.
- **Indicador de batería/carga:** se movió del centro superior al lateral superior derecho para liberar el centro y evitar tapar el reloj/avisos. Pendiente validar visualmente en la placa.
- **Cara de CHIPO:** se aumentó el objetivo de anchura del 58 % al 64 % del panel. Se conserva el límite de altura del 85 % para que el canvas no se salga de la pantalla. En el panel 480×480 el límite de altura hace que el aumento efectivo sea moderado (aprox. 5 %).
- La modificación está en `components/chipo_face/chipo_face.cpp`.

## Pendiente de corregir y validar

- **Horizonte artificial:** la implementación actual calcula roll y pitch directamente de la aceleración filtrada y resta una referencia capturada al tocar. No está demostrado que el mapeo de ejes coincida con la orientación física de la placa ni con el comportamiento original de Waveshare. No invertir ejes ni cambiar escalas sin contrastar la fuente original o una prueba física controlada.
- **Giroscopio:** el QMI8658 se configura actualmente a ±512 °/s y se interpreta en little-endian. Falta contrastar configuración, orden de bytes y escalas con el driver exacto de Waveshare para esta placa; no se declara calibrado.
- **Gravitysphere:** el mapeo de software ahora coincide con Waveshare 2.06. Aún requiere confirmación física en la placa AMOLED 2.16 para comprobar la orientación real del módulo y el sentido visual.
- **Pizarra DrawPanel:** el canvas tiene `CLICKABLE` y `PRESS_LOCK`, pero aún hay que revisar el scroll/gestos de los ancestros y validar que el lienzo no se desplace durante el trazo.
- **Reposo/reloj/logo:** se identificó que el logo central es el `emoji_label_` de XiaoZhi: aunque `SetEmotion()` está anulado para CHIPO, `ShowEmotion()` podía volver a mostrar el glifo robot al limpiar mensajes. El script de integración ahora fuerza ocultar las capas `emoji_label_` y `emoji_image_` también en `ShowEmotion()`. Además, el hook que controla la hora superior ahora considera explícitamente `chipo_buddy_->desk_clock()` para no dibujar la hora de información mientras se activa el reloj lateral BUDDY. CI debe confirmar que los anclajes del script coinciden con la versión XiaoZhi fijada.
- **Cara y estado de carga:** la cara tiene el ajuste de tamaño aplicado y el indicador de batería se movió al lateral superior derecho. Pendiente validar que no se solape con avisos OTA en pantalla real.
- **Reconocimiento de voz / palabra de activación:** sigue pendiente validar físicamente. `scripts/integrate_chipo.py` configura ESP-SR MultiNet 7 inglés con la palabra personalizada `cheepo` y umbral 20. No existe evidencia de que esa palabra esté bien reconocida en español; no bajar el umbral a ciegas. Distinguir si falla la activación «Chipo» o si el texto reconocido aparece pero el asistente interpreta mal la orden. Consultar `docs/WAKE_WORD.md`.

## Verificación

- Cambio de tamaño: commit creado en la rama de trabajo.
- Se han creado commits para la cara, el mapeo Gravitysphere, el bloqueo de scroll del canvas y el indicador lateral. En CI del commit `2245005` falló el test host `mirror y`: la expectativa seguía usando la escala antigua de 10° aunque el nivel ahora usa 30°. Se corrigió la expectativa para la nueva escala en el commit `0cc2fa6`; nueva ejecución `46002431542` en cola. No se ha probado físicamente en la placa.
- No se ha probado en la placa AMOLED.
- La versión 0.6.9 y su release no se modifican.
- No se ha hecho merge a `main` ni publicado una release 0.7.0.
