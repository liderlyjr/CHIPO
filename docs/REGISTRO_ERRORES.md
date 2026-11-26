# Registro de errores y diagnóstico de CHIPO

Este documento mantiene la trazabilidad de los fallos encontrados en CHIPO OS, las evidencias disponibles, los cambios realizados y el resultado de cada verificación.

**Regla del registro:** un error solo se marca como corregido cuando existe una modificación identificable y una prueba que lo respalda. Que el firmware compile en GitHub Actions no demuestra por sí solo que el fallo esté resuelto en la placa física.

## Estado actual — 26 de noviembre de 2026

- **Versión del repositorio:** CHIPO OS 0.6.0.
- **Placa objetivo:** Waveshare ESP32-S3 Touch AMOLED 2.16 (480 × 480).
- **Incidencias abiertas:** bloqueos/reinicios intermitentes, desfase de orientación, calibración manual del modo NIVEL y activación por voz «CHIPO».
- **Estado:** ambos problemas siguen en diagnóstico; no hay todavía un parche correctivo confirmado.
- **Prueba física:** el usuario confirma que CHIPO ya arranca y llega a funcionar, pero después presenta fallos y se reinicia.

## Incidencias

### CHIPO-IMU-001 — Orientación neutra desplazada un paso a la izquierda

**Estado:** Abierto — pendiente de precisar y corregir la calibración/orientación.

**Síntoma comunicado en la prueba física (26-11-2026)**

El usuario indica que, cuando CHIPO debería estar derecho/en posición neutra, la orientación de la cara aparece desplazada un paso hacia la izquierda.

**Evidencia y código revisado**

- La placa usa el sensor QMI8658.
- `components/chipo_face/chipo_tilt.cpp` calibra automáticamente la posición de reposo tras 1.2 s de estabilidad, si la gravedad está suficientemente dentro del plano de pantalla.
- La calibración guarda el vector de reposo `u0_`; el eje horizontal se deriva del eje vertical detectado.
- `CHIPO_TILT_MIRROR_X` solo invierte el sentido izquierda/derecha. No corrige por sí mismo un pequeño desfase de posición neutra.
- El código también puede recalibrar la posición de reposo después de 10 s en una inclinación estable de entre 3° y 30°.

**Diagnóstico pendiente**

Aún hay que distinguir si el desplazamiento es:
1. una inclinación física constante de la cara en reposo;
2. un desfase de la animación/ojos respecto al centro;
3. un mapeo de ejes/signo de la IMU.

No cambiar `CHIPO_TILT_MIRROR_X` a ciegas: eso invertiría el sentido de las reacciones, no necesariamente centraría la orientación.

**Próximos pasos**

1. Confirmar si el desfase se ve en la cara incluso sin mover la placa o solo en la reacción al inclinarla.
2. Comprobar la posición de calibración al arrancar, manteniendo la placa quieta y derecha durante al menos 2 s.
3. Revisar el mapeo de ejes y la neutralización con esa observación.
4. Aplicar el ajuste mínimo, añadir prueba automatizada y validar en la placa.

**Resolución:** pendiente; no se ha modificado todavía el mapeo de la IMU.

### CHIPO-BOOT-002 — Bloqueos y reinicios intermitentes después de arrancar

**Estado:** Abierto — diagnóstico en hardware.

**Síntoma comunicado en la prueba física (26-11-2026)**

El usuario informa que CHIPO ahora consigue arrancar y funcionar, pero después se buguea/se bloquea y se reinicia. Esto actualiza el síntoma inicial de bucle de reinicio inmediato: ya no falla necesariamente antes de entrar en funcionamiento.

**Evidencia disponible**

- Es un nuevo reporte de prueba física; todavía no se ha recibido el log serie del nuevo bloqueo.
- El panic anterior `InstrFetchProhibited` con `PC=0x00000000` y backtrace en la ruta de texto LVGL sigue siendo una pista histórica, pero no debe asumirse que explica todos los reinicios intermitentes.
- La causa raíz no está confirmada.

**Próximos pasos**

1. Obtener el log serie desde el arranque hasta el siguiente bloqueo/reinicio, incluyendo el nuevo backtrace y la razón de reset.
2. Comparar si el nuevo fallo conserva la ruta LVGL o si aparece una causa distinta.
3. Correlacionar el momento del fallo con voz, pantalla, táctil, IMU y módulos activos.
4. Documentar cada panic por separado y corregir solo con evidencia reproducible.

