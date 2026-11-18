# CHIPO DRIVE — modo copiloto para el coche

Todo está en `components/chipo_drive` (lógica y controlador) y en `components/chipo_face` (gafas y reacciones).
**Estado:** compila en GitHub y pasa los tests simulados en PC. **No se ha probado en la placa ni en un coche**:
mira "Qué falta comprobar" al final.

## Cómo se activa (solo por voz)

- **Activar:** «Chipo, vámonos» (también «vámonos» a secas, porque la palabra de activación ya abrió la conversación;
  «CHIPO DRIVE» y «activa el modo conducción» también valen).
- **Desactivar:** «desactiva el modo conducción», «Chipo, vuelve al modo escritorio», «Chipo, sal del modo coche».
- **Sensibilidad:** «sensibilidad suave», «sensibilidad normal», «sensibilidad intensa» (en cualquier momento).
- Una frase larga que solo *contiene* esas palabras («vámonos a comer a casa de mi madre») **no** activa nada.
- No se activa por USB, cargador, contacto ni nada eléctrico. Al reiniciar, CHIPO arranca siempre en modo escritorio.
- No hay un segundo reconocedor de voz: se usa la palabra de activación y el texto que devuelve el servidor
  (mensaje `stt` de XiaoZhi). **Necesita Internet**, como cualquier conversación con CHIPO.
- La palabra de activación "chee po" sigue sin probarse en la placa (ver `docs/WAKE_WORD.md`).

## Qué pasa al activarlo

1. CHIPO reconoce el comando y reproduce el "brum, brum… ¡vámonos!" (ver "Voz" abajo).
2. Las gafas de sol bajan desde arriba hasta los ojos (1,1 s, con un pequeño rebote al asentarse) y se quedan.
3. Al salir, se deslizan hacia arriba (0,8 s) y CHIPO vuelve a su cara de siempre.

Las gafas no existen antes de activar el modo. Dentro del modo, **se pausan** las reacciones de escritorio que la
carretera dispararía sin parar: caída por inclinación, mareo, sacudida "no", giro y asustarse por un golpe.
Fuera del modo todo funciona exactamente como antes (hay un test que lo comprueba).

## Qué datos usa cada sensor

| Sensor | Para qué |
|---|---|
| Acelerómetro | Gravedad de referencia (al quedar quieto en su soporte), aceleración lineal = lectura − referencia, separada en vertical, adelante/atrás y lateral. |
| Giroscopio | (1) Mientras el aparato gira o se agita, la referencia de gravedad **no se actualiza** (una curva larga no se "absorbe" como gravedad). (2) Una curva exige giro sostenido **alrededor de la vertical** junto con aceleración lateral. (3) Distingue "se movió el aparato" (cogerlo en la mano) de "se movió la carretera". |

Lectura a 50 Hz solo dentro del modo (25 Hz fuera). Todas las muestras llegan al detector (cola de 16), no solo la última.

## Cómo se detecta cada cosa

Umbrales para sensibilidad NORMAL (en `chipo_drive_motion.cpp`); SUAVE los multiplica por 1,35 e INTENSA por 0,7.

- **Aceleración:** hacia delante > 0,14 g durante 300 ms (sale por debajo de 0,08 g). Cara de entusiasmo + empujón hacia atrás.
- **Frenada:** hacia atrás > 0,18 g durante 250 ms (sale por debajo de 0,10 g). Ojos sorprendidos + las gafas resbalan.
- **Curva:** giro sobre la vertical > 8 °/s y aceleración lateral > 0,09 g durante 350 ms, con el giro casi todo sobre la vertical
  y una relación giro/aceleración físicamente coherente (solo como comprobación interna: **CHIPO no mide ni muestra velocidad**).
  La cara se inclina hacia el lado del empuje (como un pasajero), los ojos miran hacia dentro de la curva y las gafas acompañan.
- **Bache:** golpe vertical > 0,30 g en 2 muestras seguidas y mayor que el movimiento horizontal. Rebote corto.
- **Movimiento brusco:** > 0,55 g horizontal, o giro > 45 °/s con 0,18 g laterales. Sorpresa o mareo breve.
- Histéresis (entra y sale con umbrales distintos), enfriamiento (0,9–3 s según el evento) y silencio de 450 ms tras un bache.
  Las vibraciones normales (ruido de 0,05 g) no disparan nada (test de 60 s simulados).

### Curva frente a girar el aparato con la mano

Es la parte delicada. El detector solo da por buenos los movimientos si:
- La gravedad sigue cerca de la referencia del soporte (< 35°) **y** el giro "fuera de la vertical" (inclinar, voltear)
  promediado en 0,8 s es < 18 °/s. Si no, está en una mano: **ni curvas, ni aceleración, ni frenada**.
- Girarlo como un plato giratorio sobre la mesa (mucho giro, casi nada de aceleración lateral) se descarta por la comprobación de coherencia.
- Cada curva aceptada "vota" el signo de la relación giro/aceleración; tras dos curvas, una que no cuadre se rechaza.
- Si se coge y se deja en otro sitio, tras 1,5 s quieto en vertical toma una referencia nueva.

