# Registro de errores y diagnóstico de CHIPO

Este documento mantiene la trazabilidad de los fallos encontrados en CHIPO OS, las evidencias disponibles, los cambios realizados y el resultado de cada verificación.

**Regla del registro:** un error solo se marca como corregido cuando existe una modificación identificable y una prueba que lo respalda. Que el firmware compile en GitHub Actions no demuestra por sí solo que el fallo esté resuelto en la placa física.

## Estado actual — 26 de noviembre de 2026

- **Versión del repositorio:** CHIPO OS 0.6.0.
- **Placa objetivo:** Waveshare ESP32-S3 Touch AMOLED 2.16 (480 × 480).
- **Incidencia prioritaria:** reinicio durante el arranque, después de mostrar la pantalla de CHIPO OS.
- **Estado:** causa raíz todavía en investigación; no hay un parche correctivo confirmado para este reinicio.
- **Prueba física:** pendiente de repetir con un firmware corregido.

## Incidencias

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
- **Hallazgo nuevo (hipótesis, aún no causa raíz confirmada):** `scripts/integrate_chipo.py` conectaba CHIPO TOOLS a la fuente de texto del tema activo y el componente conservaba el puntero `lv_font_t*` para sus etiquetas. Si `SetTheme()` sustituye/libera ese recurso, las etiquetas podrían quedarse apuntando a una fuente inválida y fallar durante el refresco LVGL.
- Se prepara un cambio preventivo para que CHIPO TOOLS use su fuente Montserrat integrada, de vida estática, en vez de conservar un puntero prestado del tema. Al no tener la fuente personalizada, el componente ya pliega los acentos a ASCII.
- **Esto reduce un riesgo concreto, pero no demuestra todavía que sea la causa del panic ni que el reinicio esté corregido.**


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

## Cómo mantener este registro

Para cada incidencia nueva, añadir:

1. Un identificador único (`CHIPO-...`), fecha y síntoma reproducible.
2. El fragmento de log relevante, sin sustituirlo por una interpretación.
3. La causa confirmada o, si aún no se conoce, las hipótesis claramente etiquetadas.
4. Los archivos y funciones modificados, junto con el enlace al commit.
5. Las pruebas ejecutadas y sus resultados.
6. La verificación en hardware, indicando placa, firmware y resultado.

No borrar los diagnósticos anteriores cuando se encuentre una solución: actualizar el estado a corregido y añadir la evidencia de la corrección para conservar el historial.
