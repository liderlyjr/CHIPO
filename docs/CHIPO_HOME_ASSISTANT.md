# CHIPO y Home Assistant

CHIPO puede controlar Home Assistant por voz con la **API REST documentada** de Home Assistant. Está apagado
mientras no haya una dirección y un token guardados en el dispositivo: sin ellos no hace nada y el asistente
contesta como siempre. Código en `components/chipo_ha`.

Estado: lógica y controlador probados en PC y compilados en GitHub Actions. **No probado contra un Home Assistant
real ni en la placa.** La dirección y el token se guardan con `scripts/chipo_ha_provision.py` (ver «Configuración»).

## Qué puede hacer

| Frase | Llamada |
| --- | --- |
| «activa la escena cine» | `POST /api/services/scene/turn_on` con `{"entity_id":"scene.cine"}` |
| «enciende la luz del salón» | `POST /api/services/light/turn_on` con `light.salon` |
| «apaga la luz de la cocina» | `POST /api/services/light/turn_off` con `light.cocina` |
| «apaga todas las luces» | `light/turn_off` con `entity_id: all` |
| «¿qué temperatura hay en el salón?», «¿está abierta la puerta?» | `GET /api/states/<entidad>` de un sensor que tú hayas dado de alta (ver «Leer sensores») |
| «buenas noches» / «buenos días» (BUDDY) | ejecuta la escena de noche / de mañana que hayas configurado |

El nombre hablado se convierte en el identificador: minúsculas, sin acentos, espacios a `_` (`Salón de abajo` →
`light.salon_de_abajo`). **Tus entidades de Home Assistant tienen que llamarse así**, o renombrarlas allí.
Si la entidad no existe o Home Assistant no responde, CHIPO lo dice en pantalla («no responde…»).

La llamada de red se hace en una tarea aparte (nunca en la del interfaz) con un tiempo máximo de 6 s. Solo se envía
el token en la cabecera `Authorization: Bearer`, nunca se escribe en el registro ni en pantalla. Con `https://` se
valida el certificado con el paquete de certificados de ESP-IDF.

## Música (media_player)

Con la clave `media_player` (por ejemplo `media_player.salon`) CHIPO controla el reproductor de Home Assistant:

| Frase | Servicio (`POST /api/services/media_player/…`) |
| --- | --- |
| «pon música», «continúa la música» | `media_play` |
| «pausa la música», «para la música» | `media_pause` |
| «siguiente canción» | `media_next_track` |
| «canción anterior» | `media_previous_track` |
| «sube / baja el volumen de la música» | `volume_up` / `volume_down` |

Siempre hace falta decir «música», «canción», «tema» o «reproductor»: «sube el volumen» a secas sigue siendo del asistente.
CHIPO no reproduce la música ni elige qué suena (eso lo decide lo que ya tengas en ese reproductor); solo manda
el servicio. Sin `media_player` válido no hace nada. Script: `--media-player media_player.salon`. Probado en PC con
un cliente simulado; no contra un Home Assistant real.

## Leer sensores

CHIPO solo lee los sensores que le des de alta, cada uno con el nombre con el que lo dirás en voz alta
(clave `sensors`, formato `nombre hablado=entidad;otro nombre=entidad`, hasta 12):

    temperatura salon=sensor.salon_temperatura;humedad=sensor.salon_humedad;puerta=binary_sensor.puerta

Frases: «¿qué temperatura hay en el salón?», «¿cuánta humedad hay?», «¿está abierta la puerta?». La frase tiene que ser
una pregunta (qué, cuál, cuánta, dime, hay, está...) y contener **todas** las palabras del nombre; si coinciden varios,
gana el nombre más largo. Responde en pantalla con el valor y la unidad («Temperatura salon: 21.5 grados») y una
campanilla; `on/off/open/closed/unavailable...` se traducen («encendido», «abierto», «no disponible»). Se pide con
`GET <url>/api/states/<entidad>` (tiempo máximo 6 s, respuesta limitada a 8 KB, en una tarea aparte) y se leen
solo `state`, `attributes.unit_of_measurement` y `attributes.friendly_name`. Sin sensores configurados esto no hace
nada y el asistente contesta como siempre. La respuesta se muestra, no se pronuncia: la voz de CHIPO la genera el
servidor y no hay voz local.

## Configuración

La dirección y el token se leen de la NVS del dispositivo, espacio de nombres `chipo_ha`, claves de texto:
`url`, `token` (token de acceso de larga duración de tu perfil de Home Assistant), `scene_night`, `scene_morning`,
`sensors` (arriba) y `media_player` (reservada para el control de música). **No hay credenciales en el código ni en el
repositorio.**

### Guardarlas con el script (`scripts/chipo_ha_provision.py`)

Desde una terminal de ESP-IDF (o con `pip install esp-idf-nvs-partition-gen esptool`), con la placa por USB:

    python scripts/chipo_ha_provision.py --url http://homeassistant.local:8123 ^
        --sensor "temperatura salon=sensor.salon_temperatura" --sensor "humedad=sensor.salon_humedad" ^
        --scene-night scene.buenas_noches --scene-morning scene.buenos_dias ^
        --port COM5 --flash

- El token se pide oculto en la terminal (o `--token-file`, o la variable `CHIPO_HA_TOKEN`). **No existe un `--token` en
  la línea de comandos** para que no quede en el historial.
- Sin `--flash` solo crea `chipo_ha_nvs.bin`; con `--flash` lo graba en `0x9000` (la partición `nvs` de la tabla de 16 MB
  de XiaoZhi, 0x4000 bytes) y borra el archivo después (`--keep` lo conserva; contiene el token **en claro**).
- **Advertencia importante:** grabar la imagen **sustituye toda la NVS**: se pierden las redes Wi-Fi guardadas y los
  ajustes de CHIPO (color de ojos, recordatorios). Hazlo **antes** de configurar el Wi-Fi la primera vez, o vuelve a
  configurar el Wi-Fi después. Guardar solo algunas claves sin borrar el resto no se ha implementado.
- Validado en PC (formato de entidades y direcciones, imagen de 16 KB con el espacio `chipo_ha`). **No probado en la
  placa**: la dirección 0x9000 sale de la tabla de particiones del repositorio, hay que comprobarla en la placa real
  (`idf.py partition-table` o el registro de arranque) antes del primer uso.

## Alternativa del lado del servidor (MCP)

XiaoZhi también permite conectar Home Assistant desde el servidor mediante herramientas MCP, sin guardar nada en
la placa. Es una vía independiente de esta; si tu servidor ya la ofrece, funciona sin tocar el firmware.

## Qué falta

- Guardar la configuración sin borrar el resto de la NVS (por ejemplo desde una página web de CHIPO).
- Probarlo con un Home Assistant real (lectura de sensores y control).