### Eje "adelante"

Se supone CHIPO **en vertical sobre el salpicadero con la pantalla hacia el conductor**: la horizontal de la pantalla es el eje
lateral del coche y la normal de la pantalla es el eje longitudinal. Qué sentido es "adelante" **se aprende del primer empujón tras
quedarse quieto desde que se activa el modo** (por eso: *activa el modo con el coche parado y luego arranca*). Hasta entonces no
distingue aceleración de frenada (curvas, baches y bruscos sí funcionan). Se puede fijar con `CHIPO_DRIVE_FORWARD_SIGN=1` o `-1`.
Hay que fijar CHIPO bien al coche; si baila libre, las lecturas no valen.

Si la cara se inclina al revés en las curvas, pon `CHIPO_DRIVE_MIRROR_X=1` (igual que `CHIPO_TILT_MIRROR_X`).

## Personalidad

17 frases distintas (aceleración, frenada, curva, bache, brusco, calma). Para que no hable sin parar:
- Los primeros 20 s de cada trayecto, callado.
- Silencio mínimo entre comentarios: 45 s (SUAVE), 28 s (NORMAL), 16 s (INTENSA).
- Probabilidad por evento (30–65 %, escalada por sensibilidad e intensidad), máximo 4 comentarios cada 5 minutos.
- No repite las dos últimas frases de un tipo.
- No habla si el asistente está escuchando, pensando o hablando.
- Comentarios "de calma" («Jefe, vamos finos») solo tras 30 s sin eventos y cada 1,5–3 min como mucho.

## Voz: los clips

CHIPO no tiene sintetizador de voz propio (la voz viene del servidor), así que las frases de DRIVE son **clips grabados**
(OGG/Opus 48 kHz mono) incrustados en el firmware desde `components/chipo_drive/clips/<nombre>.ogg`.

- `drive_start_sfx.ogg` ya está: un "brum, brum" de motor generado por ordenador (marcador de posición; puedes cambiarlo).
- Si **no** hay `drive_start.ogg` (la frase completa grabada): suena el motor, sale el texto "Brum, brum… ¡vámonos!" y
  **contesta la voz del servidor** con sus propias palabras (no puedo garantizar qué dirá).
- Si **hay** `drive_start.ogg`: suena ese clip y se descarta la respuesta hablada del servidor a ese comando.
- Sin clip para un comentario, sale el texto en pantalla 2,5 s.

Lista de frases y nombres: `python scripts/make_drive_clips.py --list`. Para convertir tus grabaciones (cualquier formato):
`python scripts/make_drive_clips.py carpeta_con_grabaciones` (o `scripts\make_drive_clips.bat carpeta`) y recompila.
Necesita ffmpeg.

## Registro para ajustar en un trayecto real

Dentro del modo, una línea por segundo en el puerto serie: `mounted, fwd_known, yaw, lat, fwd, vert, curve`.
Apagar con `-DCHIPO_DRIVE_DEBUG_LOG=0`.

## Archivos

Nuevos: `components/chipo_drive/*` (voz, movimiento, personalidad, controlador, CMake con los clips),
`scripts/make_drive_clips.py|.bat`, `tests/host/test_drive*.cpp`, `tests/host/render_drive.cpp`, este documento.
Modificados: `components/chipo_face/*` (gafas, reacciones, observador de muestras, cola de muestras IMU a 50 Hz, `t_ms` en `ImuSample`),
`scripts/integrate_chipo.py` (ganchos en XiaoZhi), `tests/host/run.sh`.

Ganchos en XiaoZhi (`integrate_chipo.py`): crear `ChipoDrive` tras la cara; pasar el texto reconocido (`stt`) a `OnVoiceText`;
si CHIPO contesta con su sonido, ignorar el `tts start` siguiente (8 s de plazo); reproducir clips con `AudioService::PlaySound`.

## Qué falta comprobar en la placa y en un coche

- Que compila en el firmware completo (se comprueba en GitHub Actions) y arranca.
- Que la palabra "chee po" + «vámonos» se reconoce en la práctica y el texto llega como esperado.
- Que el sonido de los clips sale bien por el altavoz mientras el servidor calla.
- Orientación real de los ejes del QMI8658 respecto a la pantalla (si la cara se inclina al revés: `CHIPO_DRIVE_MIRROR_X`).
- Umbrales con un trayecto real: ruido del coche, firmeza del soporte, curvas lentas y rápidas, baches de verdad.
- Que el "adelante" aprendido es el correcto con tu soporte (si no, fija `CHIPO_DRIVE_FORWARD_SIGN`).
- Consumo con el modo activo (50 Hz de IMU) y temperatura al sol.
- Los tests son simulaciones con ruido sintético; no hay ninguna grabación de un coche real.
- Seguridad: CHIPO es un juguete. No mira la pantalla mientras conduces y no uses el modo para nada que distraiga.
