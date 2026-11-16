# Integración de CHIPO Face en XiaoZhi

Esta guía aplica al repositorio upstream `78/xiaozhi-esp32` y al perfil
`esp32-s3-touch-amoled-1.8-v2`.

## 1. Copiar el componente

Copia `components/chipo_face` dentro de la raíz del checkout XiaoZhi, conservando
su estructura. El componente necesita que el proyecto ya tenga LVGL inicializado.

Copia también `integration/xiaozhi/chipo_state_map.h` a una ruta accesible desde
el componente `main` de XiaoZhi.

## 2. Añadir el componente al build

En el `main/CMakeLists.txt` de XiaoZhi, añade `chipo_face` a la lista de
componentes requeridos del componente principal. Conserva todas las dependencias
que ya existan. La lista suele declararse mediante `idf_component_register`;
no reemplaces el contenido original.

## 3. Añadir la instancia a Application

En `main/application.h`, añade una inclusión de `chipo_face.h` y un miembro
privado persistente:

```cpp
std::unique_ptr<ChipoFace> chipo_face_;
```

La clase ya incluye <memory> en el archivo original de XiaoZhi.

## 4. Crear la cara después de SetupUI

En `Application::Initialize()`, inmediatamente después de:

```cpp
auto display = board.GetDisplay();
display->SetupUI();
```

añade:

```cpp
{
    DisplayLockGuard lock(display);
    if (lock) {
        chipo_face_ = std::make_unique<ChipoFace>();
        if (!chipo_face_->Start(lv_screen_active())) {
            chipo_face_.reset();
            ESP_LOGE(TAG, "Failed to start CHIPO Face");
        }
    }
}
```

Añade `#include "chipo_face.h"` a `application.h`. El archivo de aplicación
ya incluye `display.h`, que define `DisplayLockGuard`. El componente debe
crearse después de que el perfil de la placa haya inicializado la pantalla y
XiaoZhi haya creado su interfaz.

## 5. Actualizar la expresión en el evento de estado

En `main/application.cc`, incluye `chipo_state_map.h`. Dentro de
`Application::HandleStateChangedEvent()`, después de obtener el display:

```cpp
auto display = board.GetDisplay();

if (chipo_face_) {
    DisplayLockGuard lock(display);
    if (lock) {
        chipo_face_->SetState(ChipoStateFromXiaoZhi(new_state));
    }
}
```

No llames a LVGL desde callbacks de audio, Wi-Fi o temporizadores externos.
El manejador de estado se ejecuta en el bucle principal de la aplicación; el
bloqueo del display protege las operaciones LVGL.

## 6. Comportamiento visual y límites conocidos

La cara se crea como objeto superior centrado en la pantalla activa. En esta
primera integración queda por encima del contenido central de la interfaz
original; es una capa visual inicial, no una sustitución completa de todas las
pantallas de XiaoZhi. La barra superior y el área de mensajes pueden requerir
un diseño CHIPO dedicado en la siguiente iteración.

El estado Thinking no se asigna artificialmente: el enum público de estados
de XiaoZhi no distingue de forma fiable el procesamiento del modelo. Se añadirá
cuando exista un evento de protocolo inequívoco.

## 7. Compilación y validación

Selecciona el perfil oficial `esp32-s3-touch-amoled-1.8-v2` y usa la versión
de ESP-IDF exigida por el checkout concreto de XiaoZhi. Compila antes de
flashear. Este repositorio CHIPO no contiene todavía un binario y la integración
descrita aún debe compilarse y probarse en hardware real.
