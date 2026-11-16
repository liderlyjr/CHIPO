# CHIPO OS V0.2 — módulo visual

## Incluido
- Componente ESP-IDF `chipo_face`.
- Cara LVGL con ojos, boca y parpadeo periódico.
- Estados: Idle, Listening, Thinking, Speaking, Connecting y Error.
- Código separado de los controladores de pantalla/audio de XiaoZhi.

## Integración prevista
1. Copiar `components/chipo_face` a la raíz del repositorio XiaoZhi.
2. Añadir la dependencia LVGL al componente de interfaz que lo utilice.
3. Crear una instancia persistente desde la tarea UI después de inicializar LVGL:
   ```cpp
   static ChipoFace chipo_face;
   chipo_face.Start(parent_lvgl_object);
   ```
4. Traducir los cambios de `Application::GetInstance().GetDeviceState()` a `ChipoFace::State`.

Correspondencia inicial:
- `kDeviceStateIdle` → Idle
- `kDeviceStateListening` → Listening
- `kDeviceStateSpeaking` y `kDeviceStateNotifying` → Speaking
- `kDeviceStateConnecting`, `kDeviceStateStarting`, `kDeviceStateActivating` → Connecting
- `kDeviceStateFatalError` → Error
- Thinking debe activarse cuando el protocolo confirme que el modelo está procesando, no por una suposición temporal.

## Límites
Esta entrega no es todavía firmware completo ni binario flasheable. No se ha compilado en el equipo del usuario ni probado en la placa. Debe verificarse la versión LVGL del checkout y adaptar las llamadas de API si el componente usa una versión distinta. Todas las llamadas LVGL deben ejecutarse en la tarea UI.

No se inicializa el audio ES8311, el micrófono, el táctil, el CO5300 ni el AXP2101: esos controladores pertenecen al perfil oficial de placa de XiaoZhi.
