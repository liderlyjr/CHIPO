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
- **Palabra de activación:** `cheepo` (la que funciona en la placa). «Jarvis» se revirtió en 0.7.1.

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


## 0.7.1 — fluidez (27-11-2026)

La 0.7.0 ya está publicada, y CHIPO solo instala versiones **mayores**, así que la corrección sale como 0.7.1.

- **Horizonte artificial:** antes recalculaba los 230 400 píxeles con coma flotante 20 veces por segundo y volcaba la pantalla entera cada vez. Ahora se dibuja por tramos de fila (`HorizonRender`, en `chipo_tools_logic.cpp`) y no se redibuja si el ángulo cambia menos de 0,15°. Un test compara el resultado con la fórmula antigua píxel a píxel: 0 diferencias en 65 combinaciones de giro e inclinación.
- **Cara bajo una herramienta:** con nivel, gravedad, horizonte o pizarra abiertos, la cara se oculta (`ChipoFace::SetCovered`). Antes seguía animándose a 25 fps debajo, y LVGL volvía a pintar esa zona cada vez. La animación y el sensor siguen funcionando, así que no se pierde nada al volver.
- Sigue sin validarse en la placa: la fluidez real hay que medirla allí.
- **Palabra de activación:** restaurada `cheepo`; la 0.7.0 publicada llevaba `jarvis`.

## 0.7.2 — reloj, clima y tamaño de la cara (27-11-2026)

- **Reloj duplicado:** aparecían dos «23:41» superpuestos: el de CHIPO y el de la barra de estado de XiaoZhi. El parche de integración deja vacío el texto de la barra de XiaoZhi y se queda solo el de CHIPO.
- **Clima:** la ubicación por IP daba Barcelona desde Sabadell y Tarragona desde otra red. Ahora la ubicación es fija (Sabadell, 41,5463 / 2,1086) en `chipo_weather_config.h`; con `CHIPO_WEATHER_CITY ""` vuelve a la ubicación por IP.
- **Cara más grande:** ojos y boca pasan de ~52 % a ~76 % del ancho de pantalla (escala hasta 2,6). Hay que verlo en la placa por si choca con el reloj de arriba o la temperatura de abajo.
