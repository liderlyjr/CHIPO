# Registro de errores y diagnóstico de CHIPO

Este documento mantiene la trazabilidad de los fallos encontrados en CHIPO OS, las evidencias disponibles, los cambios realizados y el resultado de cada verificación.

**Regla del registro:** un error solo se marca como corregido cuando existe una modificación identificable y una prueba que lo respalda. Que el firmware compile en GitHub Actions no demuestra por sí solo que el fallo esté resuelto en la placa física.

## Estado actual — 27 de noviembre de 2026

- **Versión en desarrollo:** CHIPO OS 0.7.0, rama `release/chipo-os-0.7.0`; la Release oficial sigue siendo 0.6.9.
- **Placa objetivo:** Waveshare ESP32-S3 Touch AMOLED 2.16 (480 × 480).
- **Incidencias abiertas:** bloqueos/reinicios intermitentes, desfase de orientación, validación física de NIVEL/Gravitysphere/HORIZONTE/DrawPanel y activación por voz «Chipo» (`cheepo`, restaurada en 0.7.1).
- **Estado:** hay correcciones de código y CI, pero la placa física todavía debe validar el comportamiento real.
- **Prueba física:** el usuario informó que CHIPO arranca y funciona, pero luego presenta fallos y se reinicia; falta un nuevo log serie para cerrar esa incidencia.

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


### CHIPO-TOOLS-LEVEL-001 — Calibración manual del nivel de burbuja

**Estado:** Implementado en rama de corrección; pendiente de compilación y prueba física.

**Cambio realizado**

- Al abrir NIVEL, la pantalla indica «Toca para calibrar - Mantén para salir».
- Un toque con CHIPO apoyado y estable captura la gravedad actual como cero de referencia de esa sesión.
- Si el dispositivo no está suficientemente plano, no acepta la calibración y muestra «Ponme plano para calibrar».
- Una pulsación larga sigue cerrando NIVEL.
- La calibración se conserva mientras NIVEL permanezca abierto; al volver a abrirlo se solicita calibrar de nuevo. Todavía no se guarda en memoria persistente.

**Limitación conocida**

Esta primera implementación permite compensar una mesa ligeramente desnivelada durante la sesión, pero no guarda el cero tras reiniciar. La calibración persistente queda pendiente de integrar con el almacenamiento de ajustes y probar en la placa.



### CHIPO-HW-I2C-001 — Timeout de lectura AXP2101 provoca aborto y reinicio

**Estado:** Parche de tolerancia añadido en rama; compilación y prueba física pendientes. Causa eléctrica/origen del timeout aún no confirmado.

**Evidencia física (26-11-2026)**

El log de la placa muestra, durante el estado `speaking`:

```text
W AudioService: Encode queue is full, dropping oldest frame
StateMachine: listening -> speaking
Application: << Mañana en Barcelona hará sol durante el día,
ESP_ERROR_CHECK failed: esp_err_t 0x107 (ESP_ERR_TIMEOUT)
file: "./main/boards/common/i2c_device.cc" line 43
func: uint8_t I2cDevice::ReadReg(uint8_t)
expression: i2c_master_transmit_receive(...)
abort() was called
```

**Causa inmediata confirmada**

La lectura de un registro I²C devuelve timeout (0x107). La implementación común de `I2cDevice::ReadReg()` convierte cualquier error de lectura en aborto mediante `ESP_ERROR_CHECK`, reiniciando el ESP32-S3. Por tanto, este reinicio concreto no es evidencia de falta de RAM ni de un fallo de LVGL/TTS.

**Dispositivo/tarea probable**

En la placa Waveshare 2.16, el AXP2101 (PMIC/medidor de batería) comparte el bus I²C con el controlador táctil y otros dispositivos. CHIPO consulta el estado de batería periódicamente desde el timer de gestión de energía; las lecturas `IsCharging()`, `IsDischarging()` y `GetBatteryLevel()` pasan por `Axp2101::ReadReg()`. La coincidencia temporal hace de esta ruta el primer sospechoso, pero el log no incluye la dirección ni el registro, por lo que todavía no demuestra qué dispositivo originó el timeout.

**Parche añadido en `scripts/integrate_chipo.py`**

- `I2cDevice::ReadReg()` deja de ejecutar `ESP_ERROR_CHECK` ante un fallo transitorio: registra dirección, registro y error, y devuelve el sentinel `0xFF`.
- `Axp2101` trata una lectura fallida del estado como desconocida y una lectura fallida del porcentaje como `-1`.
- La implementación de la placa Waveshare devuelve `false` si el porcentaje de batería no es válido, en vez de publicar un valor incorrecto.

