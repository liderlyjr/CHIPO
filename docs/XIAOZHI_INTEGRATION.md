# Integración CHIPO + XiaoZhi

La integración está automatizada por `scripts/integrate_chipo.py`. No hace falta editar a mano el repositorio original.

## Qué modifica el script

1. Copia `components/chipo_face` al directorio `components` de XiaoZhi.
2. Copia el adaptador `chipo_state_map.h` a `main`.
3. Añade la instancia persistente `ChipoFace` a `Application`.
4. Inicializa la cara después de `display->SetupUI()`, bajo `DisplayLockGuard`.
5. Actualiza la expresión desde `Application::HandleStateChangedEvent()`, también bajo el bloqueo de pantalla.
6. Enlaza el componente LVGL desde `main/CMakeLists.txt`.
7. Activa el perfil `CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_1_8_v2` y el estilo de mensajes esperado por el perfil de la placa.

## Estados

- Idle / Unknown / Upgrading → Idle
- Starting / Connecting / Activating / Wi-Fi configuring → Connecting
- Listening / Audio testing → Listening
- Speaking / Notifying → Speaking
- Fatal error → Error

XiaoZhi no expone un estado público inequívoco de procesamiento del modelo en el enum actual. Por eso Thinking no se simula con un temporizador: se conectará cuando exista un evento fiable.

## Compilación

Sigue [BUILD_WINDOWS.md](BUILD_WINDOWS.md). El script comprueba que los archivos y anclas de integración existan y se detiene si la estructura upstream ha cambiado. La compilación se realiza con el entorno ESP-IDF y los componentes oficiales de XiaoZhi.

## Alcance de esta versión

CHIPO Face es una capa visual inicial superpuesta a la pantalla de XiaoZhi. No reemplaza todavía el diseño completo de la interfaz, los diálogos, el menú de configuración ni el sistema de conversación. Los controladores CO5300, táctil, ES8311 y AXP2101 siguen siendo responsabilidad del perfil oficial de Waveshare en XiaoZhi.

La compilación automatizada debe terminar correctamente y después debe validarse el arranque en la placa física antes de considerar el firmware listo para uso diario.