**Resolución:** pendiente. No hay todavía un arreglo validado en hardware.


### CHIPO-BOOT-001 — Reinicio durante el arranque por fallo en la ruta de texto de LVGL

**Estado:** Abierto — diagnóstico en curso.

**Síntoma observado**

La placa muestra CHIPO OS y después se reinicia, entrando en un ciclo de arranque. En los registros compartidos aparece un panic de ESP-IDF:

- `Guru Meditation Error: Core 1 panic'ed (InstrFetchProhibited)`
- `PC: 0x00000000`

El registro sitúa el fallo durante la actualización/refresco de elementos de pantalla, después de la inicialización de componentes de la placa y de la carga de recursos.

**Evidencia técnica disponible**

El análisis del backtrace/ELF apunta a la ruta de renderizado de texto de LVGL, con estas funciones implicadas:

- `lv_font_get_glyph_width`
- `lv_text_get_next_word`
- `lv_label_refr_text`
- `lv_display_refr_timer`

Esto es compatible con una referencia de fuente, callback o dato asociado al texto inválido o nulo. **No identifica todavía el objeto concreto ni demuestra que la fuente tipográfica sea la causa raíz.**

**Comprobaciones realizadas**

- La inicialización de la pantalla, el táctil, la IMU QMI8658 y los códecs de audio aparece en los registros antes del fallo.
- El análisis del backtrace sitúa el problema en el refresco/cálculo de texto de LVGL.
- Se está revisando la asignación y vida útil de las fuentes, las etiquetas y el cambio de tema mediante `SetTheme()`.
- No se ha confirmado todavía una etiqueta concreta con una fuente inválida.

**Compilación y pruebas relacionadas**