El parche evita que un timeout de lectura de un registro convierta automáticamente un fallo recuperable en un reinicio. No repara por sí solo un bus bloqueado ni demuestra todavía por qué aparece el timeout. Las escrituras I²C y las lecturas de bloques mantienen su comportamiento anterior.

**Commit:** `525e66f34e05692df4bf3893b994e60bf5304165` en la rama `fix/manual-level-calibration`.

**Validación pendiente**

1. Compilar el overlay completo contra XiaoZhi/ESP-IDF.
2. Comprobar que el log identifica dirección y registro si vuelve a fallar.
3. Repetir varias respuestas largas y observar si CHIPO sigue hablando sin reiniciarse.
4. Comprobar lectura de batería y estado de carga con bus normal y con fallo simulado.
5. Solo cerrar la incidencia después de prueba física.

| 26-11-2026 | CHIPO-HW-I2C-001 | Backtrace/abort atribuido a `ESP_ERROR_CHECK` en `I2cDevice::ReadReg()`; parche de lectura tolerante añadido en rama, pendiente de build y prueba en hardware. |


---

# Bitácora técnica para asistentes de código (Claude, ChatGPT y colaboradores)

**Actualizada: 27-11-2026**  
**Rama de trabajo:** `release/chipo-os-0.7.0`  
**PR de trabajo:** [#32](https://github.com/liderlyjr/CHIPO/pull/32) (Draft)  
**Regla de publicación:** no fusionar a `main` ni crear/promover una Release sin autorización expresa del propietario. La autorización dada para publicar 0.7.0 cuando el Action esté verde no autoriza por sí sola a fusionar a `main`; confirmar ese paso por separado.

> **Instrucción para Claude y cualquier agente que continúe el trabajo:** antes de diagnosticar o modificar CHIPO, leer este documento completo, `docs/WAKE_WORD.md`, `docs/XIAOZHI_INTEGRATION.md`, `XIAOZHI_REF`, `VERSION` y el workflow `.github/workflows/build-chipo.yml`. Revisar el estado actual de la rama y los commits antes de actuar. No asumir que esta bitácora describe el HEAD actual si hay commits posteriores. No revertir una corrección sin reproducir el problema y explicar por qué la solución existente no sirve. Mantener esta bitácora actualizada en el mismo cambio de código.

## 1. Cómo trabajar sin romper la integración

- CHIPO apunta exclusivamente a la **Waveshare ESP32-S3 Touch AMOLED 2.16, 480 × 480**.
- CHIPO es una capa sobre XiaoZhi; la integración real se aplica mediante `scripts/integrate_chipo.py`. No editar una copia temporal de XiaoZhi esperando que el cambio llegue al firmware: los parches permanentes deben quedar en CHIPO, normalmente en ese script o en `components/chipo_*`.
- La versión exacta de XiaoZhi está fijada en `XIAOZHI_REF`. No actualizarla por intuición. Si se cambia, ejecutar la integración y comprobar todos los anclajes.
- `replace_once()` en `scripts/integrate_chipo.py` exige encontrar exactamente un anclaje. Si falla con `expected one anchor, found 0/2`, no eliminar la protección ni hacer un reemplazo amplio: inspeccionar la versión fijada de XiaoZhi, localizar el bloque correcto y actualizar el parche de forma explícita.
- El script debe poder ejecutarse dos veces: la segunda ejecución debe indicar `[already applied]` y no duplicar cambios. Si una nueva modificación rompe esta idempotencia, corregirla.
- Mantener pruebas host para la lógica que pueda probarse fuera de la placa. Ejecutar `tests/host/run.sh` (en CI se usa LVGL 9.5.0, AddressSanitizer y UBSan).
- Distinguir siempre: **prueba host**, **compilación ESP-IDF**, **arranque en placa** y **validación funcional física**. Son niveles diferentes.
- Cada incidencia debe indicar commit, archivos, evidencia, pruebas y limitaciones. No marcar como resuelta en hardware solo porque CI esté verde.

## 2. Cambios realizados para 0.7.0

Estos cambios están registrados en la rama `release/chipo-os-0.7.0`. La lista se basa en los commits y el trabajo documentado; verificar el HEAD antes de continuar.

### CHIPO-UI-001 — Tamaño de la cara

- **Archivo:** `components/chipo_face/chipo_face.cpp`.
- **Cambio:** se aumentó el tamaño de la cara del 58 % al 64 %, conservando el límite de altura máxima del 85 %.
- **Motivo:** la cara se veía demasiado pequeña en la pantalla.
- **Validación:** cambio aplicado en la rama. La apariencia final y el encuadre deben confirmarse en la AMOLED física.

### CHIPO-TOOLS-DRAWPANEL-001 — Gestos/scroll interferían con la pizarra

- **Archivo:** `components/chipo_tools/chipo_tools.cpp`.
- **Cambio:** se desactivaron los flags de scroll, gesture bubble y scroll chain que interferían con la interacción de DrawPanel.
- **Motivo:** evitar que el gesto de dibujar se convierta en desplazamiento o se propague a contenedores superiores.
- **Validación:** cambio aplicado; falta comprobar dibujo continuo, bordes, limpieza y respuesta táctil en la placa.
- **Importante:** no afirmar que se ha integrado la implementación original completa de DrawPanel de Waveshare. Este cambio es una corrección de flags de interacción, no una sustitución íntegra del módulo por el código upstream. Si se pide equivalencia exacta con Waveshare, comparar el código fuente original, versión y dependencias antes de afirmar que es idéntico.

### CHIPO-SETTINGS-BATTERY-001 — Indicador de batería

- **Archivo:** `components/chipo_settings/chipo_settings.cpp`.
- **Cambio:** se movió el indicador de batería a la esquina superior derecha.
- **Validación:** aplicado; comprobar visualmente que no se solape con otros indicadores en la placa.

### CHIPO-SENSOR-GRAVITY-001 — Mapeo de ejes de Gravitysphere

- **Cambio:** `GravityBallFromAccel()` usa el mapeo `x = -accel_y * 8` y `y = accel_x * 8`, siguiendo la orientación usada por Waveshare 2.06.
- **Archivos relacionados:** `components/chipo_tools/chipo_tools_logic.cpp` y sus pruebas host.
- **Motivo:** alinear el sentido del movimiento de la bola con la orientación física de la placa.
- **Pruebas:** se actualizaron las expectativas de los ejes en `tests/host/test_tools_logic.cpp`.
- **Pendiente:** comprobar el sentido real al inclinar la Waveshare 2.16; la orientación de montaje puede requerir verificación física. No invertir ejes a ciegas.

### CHIPO-TOOLS-LEVEL-002 — Escala de burbuja y ángulo del nivel

- **Archivos:** `components/chipo_tools/include/chipo_tools_logic.h`, `components/chipo_tools/chipo_tools_logic.cpp` y `tests/host/test_tools_logic.cpp`.
- **Cambio:** la burbuja recorre la escala completa a 30° (antes 10°); el ángulo y la burbuja siguen visibles hasta 35° y se ocultan por encima de ese límite.
- **Se conserva:** `kLevelOkDeg = 0.6`.
- **Pruebas añadidas/actualizadas:** casos de 10°, 30°, 40° y comprobaciones de ejes/mirror.
- **Fallo de test encontrado:** una expectativa de `mirror y` seguía calculada con la escala antigua de 10°. El código nuevo estaba usando 30°, por lo que la aserción esperaba un desplazamiento equivocado.
- **Solución:** actualizar la expectativa del test host para la nueva escala, no revertir el comportamiento de 30°.
- **Commit de corrección de expectativa:** `0cc2fa603c58dc9e5d23138a8577ba86d04d7042`.
- **Pendiente:** validar en placa que el movimiento visual corresponde al ángulo físico y que la burbuja queda dentro de los límites de la pantalla.

### CHIPO-XIAOZHI-UI-001 — XiaoZhi volvía a mostrar emoji/logo sobre la cara CHIPO

- **Archivo principal:** `scripts/integrate_chipo.py`.
- **Cambio:** el parche de integración hace que `ShowEmotion()` oculte explícitamente `emoji_label_` y `emoji_image_` y pause el controlador GIF, porque CHIPO es propietario de la cara/renderizado.
- **Motivo:** evitar que el cleanup de mensajes de XiaoZhi restaure su logo o emoji centrado por encima de la interfaz CHIPO.
- **Validación:** el parche forma parte del script de integración; comprobar en compilación y en placa los cambios de estado listening/speaking/idle.

### CHIPO-BUDDY-CLOCK-001 — Reloj superior y modo reloj de mesita

- **Integración:** el hook del reloj superior debe tener en cuenta `chipo_buddy_->desk_clock()`.
- **Motivo:** el modo de reloj de mesita de BUDDY también debe mantener el tratamiento de la zona superior.
- **Pendiente:** confirmar visualmente la transición a modo mesita y salida de ese modo.

## 3. Activación por voz: diagnóstico y experimento Jarvis

### Lo que aprendimos del diagnóstico

Hay dos fallos diferentes que no deben mezclarse:

1. **No se activa al pronunciar la palabra de wake word:** investigar detector de activación, modelo, fonética, sensibilidad y ruta de audio en reposo.
2. **Sí se activa, pero no entiende la frase posterior:** investigar ASR, idioma, conexión/servicio de XiaoZhi, calidad de audio y transcripción. Cambiar la wake word no corrige el ASR.

En la prueba física anterior, pulsar BOOT iniciaba la escucha y CHIPO reconocía voz, pero decir la palabra de activación sin pulsar BOOT no despertaba el dispositivo. Eso apuntó al detector de wake word, sin demostrar que el micrófono estuviera averiado.

El log de arranque llegó a mostrar MultiNet inglés `mn7_en`, palabra configurada `chee po`, umbral `0.20` y una salida de comando que parecía `pm Pb`. Esos datos demostraban inicialización de la ruta MultiNet, pero no que la palabra fuese reconocida correctamente.

### Experimento actual: Jarvis

- **Archivo:** `scripts/integrate_chipo.py`.
- **Configuración:** `CONFIG_USE_CUSTOM_WAKE_WORD=y`, `CONFIG_SR_MN_CN_NONE=y`, `CONFIG_SR_MN_EN_MULTINET7_QUANT=y`, `CONFIG_CUSTOM_WAKE_WORD="jarvis"`, `CONFIG_CUSTOM_WAKE_WORD_DISPLAY="Chipo"`, `CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=20`.
- **Documentación:** `docs/WAKE_WORD.md`.
- **Commits:** `14cc7b34cd0a88d251f23f57442d878833792cfb` (configuración) y `53b18ea40f164f6257328d85c9d500943d1624c5` (documentación).
- **Qué NO se ha hecho:** no se ha cambiado a WakeNet, no se ha integrado un modelo español, no se ha ajustado la ganancia del micrófono ni el threshold.
- **Estado:** experimento configurado, no validado físicamente. No decir que Jarvis ya funciona.

### Alternativa española investigada

La documentación/catálogo oficial de Espressif consultado identifica el modelo WakeNet español `wn10_es_holaesp` («Hola ESP»). Esto es un modelo de WakeNet, no un alias intercambiable con `CONFIG_CUSTOM_WAKE_WORD` de MultiNet. Para usarlo hay que verificar que la versión de ESP-SR fijada por el firmware contiene ese modelo y cambiar/integrar correctamente la ruta WakeNet/AFE y sus recursos. Cambiar solo el texto de la palabra personalizada no basta.

## 4. GitHub Actions y fallos de CI conocidos

- El workflow de compilación es `.github/workflows/build-chipo.yml`.
- El job `host-tests` debe pasar antes del job `build`.
- El firmware se compila en el contenedor `espressif/idf:v6.1`, para ESP32-S3.
- El artefacto se llama `CHIPO-OS-Waveshare-ESP32-S3-AMOLED-2.16` e incluye `BUILD_INFO.txt` con los commits usados.
- En PR, el workflow compila y sube artefactos; el job `release` solo se ejecuta en un push a `main`. Por eso un Action verde de PR **no publica por sí solo** una Release.
- El workflow puede cancelar ejecuciones anteriores al entrar un commit nuevo (`concurrency.cancel-in-progress: true`). Una ejecución cancelada no es una compilación fallida; comprobar siempre SHA, rama, evento y conclusión.
- Si un test falla tras cambiar una escala o fórmula, revisar si la expectativa del test sigue usando el comportamiento anterior. No cambiar el código de producción para satisfacer una aserción obsoleta sin analizar primero la matemática.

### Historial reciente

- Run **#268**, ID `46003344541`, rama `release/chipo-os-0.7.0`, SHA `53b18ea40f164f6257328d85c9d500943d1624c5`, evento `pull_request`. En la última consulta del 27-11-2026 estaba `in_progress`. Consultar el estado actual antes de sacar conclusiones: [Action #268](https://github.com/liderlyjr/CHIPO/actions/runs/46003344541).
- Varias ejecuciones posteriores a la corrección del test fueron canceladas por nuevos pushes. No atribuirles un fallo de compilación.
- El run #268 corresponde al commit de documentación Jarvis, cuyo SHA contiene también la configuración Jarvis en el commit anterior de la misma rama. Verificar siempre el SHA real de cada run.

## 5. Publicación de 0.7.0: procedimiento y límites

El propietario autorizó el 27-11-2026: **cuando el Action correspondiente esté verde, publicar CHIPO OS 0.7.0**. Esta autorización no elimina las siguientes comprobaciones:

1. Consultar el run más reciente de la rama y comprobar `conclusion=success`, no solo que un job aislado esté verde.
2. Confirmar que el SHA compilado es el HEAD que se pretende publicar y que incluye los cambios esperados.
3. Descargar/revisar el artefacto y su `BUILD_INFO.txt`; comprobar que existe el binario correcto.
4. Revisar `VERSION` y el tag destino. No sobrescribir una Release/tag existente.
5. Antes de publicar, comprobar el mecanismo de publicación configurado en el workflow. El job automático de Release solo corre en push a `main`; un PR verde no dispara ese job.
6. **No hacer merge a `main` automáticamente:** el usuario autorizó la publicación, pero el merge debe confirmarse aparte salvo que el usuario lo autorice expresamente.
7. Si la publicación requiere merge, tag o creación manual, explicar qué acción concreta falta y solicitar autorización para ese paso.
8. Tras publicar, verificar la Release y los assets desde GitHub; solo entonces anunciar que está publicada.

No publicar si CI falla, si el artefacto falta, si el SHA no coincide o si no se puede verificar el contenido. No declarar que el firmware ha sido probado en la placa si solo se compiló en CI.

## 6. Pendientes de validación física de esta tanda

- Cara al 64 %: encuadre, recorte y fluidez.
- DrawPanel: trazo continuo, scroll no deseado, gestos y borrado.
- Gravitysphere: ejes y sentido al inclinar la placa.
- NIVEL: escala 30°, visibilidad hasta 35°, ocultación por encima y precisión del cero.
- Indicador de batería: posición y legibilidad.
- XiaoZhi: que no reaparezcan emoji/logo durante transiciones de estado.
- BUDDY: reloj de mesita y zona superior.
- Jarvis: activación real, falsas activaciones en silencio y escucha posterior.
- Reinicios/errores I²C y reproducción de voz larga: obtener logs serie nuevos y no asumir que las correcciones anteriores resolvieron todos los síntomas.

## 7. Plantilla obligatoria para nuevas incidencias

Añadir cada caso con este formato:

```markdown
### CHIPO-AREA-NNN — Título breve

**Estado:** Abierto / Parche aplicado / CI correcto / Validado en placa / Cerrado.

**Síntoma y fecha:**
Descripción literal del comportamiento observado.

**Evidencia:**
Logs, backtrace, pasos para reproducir, run de CI y SHA.

**Diagnóstico:**
Separar causa confirmada de hipótesis.

**Solución aplicada:**
Archivos, funciones, motivo técnico y commit.

**Pruebas:**
Pruebas host, compilación y resultado de cada una.

**Validación física:**
Placa, versión instalada, pasos y resultado. Si no se hizo, escribir «Pendiente».

**Pendientes / riesgos:**
Qué falta comprobar y qué no debe darse por resuelto.
```

**Regla final para agentes:** si no puedes demostrar que una solución está en el código actual, no afirmes que existe. Si no puedes demostrar que una prueba se ejecutó, no afirmes que pasó. Si no tienes el log necesario, pide el log antes de inventar una causa. Documenta también los intentos fallidos y por qué se descartaron.


---

## 27-11-2026 — Correcciones adicionales de CHIPO OS 0.7.0

**Rama:** `release/chipo-os-0.7.0`  
**Estado:** código actualizado; CI de esta revisión pendiente; validación física pendiente.

### Horizonte artificial — salto angular
- Se extrajo el cálculo a `HorizonFromAccel()` dentro de `chipo_tools_logic.cpp`, sin dependencias ESP-IDF/LVGL.
- Roll ahora usa diferencia circular con `std::remainder(..., 360)`, evitando saltos de casi 360° al cruzar el límite +180°/-180°.
- Se rechazan muestras cuya magnitud no representa gravedad estática dentro del rango configurado.
- Tests host añadidos: posición neutra, cruce ±180°, muestra de caída libre y pitch de 12°.
- Esto corrige un fallo matemático reproducible; no demuestra que los signos físicos coincidan con la orientación de la placa hasta probarlo en hardware.

### Gravitysphere — calibración
- Antes, tras cinco intentos de calibración inestable, el código aceptaba igualmente la media de las muestras como bias. Eso podía fijar una referencia incorrecta si se movía la placa.
- Ahora solo se marca calibrada cuando el rango de las 200 muestras cumple el umbral de estabilidad. Si agota cinco intentos, informa del fallo y espera un toque para reintentar.
- El mapeo de aceleración conserva el algoritmo Waveshare: `x -= accelY * 8`, `y += accelX * 8`.

### DrawPanel — superficie táctil fija
- Se aplica tanto al root del overlay como al canvas la política de superficie fija de Waveshare: desactivar scroll, elasticidad, momentum, scroll-chain, gesto/event bubbling y barras de desplazamiento.
- Se mantiene `CLICKABLE | PRESS_LOCK` para que un trazo no se pierda al arrastrar el dedo.
- El buffer RGB565 y el almacenamiento de trazos siguen siendo la implementación CHIPO; no se afirma que toda la aplicación sea un port literal de la app DrawPanel de Waveshare.
- Pendiente validar trazos largos, esquinas y salida en la pantalla real.

### QMI8658 — estado de configuración
- Se documentó la configuración efectiva del lector CHIPO: acelerómetro ±4 g/250 Hz, giroscopio opcional ±512 °/s, auto-incremento y lectura little-endian; el consumidor procesa a 25 Hz y a 50 Hz en DRIVE.
- No se alteró la configuración compartida del sensor en esta revisión: cambiar escalas/ODR sin medición física podría afectar cara, herramientas y DRIVE a la vez.
- Referencia oficial 2.16: ejemplo [QMI8658 de Waveshare](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16/tree/main/examples/arduino/04_LVGL_QMI8658_ui) y driver [SensorQMI8658.hpp](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16/blob/main/examples/arduino/libraries/SensorLib/src/SensorQMI8658.hpp).

### Versionado y documentación
- `VERSION` se actualizó a `CHIPO_OS_VERSION=0.7.0`, con estado `development-not-ready-for-release`. Esto identifica el firmware de desarrollo; no publica una Release.
- README, CHIPO_TOOLS, WAKE_WORD y el documento `CHIPO_0.7.0_STATUS.md` se sincronizaron con la configuración experimental Jarvis y las correcciones anteriores.
- No se ha fusionado a `main`, no se ha creado tag ni Release 0.7.0. La 0.6.9 permanece intacta.

### Validaciones pendientes
1. Ejecutar CI completo para el nuevo HEAD y comprobar que el script de integración y el build ESP-IDF siguen pasando.
2. En la placa: verificar signos de los ejes, horizonte, nivel y Gravitysphere.
3. Probar DrawPanel en toda la pantalla y que no se desplace.
4. Revisar hora superior/logo/emociones y modo reloj BUDDY.
5. Probar audio, wake word Jarvis y ASR por separado.
6. Ejecutar prueba prolongada y recopilar log serie si vuelve a reiniciarse.

## 2026-11-27 — Corrección de compilación LVGL 9.5 (CI)

- **Fallo observado:** el job `Face and tilt tests (on a PC)` del run [46006214443](https://github.com/liderlyjr/CHIPO/actions/runs/46006214443) falló al compilar `components/chipo_tools/chipo_tools.cpp`.
- **Causa:** `lv_obj_clear_flag()`, `lv_obj_add_flag()` y `lv_obj_remove_flag()` reciben un único `lv_obj_flag_t` en LVGL 9.5; combinar varias banderas con OR las promociona a `int` y provoca error de conversión.
- **Corrección aplicada:** `ConfigureFixedTouchObject()` ahora recorre una lista de banderas y hace una llamada por bandera. CLICKABLE y PRESS_LOCK también se configuran en llamadas separadas.
- **Commit:** `ec117e05173175cdeff508eebe8b3586c3abdc61`.
- **Estado:** corrección subida a `release/chipo-os-0.7.0`; pendiente comprobar el nuevo CI, la compilación ESP32-S3 y las pruebas físicas. No publicar hasta completar las comprobaciones.
