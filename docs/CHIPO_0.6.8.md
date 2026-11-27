# CHIPO 0.6.8 — registro de integración de herramientas

**Rama de trabajo:** `feature/0.6.8-four-tools`  
**Base:** `main` (sin modificar)  
**Estado de esta rama:** implementación de pantallas en progreso; PR #28 abierto como borrador. La ejecución de CI #200 seguía ejecutando las pruebas host con AddressSanitizer/UBSan al último chequeo; la compilación ESP-IDF todavía no ha confirmado resultado. No es firmware publicable.

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

- Waveshare ESP32-S3 Touch AMOLED 1.75: componentes `Gravitysphere` y `draw/Drawpanel`, revisados como referencia. El repositorio declara Apache-2.0. Se adapta el comportamiento de DrawPanel (puntos rojos de 10 px, recorte circular y límite de 1.000 puntos) al buffer RGB565 negro de CHIPO; no se incorpora la app Brookesia ni sus dependencias.
- `emwstudio/esp32-attitude`: referencia localizada para el horizonte artificial y la fusión de sensores en la variante Waveshare AMOLED 2.16. Antes de reutilizar código literal hay que verificar su licencia; si no autoriza la reutilización, implementar la lógica de forma independiente.
- El firmware CHIPO ya contiene lectura del QMI8658 y una lógica básica de nivel. Debe aprovecharse esa ruta existente.

## Cambios realizados en esta rama

- Se creó la rama `feature/0.6.8-four-tools` desde `main`.
- En `components/chipo_tools/include/chipo_tools.h` y `chipo_tools_logic.h` se definieron las herramientas `Level`, `GravityBall`, `Horizon` y `Canvas`; se retiró `Ball`/bola mágica.
- En `components/chipo_tools/chipo_tools_logic.cpp` se eliminaron las respuestas de la bola mágica y se añadieron intenciones de voz para Gravity Ball y horizonte.
- En `components/chipo_tools/chipo_tools.cpp` se implementó el ciclo GPIO18: Ninguna → Nivel → Gravity Ball → Horizonte → DrawPanel → Ninguna.
- El nivel permite calibración manual con 200 muestras del QMI8658; comprueba la estabilidad X/Y con una ventana de 0,01 g (equivalente a los 0,1 m/s² del ejemplo Waveshare) y calcula el cero con la media de los tres ejes. Reutiliza el flujo IMU existente.
- DrawPanel se adaptó al estilo CHIPO: AMOLED negro, puntos rojos de 10 px, recorte circular y máximo de 1.000 puntos almacenados. No se añade la interfaz Brookesia de Waveshare.
- Gravity Ball y horizonte artificial tienen una primera implementación LVGL basada en el acelerómetro ya disponible; todavía requieren validación de movimiento, orientación y rendimiento en el dispositivo.
- Se añadieron pruebas host para la calibración, el límite de puntos y el ciclo GPIO18. Se abrió el PR #28 en borrador para validación.

**Importante:** el código está guardado en la rama y el PR, pero aún no se ha confirmado una ejecución correcta de las pruebas ni una compilación ESP-IDF. No hay release ni OTA.

## Pendiente antes de considerar 0.6.8 lista

- Ejecutar las pruebas host y corregir fallos.
- Compilar firmware ESP-IDF para la placa CHIPO y corregir errores de integración.
- Validar en hardware la calibración del nivel, la respuesta de Gravity Ball, el roll/pitch del horizonte, el tacto y el límite de puntos de DrawPanel.
- Ajustar el render del horizonte para no saturar CPU/PSRAM y comprobar la liberación de buffers y objetos al cambiar de herramienta.
- Verificar que no queda ninguna ruta, evento, estado, texto o prueba de la bola mágica.
- Revisar memoria PSRAM, liberación de buffers, temporizadores LVGL y limpieza de pantallas al cambiar de herramienta.
- Actualizar pruebas host y ejecutar CI/compilación ESP-IDF.
- Verificar la OTA y el comportamiento físico en la placa. Una compilación correcta no sustituye esta prueba.

## Criterio de publicación

No crear una release ni anunciar una OTA 0.6.8 hasta que el firmware compile y las pruebas automatizadas pasen. Los resultados de hardware deben marcarse aparte como pendientes hasta que se prueben en el dispositivo.

## Cambios anteriores del proyecto que esta rama debe conservar

Según el historial de trabajo de CHIPO, estos cambios son parte de la línea base previa y no deben perderse al integrar 0.6.8:

- **0.6.6 — interfaz CHIPO:** fondo AMOLED negro y visualización de los ojos/boca de CHIPO sin la interfaz ni los textos de XiaoZhi.
- **PR #26 — temporizador:** mantener visible la cuenta atrás durante su ejecución y regresar después a la pantalla adecuada, en vez de volver prematuramente a la cara.
- **PR #27 — palabra de activación:** ajuste de la configuración de la palabra personalizada a `cheepo`, conservando MultiNet7 y el umbral indicado en ese cambio.

Estos puntos se registran como requisitos de conservación de la base existente. No se presentan como nuevas modificaciones realizadas por esta rama ni como pruebas físicas confirmadas durante esta integración.
