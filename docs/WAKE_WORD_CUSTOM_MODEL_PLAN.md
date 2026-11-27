# Plan técnico — palabra de activación «Hola Chipo»

**Proyecto:** CHIPO OS 0.7.0  
**Placa objetivo:** Waveshare ESP32-S3-Touch-AMOLED-2.16  
**Estado:** plan aprobado por el usuario; modelo aún no solicitado ni integrado.  
**Alcance:** sustituir únicamente la palabra de activación. No añadir comandos MultiNet nuevos.

## 1. Objetivo funcional

- Frase exacta: **«Hola Chipo»**.
- Idioma: español.
- Perfil de voces: múltiples hablantes, no ligado a la voz del propietario.
- Detección local en la placa, sin requerir conexión a Internet para despertar.
- Mantener intactos el reconocimiento de órdenes de XiaoZhi, el servidor, el flujo de audio y los componentes de CHIPO DRIVE.
- No modificar la sensibilidad del micrófono ni el umbral actual de MultiNet como sustituto del entrenamiento.

## 2. Arquitectura elegida

### Motor objetivo: Espressif WakeNet 10

Espressif publica un modelo español WakeNet 10 para «Hola ESP» (wn10_es_holaesp). Sirve como referencia de idioma, familia y compatibilidad, pero **no detecta «Hola Chipo»**.

Para la frase propia hay que solicitar a Espressif un modelo WakeNet personalizado. La opción recomendada para el requisito de varias voces es:

**Grabaciones crowdsourced de Espressif + muestras TTS.**

La alternativa TTS-only evita grabaciones humanas, pero no es la opción preferida cuando el objetivo prioritario es generalizar entre hablantes reales. El corpus aportado por el cliente exige al menos 20.000 muestras cualificadas y un proceso de recogida mucho mayor; no se recomienda como primera vía.

Espressif indica una tarifa única de personalización (precio a solicitar) y no una licencia recurrente por dispositivo/tiempo. En la modalidad crowdsourced, la recogida de voces se acuerda con Espressif y, una vez listo el corpus, el entrenamiento/optimización suele tardar 2–3 semanas.

## 3. Solicitud a Espressif

Contactar con sales@espressif.com y solicitar:

- WakeNet 10 personalizado para la frase exacta «Hola Chipo».
- Español de España como pronunciación de referencia, admitiendo variaciones de acento del español.
- Entrenamiento multi-speaker con grabaciones crowdsourced + TTS.
- Recomendación de umbral/modo de detección para el ESP32-S3.
- Entrega del modelo para ESP-SR y su integración con ESP-IDF.
- Versión mínima/recomendada de ESP-SR y ESP-IDF, nombre exacto del modelo, formato de entrega, instrucciones de empaquetado y redistribución.
- Precio único, plazos, condiciones de uso y posibilidad de incluir el modelo en firmware distribuido de CHIPO OS.

No iniciar un entrenamiento con corpus propio ni pagar nada sin recibir y revisar presupuesto, requisitos y condiciones.

## 4. Requisitos del corpus si Espressif solicita grabaciones del cliente

La guía oficial de Espressif para corpus propio indica WAV mono, PCM firmado de 16 bits, 16 kHz; más de 500 hablantes, incluyendo hombres y mujeres de distintas edades y al menos 100 menores; ambiente silencioso por debajo de 40 dB y micrófono de alta fidelidad. Para cada hablante se solicitan 15 repeticiones a 1 m y otras 15 a 3 m (5 rápidas, 5 normales y 5 lentas).

Esta carga corresponde a la modalidad de corpus aportado por el cliente. No asumir que hay que recopilarla si se contrata la modalidad crowdsourced de Espressif: confirmar el flujo exacto con su equipo.

## 5. Integración en CHIPO OS

La revisión del XiaoZhi fijado por XIAOZHI_REF confirma que hay dos rutas distintas:

- USE_CUSTOM_WAKE_WORD crea CustomWakeWord, que usa **MultiNet** para una frase fonética añadida como comando.
- USE_ESP_WAKE_WORD crea EspWakeWord, que usa **WakeNet** (ESP_WN_PREFIX) y el modelo cargado en los assets/model partition.

