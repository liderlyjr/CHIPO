# CHIPO 0.6.8 — registro de integración de herramientas

**Rama de trabajo:** `feature/0.6.8-four-tools`  
**Base:** `main` (sin modificar)  
**Estado de esta rama:** trabajo parcial; todavía no es firmware compilado ni publicable.

## Objetivo acordado

La 0.6.8 debe incorporar cuatro herramientas en la pantalla AMOLED de 480 × 480, usando el botón físico GPIO18 como único control físico adicional:

1. **Nivel:** sustituir la herramienta de nivel actual por la implementación/experiencia basada en el ejemplo de nivel de Waveshare, adaptada a CHIPO y al sensor QMI8658.
2. **Pizarra:** sustituir la pizarra actual por DrawPanel de Waveshare, adaptado a la pantalla táctil y a la interfaz CHIPO.
3. **Gravity Ball:** nueva aplicación; una pelota debe desplazarse de forma suave según la inclinación de CHIPO.
4. **Horizonte artificial:** nueva aplicación con cielo y tierra, línea de horizonte y referencia fija; el horizonte debe rotar con el roll y desplazarse con el pitch.

Se confirmó expresamente que **la bola mágica debe eliminarse**. No debe reaparecer ni ocupar un paso del ciclo GPIO18.

## Reglas de interfaz y compatibilidad

- Mantener fondo AMOLED negro fuera de las zonas dibujadas por cada herramienta.
- Mostrar únicamente la interfaz de CHIPO; no recuperar ventanas, textos ni capas visuales de XiaoZhi.
- Reutilizar el QMI8658 y la ruta IMU que ya existen en CHIPO; no inicializar un segundo driver para el mismo sensor.
- No añadir botones físicos.
- Mantener intacta la versión estable 0.6.7 mientras se desarrolla y valida esta rama.
- No publicar una OTA hasta que las pruebas y la compilación de firmware pasen.

## Ciclo previsto de GPIO18

`Ninguna → Nivel → Gravity Ball → Horizonte artificial → Pizarra → Ninguna`

La salida de cada herramienta debe devolver el control a la pantalla principal sin dejar capas o temporizadores activos.

## Referencias técnicas localizadas

- Waveshare ESP32-S3 Touch AMOLED 1.75: componentes `Gravitysphere` y `draw/Drawpanel`, localizados como referencias para adaptar Gravity Ball y la pizarra. La placa de ese ejemplo no es la pantalla CHIPO 2.16; no se debe copiar sin adaptar resolución, dependencias y hardware.
- `emwstudio/esp32-attitude`: referencia localizada para el horizonte artificial y la fusión de sensores en la variante Waveshare AMOLED 2.16. Antes de reutilizar código literal hay que verificar su licencia; si no autoriza la reutilización, implementar la lógica de forma independiente.
- El firmware CHIPO ya contiene lectura del QMI8658 y una lógica básica de nivel. Debe aprovecharse esa ruta existente.

## Cambios realmente realizados en esta rama hasta este registro

- Se creó la rama `feature/0.6.8-four-tools` desde `main`.
- En `components/chipo_tools/include/chipo_tools.h` se cambió el enum de herramientas para declarar `Level`, `GravityBall`, `Horizon` y `Canvas`, retirando `Ball`; se añadieron campos de estado y declaraciones para las nuevas pantallas.
- En `components/chipo_tools/include/chipo_tools_logic.h` se retiraron las declaraciones de la bola mágica y se añadieron intenciones de voz para Gravity Ball y horizonte.
- En `components/chipo_tools/chipo_tools_logic.cpp` se retiró la tabla/lógica de respuestas de la bola mágica y se añadieron reconocedores de voz básicos para Gravity Ball y horizonte.

**Importante:** esos cambios son todavía parciales. En el momento de escribir este registro, el archivo de implementación `components/chipo_tools/chipo_tools.cpp` no ha sido adaptado ni guardado en esta rama. Por tanto, las cuatro aplicaciones todavía no están implementadas y el ciclo GPIO18 no está terminado.

## Pendiente antes de considerar 0.6.8 lista

- Implementar el nuevo nivel y su calibración/lectura en pantalla.
- Integrar y adaptar DrawPanel, incluyendo táctil, borrado y salida.
- Implementar Gravity Ball con movimiento estable y límites de pantalla.
- Implementar el horizonte artificial con roll/pitch y una referencia visual fija.
- Completar el ciclo GPIO18 y retirar todas las rutas, eventos, estados, textos y pruebas de la bola mágica.
- Revisar memoria PSRAM, liberación de buffers, temporizadores LVGL y limpieza de pantallas al cambiar de herramienta.
- Actualizar pruebas host y ejecutar CI/compilación ESP-IDF.
- Verificar la OTA y el comportamiento físico en la placa. Una compilación correcta no sustituye esta prueba.

## Criterio de publicación

No crear una release ni anunciar una OTA 0.6.8 hasta que el firmware compile y las pruebas automatizadas pasen. Los resultados de hardware deben marcarse aparte como pendientes hasta que se prueben en el dispositivo.
