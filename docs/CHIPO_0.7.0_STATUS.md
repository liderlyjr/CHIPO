# CHIPO OS 0.7.0 — seguimiento de correcciones

Rama: `release/chipo-os-0.7.0`  
PR: [#32](https://github.com/liderlyjr/CHIPO/pull/32) (sigue en Draft)

Este documento distingue cambios de código, pruebas automatizadas y validación física. **CI verde no equivale a firmware validado en la placa.**

## Corregido en código

- **Cara:** objetivo de anchura aumentado del 58 % al 64 %, conservando el límite vertical del 85 %.
- **Indicador de batería/carga:** movido a la esquina superior derecha para dejar libre el centro.
- **XiaoZhi / emociones:** el parche de integración vuelve a ocultar las capas de emoji/logo de XiaoZhi al limpiar mensajes y pausa el GIF ajeno a CHIPO. El hook del reloj superior contempla el modo Desk Clock de BUDDY.
- **Nivel:** escala completa de burbuja a 30°; lectura disponible hasta 35°; el umbral de nivelado sigue en 0,6°. Hay tests a 3°, 10°, 30° y 40°.
- **Gravitysphere:** mapeo de pantalla alineado con el algoritmo público Waveshare: `x -= accelY * 8`, `y += accelX * 8`. La calibración ahora solo se acepta si las 200 muestras son estables; tras cinco intentos fallidos no adopta una referencia incorrecta y permite reintentar tocando la pantalla.
- **Horizonte artificial:** cálculo separado en lógica pura y probado en PC. El ángulo de roll usa diferencia circular para evitar saltos de casi 360° al cruzar el límite ±180°. Las muestras fuera del rango de gravedad se descartan.
- **DrawPanel:** el root y el canvas usan la política de superficie fija de Waveshare: sin scroll, momentum, elasticidad, encadenamiento, burbujeo de gestos/eventos ni barras de desplazamiento; se bloquea la pulsación para mantener el trazo sobre el lienzo.
- **Palabra experimental:** la configuración actual es «Jarvis» en MultiNet 7 inglés; no está validada en la placa ni garantiza reconocimiento en español.

## QMI8658 — configuración documentada

El lector de CHIPO usa el bus I²C ya inicializado por XiaoZhi y comparte sus muestras entre cara, herramientas y DRIVE; no crea un segundo bus. La lectura actual usa auto-incremento y bytes little-endian, acelerómetro ±4 g a 250 Hz, y giroscopio opcional ±512 °/s. El consumidor de UI recibe muestras a 25 Hz (50 Hz en modo DRIVE). No se cambió la configuración del sensor en esta corrección para evitar regresiones sin prueba física.

Referencia de la placa 2.16: [ejemplo QMI8658 de Waveshare](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16/tree/main/examples/arduino/04_LVGL_QMI8658_ui) y [SensorQMI8658.hpp](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16/blob/main/examples/arduino/libraries/SensorLib/src/SensorQMI8658.hpp). La configuración Gravitysphere de Waveshare se usa como referencia de algoritmo, no como prueba de equivalencia de todo el firmware.

## Pendiente de validación física

- Confirmar orientación y signos del nivel, horizonte y Gravitysphere en la AMOLED 2.16.
- Probar táctil de DrawPanel con trazos largos, esquinas y gesto de salida.
- Comprobar que hora superior, logo/emociones y reloj BUDDY no se solapan en reposo y al limpiar mensajes.
- Probar micrófonos, altavoz, activación «Jarvis» y reconocimiento de órdenes por separado.
- Prueba prolongada de estabilidad, memoria PSRAM, batería y actualizaciones OTA.
- Confirmar que no reaparecen los reinicios o bloqueos ya registrados.

## Estado de publicación

- `VERSION` apunta a 0.7.0 para que el candidato se identifique correctamente.
- La rama no se ha fusionado a `main`; no se ha publicado una Release 0.7.0.
- La 0.6.9 y su Release permanecen intactas.
- La PR sigue en Draft hasta cerrar los pendientes y validar en hardware.
