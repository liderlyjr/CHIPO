# CHIPO AMBIENT — lluvia, fuego y ruido suave de fondo

"Chipo, pon sonido de lluvia" · "ponme la chimenea" · "ruido blanco" (o "ruido de fondo / suave / rosa / marrón").
Para parar: "para la lluvia", "apaga el sonido", "quita el ruido" (solo entiende esto mientras suena algo).

## Cómo funciona (y por qué así)
El audio de XiaoZhi tiene una cola de reproducción de solo **20 paquetes Opus (1,2 s)** y `PlaySound` bloquea a quien lo
llama hasta meter todo el clip. Un clip largo congelaría el bucle principal, así que cada sonido son **9 trozos de 0,96 s**
(16 paquetes de 60 ms), partes consecutivas de **un mismo flujo** codificado (`scripts/make_ambient_sfx.py`, síntesis pura,
sin grabaciones; ~28 KB por sonido), y CHIPO encola **uno por segundo**. El bucle completo dura 8,64 s y está hecho para
unirse sin salto. Se regeneran con `python3 scripts/make_ambient_sfx.py` (numpy, scipy, ffmpeg con libopus).

- **Se pausa** mientras el asistente escucha o habla (XiaoZhi vacía su cola entonces) y **vuelve solo** al terminar.
- **Se apaga solo a los 30 minutos** («Sonido ambiente apagado (30 min)»): reproducir seguido mantiene encendido el códec y
  gasta batería; no hay medición de cuánto.
- Si falta el clip, lo dice en pantalla y deja contestar al asistente.
- La cara pone gesto relajado un momento al empezar.

## Qué NO está comprobado (no se ha probado en la placa)
- Que el empalme entre trozos suene **sin cortes**: depende del tiempo real del bucle principal y del decodificador de la
  placa. Las constantes (`kPieceMs`, el trozo adelantado, la pausa al reanudar) están en `chipo_ambient.h/.cpp` por si hay
  que ajustarlas. En PC solo se prueba el ritmo (un trozo cada 0,96 s ±1 tick), el orden, la pausa y la parada.
- El volumen: se normaliza todo a RMS 0,09 (suave), pero lo que se oye depende del volumen del altavoz de la placa.
- El consumo de batería con el sonido continuo.

## Música
Con un `media_player` de Home Assistant configurado, «pon música», «pausa la música», «siguiente canción»,
«canción anterior», «sube/baja el volumen de la música» llaman a los servicios `media_player/*` de Home Assistant. CHIPO
**no reproduce música él mismo**: suena el altavoz que controla Home Assistant. Ver `docs/CHIPO_HOME_ASSISTANT.md`.
