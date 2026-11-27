# CHIPO OS 0.7.0 — seguimiento de correcciones

Rama: `release/chipo-os-0.7.0`

Este documento separa cambios aplicados de tareas pendientes. No equivale a una validación en la placa.

## Aplicado

- **DrawPanel:** se desactivaron explícitamente en el canvas las banderas LVGL de scroll, burbujeo de gestos y encadenamiento de scroll horizontal/vertical. El objetivo es impedir que el lienzo se desplace mientras se mantiene un trazo. Pendiente confirmar el comportamiento táctil en la placa.
- **Indicador de batería/carga:** se movió del centro superior al lateral superior derecho para liberar el centro y evitar tapar el reloj/avisos. Pendiente validar visualmente en la placa.
- **Cara de CHIPO:** se aumentó el objetivo de anchura del 58 % al 64 % del panel. Se conserva el límite de altura del 85 % para que el canvas no se salga de la pantalla. En el panel 480×480 el límite de altura hace que el aumento efectivo sea moderado (aprox. 5 %).
- La modificación está en `components/chipo_face/chipo_face.cpp`.

## Pendiente de corregir y validar

- **Horizonte artificial:** la implementación actual calcula roll y pitch directamente de la aceleración filtrada y resta una referencia capturada al tocar. No está demostrado que el mapeo de ejes coincida con la orientación física de la placa ni con el comportamiento original de Waveshare. No invertir ejes ni cambiar escalas sin contrastar la fuente original o una prueba física controlada.
- **Giroscopio:** el QMI8658 se configura actualmente a ±512 °/s y se interpreta en little-endian. Falta contrastar configuración, orden de bytes y escalas con el driver exacto de Waveshare para esta placa; no se declara calibrado.
- **Gravitysphere:** el mapeo X/Y actual es provisional. La prueba física reportada (inclinar hacia abajo y ver la bola moverse a la derecha) requiere resolver orientación antes de fijarlo.
- **Pizarra DrawPanel:** el canvas tiene `CLICKABLE` y `PRESS_LOCK`, pero aún hay que revisar el scroll/gestos de los ancestros y validar que el lienzo no se desplace durante el trazo.
- **Reposo/reloj/logo:** BUDDY dibuja su reloj en una capa LVGL independiente y CHIPO Face dibuja sobre la UI de XiaoZhi. Falta rastrear en la integración de la aplicación qué vista adicional permanece activa cuando aparece el logo y la hora duplicada. No se debe ocultar el logo a ciegas sin identificar su propietario.
- **Cara y estado de carga:** la cara tiene el ajuste de tamaño aplicado; falta mover el indicador de carga a un lateral y comprobar que no se solape con la hora ni con avisos OTA.

## Verificación

- Cambio de tamaño: commit creado en la rama de trabajo.
- Se han creado commits para el aumento de cara y el bloqueo de scroll del canvas; no se ha ejecutado aquí una compilación completa ESP-IDF ni la batería de pruebas host después de esos cambios.
- No se ha probado en la placa AMOLED.
- La versión 0.6.9 y su release no se modifican.
- No se ha hecho merge a `main` ni publicado una release 0.7.0.