Por tanto, el modelo personalizado no se integra cambiando CONFIG_CUSTOM_WAKE_WORD ni escribiendo «Hola Chipo» en sdkconfig.defaults. Eso seguiría usando MultiNet y no cargaría el modelo WakeNet entrenado.

Trabajo de integración, después de recibir el modelo:

1. Verificar el paquete entregado por Espressif y su compatibilidad con las versiones ESP-SR/ESP-IDF que fija XiaoZhi.
2. Añadir el modelo al mecanismo de assets/model partition que usa el firmware; no guardar el WAV/corpus dentro del firmware.
3. Activar la ruta USE_ESP_WAKE_WORD en la configuración de CHIPO y dejar de usar la ruta MultiNet como wake word. No eliminar ni alterar el reconocimiento normal de órdenes.
4. Ajustar la selección de modelo para elegir explícitamente el modelo «Hola Chipo» si el paquete incluye más de un WakeNet; no depender de «primer modelo encontrado».
5. Mantener el AFE y la captura de audio de la placa. La Waveshare 2.16 usa ESP32-S3R8, 8 MB PSRAM, 16 MB flash y ADC de micrófono ES7210 con dos micrófonos; verificar el formato/canales reales que entrega el codec antes de alimentar WakeNet.
6. Revisar la tabla de particiones y el tamaño real del paquete. No aumentar ni mover particiones hasta conocer el tamaño del modelo y comprobar que OTA y almacenamiento de assets siguen cabiendo.
7. Añadir logs de arranque: motor, nombre exacto del modelo, frecuencia, tamaño de bloque y resultado de inicialización. Si el modelo no carga, informar error y no simular que la palabra está activa.

## 6. Plan de validación

### Banco de pruebas

- Varios hablantes (voces graves/agudas, edades y acentos distintos).
- Distancias: cerca, 1 m y 3 m.
- Entornos: habitación silenciosa y ruido doméstico/TV moderado.
- Frase objetivo repetida con velocidad normal y variada.
- Frases negativas parecidas («Hola, Chipo» incompleto, «Hola chico», conversación casual) y periodos de silencio.

### Métricas que registrar

- Tasa de detección correcta por hablante/distancia.
- Falsos despertares por hora en silencio y ruido.
- Tiempo desde el final de «Hola Chipo» hasta la activación.
- Consumo de RAM/PSRAM, CPU y estabilidad térmica.
- Efecto sobre latencia de audio, reproducción TTS y conversación.

### Criterio de aceptación

No declarar la función lista solo porque compile o porque se active con una voz. Debe superar pruebas con varias personas, sin falsos despertares excesivos y sin romper el inicio/fin de escucha de XiaoZhi. Guardar logs y resultados en la documentación de la versión.

## 7. Estado actual y salvaguardas

- La configuración actual de CHIPO usa MultiNet 7 inglés con CONFIG_CUSTOM_WAKE_WORD="chee po" y umbral 20.
- La restauración de chee po se conserva como fallback hasta que el modelo WakeNet «Hola Chipo» esté integrado y validado.
- No se cambia todavía scripts/integrate_chipo.py, el modelo de audio, las ganancias, el umbral ni la tabla de particiones.
- No se ha recibido un modelo WakeNet «Hola Chipo», no se ha compilado esta integración y no se ha probado en hardware.
- La rama de trabajo sigue siendo release/chipo-os-0.7.0; no tocar stable 0.6.9 ni publicar release/merge a main.

## 8. Referencias oficiales

- Proceso de personalización WakeNet: https://docs.espressif.com/projects/esp-sr/en/latest/esp32/wake_word_engine/ESP_Wake_Words_Customization.html
- Modelos WakeNet y modelo español «Hola ESP»: https://github.com/espressif/esp-sr
- Implementación Waveshare 2.16 / especificación de placa: https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.16
- XiaoZhi fijado por CHIPO: XIAOZHI_REF; rutas main/audio/wake_words/custom_wake_word.cc y main/audio/wake_words/esp_wake_word.cc.
