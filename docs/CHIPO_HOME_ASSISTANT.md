# CHIPO y Home Assistant

CHIPO puede controlar Home Assistant por voz con la **API REST documentada** de Home Assistant. Está apagado
mientras no haya una dirección y un token guardados en el dispositivo: sin ellos no hace nada y el asistente
contesta como siempre. Código en `components/chipo_ha`.

Estado: lógica y controlador probados en PC y compilados en GitHub Actions. **No probado contra un Home Assistant
real ni en la placa**, y **falta una forma cómoda de guardar la dirección y el token** (ver «Configuración»).

## Qué puede hacer

| Frase | Llamada |
| --- | --- |
| «activa la escena cine» | `POST /api/services/scene/turn_on` con `{"entity_id":"scene.cine"}` |
| «enciende la luz del salón» | `POST /api/services/light/turn_on` con `light.salon` |
| «apaga la luz de la cocina» | `POST /api/services/light/turn_off` con `light.cocina` |
| «apaga todas las luces» | `light/turn_off` con `entity_id: all` |
| «buenas noches» / «buenos días» (BUDDY) | ejecuta la escena de noche / de mañana que hayas configurado |

El nombre hablado se convierte en el identificador: minúsculas, sin acentos, espacios a `_` (`Salón de abajo` →
`light.salon_de_abajo`). **Tus entidades de Home Assistant tienen que llamarse así**, o renombrarlas allí.
Si la entidad no existe o Home Assistant no responde, CHIPO lo dice en pantalla («no responde…»).

La llamada de red se hace en una tarea aparte (nunca en la del interfaz) con un tiempo máximo de 6 s. Solo se envía
el token en la cabecera `Authorization: Bearer`, nunca se escribe en el registro ni en pantalla. Con `https://` se
valida el certificado con el paquete de certificados de ESP-IDF.

## Configuración

La dirección y el token se leen de la NVS del dispositivo, espacio de nombres `chipo_ha`, claves de texto:
`url` (por ejemplo `http://homeassistant.local:8123`), `token` (token de acceso de larga duración de tu perfil de
Home Assistant), `scene_night`, `scene_morning` (por ejemplo `scene.buenas_noches`; opcionales). **No hay
credenciales en el código ni en el repositorio.**

**Pendiente:** todavía no hay una pantalla ni un comando para escribir esos valores en la placa. No se compilan dentro
del firmware a propósito: las compilaciones de GitHub se publican como Releases y un token dentro sería público.
Opciones que quedan por decidir cuando llegue la placa: una página de configuración como la del Wi-Fi de XiaoZhi,
o una carga puntual por USB con las herramientas de ESP-IDF sin borrar los datos del Wi-Fi.

## Alternativa del lado del servidor (MCP)

XiaoZhi también permite conectar Home Assistant desde el servidor mediante herramientas MCP, sin guardar nada en
la placa. Es una vía independiente de esta; si tu servidor ya la ofrece, funciona sin tocar el firmware.

## Qué falta

- Leer estados (temperatura, sensores) y decirlos: pide analizar las respuestas JSON de `GET /api/states/<entidad>`.
- Guardar la configuración desde el propio CHIPO (arriba).
- Probarlo con un Home Assistant real.