- GitHub Actions, ejecución [Build 145](https://github.com/liderlyjr/CHIPO/actions/runs/45885507339), commit `fde6b0b4e26eae49f7136035ec025bc41a5ea3bb`: finalizó con éxito el 26-11-2026.
- Los tres trabajos de esa ejecución terminaron correctamente: pruebas host de Face/inclinación, compilación para la Waveshare 2.16 y publicación del release.
- Se generó el artefacto `CHIPO-OS-Waveshare-ESP32-S3-AMOLED-2.16`.
- Estas pruebas verifican las tareas automatizadas y la generación del firmware; **no prueban que el reinicio esté solucionado en la placa física**.

**Próximos pasos**

1. Seguir las referencias de fuente y los callbacks de texto desde `SetTheme()` y la creación/actualización de etiquetas.
2. Identificar qué objeto o callback puede llegar nulo y en qué momento de su ciclo de vida.
3. Aplicar un cambio mínimo y documentar el commit exacto.
4. Ejecutar compilación y pruebas automatizadas.
5. Probar el firmware resultante en la placa y registrar el resultado del arranque.

**Resolución:** pendiente. No se ha aplicado todavía un parche para esta incidencia.

---

## Historial de cambios y verificaciones

| Fecha | Referencia | Acción / resultado |
|---|---|---|
| 26-11-2026 | Build 145 — [GitHub Actions](https://github.com/liderlyjr/CHIPO/actions/runs/45885507339) | Ejecución completada correctamente; pruebas host, compilación y publicación correctas. No equivale a validación física. |
| 26-11-2026 | CHIPO-BOOT-001 | Se documenta el panic `InstrFetchProhibited`, PC nulo y la ruta de texto LVGL señalada por el backtrace. Causa raíz pendiente. |
| 26-11-2026 | CHIPO-BOOT-002 | Nueva prueba física: CHIPO arranca y funciona, pero presenta bloqueos y reinicios intermitentes. Falta el log del nuevo fallo. |
| 26-11-2026 | CHIPO-IMU-001 | Nueva observación física: orientación neutra desplazada un paso a la izquierda. Se revisa la calibración QMI8658; ajuste pendiente. |
| 26-11-2026 | Build 148 — [GitHub Actions](https://github.com/liderlyjr/CHIPO/actions/runs/45889245590) | Compilación y workflow completados correctamente. El commit fue documental; no contiene una reparación de los dos problemas físicos. |

## Cómo mantener este registro

Para cada incidencia nueva, añadir:

1. Un identificador único (`CHIPO-...`), fecha y síntoma reproducible.
2. El fragmento de log relevante, sin sustituirlo por una interpretación.
3. La causa confirmada o, si aún no se conoce, las hipótesis claramente etiquetadas.
4. Los archivos y funciones modificados, junto con el enlace al commit.
5. Las pruebas ejecutadas y sus resultados.
6. La verificación en hardware, indicando placa, firmware y resultado.

No borrar los diagnósticos anteriores cuando se encuentre una solución: actualizar el estado a corregido y añadir la evidencia de la corrección para conservar el historial.


### CHIPO-AUDIO-001 — La palabra de activación «CHIPO» no inicia la escucha

**Estado:** Abierto — diagnóstico acotado al flujo de activación por voz.

**Nueva evidencia física (26-11-2026)**

El usuario confirma que, al pulsar el botón BOOT de la placa, CHIPO sí entra en escucha y reconoce la voz. Sin pulsar BOOT, no se activa al decir «CHIPO».

**Qué permite concluir esta prueba**

- El micrófono y la ruta de captura/reconocimiento de voz funcionan al menos en el modo de escucha iniciado manualmente.
- El fallo parece estar en la detección de la palabra de activación, su modelo/configuración o en la transición desde reposo al modo de escucha.
- Esto no demuestra todavía que todo el pipeline de audio esté libre de fallos ni identifica la causa exacta.

**Próximos pasos**

1. Seguir en el código la configuración del detector de wake word y el modelo realmente incluido en el firmware.
2. Comprobar la etiqueta/fonética configurada para «CHIPO», la sensibilidad, el idioma/modelo y la ruta de audio usada en reposo.
3. Verificar que el detector permanece activo después del arranque y que no se detiene al entrar en reposo.
4. Corregir con el cambio mínimo y validar en la placa: activación por «CHIPO» sin pulsar BOOT y escucha manual por BOOT como control.

**Resolución:** pendiente. No se ha cambiado todavía el modelo ni la configuración de activación.


| 26-11-2026 | CHIPO-AUDIO-001 | Prueba física: BOOT inicia la escucha y CHIPO reconoce la voz; la activación por la palabra «CHIPO» sin pulsar BOOT falla. Sospecha centrada en wake word; causa pendiente. |


### CHIPO-AUDIO-002 — Reinicio durante respuestas habladas largas

**Estado:** Abierto — causa pendiente de confirmar con log serie.

**Evidencia física (26-11-2026)**

El usuario informa que CHIPO se reinicia cuando habla durante mucho tiempo. Esto se suma al reporte previo de reinicios intermitentes durante el uso.

**Áreas a investigar**

- Memoria libre y mínima (heap interno y PSRAM) durante TTS largo.
- Pila de las tareas de audio, decodificación y reproducción; posibles stack overflows.
- Watchdog y bloqueos de tareas durante la salida de audio.
- Asignaciones acumuladas o fragmentación al recibir/reproducir bloques de voz.
- Ruta LVGL mientras la cara anima durante Speaking.

No se declara ninguna como causa confirmada. El fallo podría ser un panic, watchdog, brownout o reinicio solicitado; el motivo de reset y el backtrace son necesarios para distinguirlos.

**Prueba necesaria**

Capturar por puerto serie a 115200 desde antes de iniciar una respuesta larga hasta el reinicio. Guardar especialmente el motivo de reset, Guru Meditation, Backtrace, Stack canary, Task watchdog y mensajes de memoria. Repetir con una respuesta corta como control.

**OTA**

El repositorio ya incorpora `chipo_update` y un flujo de Releases de GitHub para la Waveshare 2.16. Las Releases se publican como pre-release y no se instalan automáticamente hasta promoverlas a latest. La descarga real, instalación y rollback siguen pendientes de prueba física. Cualquier parche correctivo se publicará primero como pre-release; no se promoverá ni se afirmará que está arreglado hasta validarlo en la placa.

| 26-11-2026 | CHIPO-AUDIO-002 | Usuario informa reinicio al hablar durante mucho tiempo; registrar motivo de reset/backtrace antes de atribuir causa. |
