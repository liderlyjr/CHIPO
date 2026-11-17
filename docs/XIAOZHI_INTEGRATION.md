# Integración CHIPO + XiaoZhi

La integración está automatizada por `scripts/integrate_chipo.py`. No hace falta editar a mano el repositorio original.

## Qué modifica el script

1. Copia `components/chipo_face`, `components/chipo_weather` y `components/chipo_update` al directorio `components` de XiaoZhi.
2. Copia el adaptador `chipo_state_map.h` a `main`.
3. Añade la instancia persistente `ChipoFace` a `Application`.
4. Inicializa la cara después de `display->SetupUI()`, bajo `DisplayLockGuard`.
5. Actualiza la expresión desde `Application::HandleStateChangedEvent()`, también bajo el bloqueo de pantalla.
6. Enlaza los componentes desde `main/CMakeLists.txt`.
7. Crea `ChipoWeather` después de la cara y le pasa cada frase reconocida por voz (`stt`) para abrir el clima; ver [CHIPO_WEATHER.md](CHIPO_WEATHER.md).
8. Activa las fuentes Montserrat 20 y 24 de LVGL (para la pantalla del clima).
9. Activa el perfil `CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_1_8_v2` y el estilo de mensajes esperado por el perfil de la placa.
10. Actualizador por GitHub Releases y bloqueo del OTA de XiaoZhi (`ota.cc`, `UpgradeFirmware`, `PROJECT_VER`); ver [CHIPO_UPDATE.md](CHIPO_UPDATE.md).

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
