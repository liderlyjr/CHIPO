# CHIPO Face — animaciones y reacción a la inclinación

Todo está en `components/chipo_face`. Los valores ajustables están en `include/chipo_config.h`
(se pueden cambiar con `-DNOMBRE=valor` sin editar el archivo).

## Cómo se anima

- Cada gesto se suaviza: ojos, pupilas, boca y brillo se acercan al valor objetivo, nada salta de golpe.
- El parpadeo va por tiempo (cierra en 70 ms, se queda 45 ms, abre en 90 ms), cada 2,8–6,2 s, y a veces es doble.
- Ojos y boca crecen desde su centro, así que no "suben y bajan" al cambiar de tamaño.
- La cara completa va sobre un muelle: cuando se mueve, se pasa un poco y vuelve (balanceo).
- Solo se redibuja lo que cambia; si nada cambia, no se toca la pantalla.
- Nada de la cara captura toques: la interfaz de XiaoZhi que hay debajo sigue respondiendo.

## Estados

| Estado | Qué se ve | Cuándo |
|---|---|---|
| Reposo | ojos normales, boca plana, la mirada vaga sola | estado Idle de XiaoZhi |
| Escuchando | ojos más grandes, boca pequeña redonda, brillo que "respira" | Listening |
| Pensando | pupilas girando, tres puntos que parpadean bajo la boca | XiaoZhi reconoce lo que dices (`stt`) y todavía no contesta |
| Hablando | la boca se abre y cierra (no sigue el audio real) | Speaking |
| Conectando | ojos algo más pequeños, mirada vagando | arranque, Wi-Fi, activación |
| Error | ojos achatados, boca baja | error fatal |
| Dormido | ojos cerrados, brillo que respira despacio | 2 min en reposo sin voz ni toques (`CHIPO_SLEEP_AFTER_MS`) |

"Pensando" se apaga solo al empezar a hablar, al dar error, o a los 25 s si el servidor no responde.
Despierta con voz, toque, inclinación o sacudida.

## Reacción a la inclinación (acelerómetro QMI8658)

La placa tiene un QMI8658 en el mismo bus I2C que usa XiaoZhi (puerto 0, SDA 15, SCL 14, dirección 0x6B;
fuente: ejemplo `92_qmi8658_imu` de Waveshare). CHIPO lee el acelerómetro y el giroscopio
a 25 lecturas por segundo y no toca ningún archivo de la placa de XiaoZhi.

- **Postura de casa:** Waveshare no documenta cómo está orientado el sensor respecto a la pantalla, así que CHIPO
  lo averigua solo: la primera vez que está quieto 1,2 s y más o menos derecho, esa postura queda guardada.
  Si arranca tumbado, espera a que lo pongas de pie. Si lo dejas apoyado en otra postura (menos de 30° de
  diferencia) más de 10 s, esa pasa a ser la nueva "casa".
- **Inclinación suave** (menos de 45°): la cara se desliza un poco hacia el lado que tira la gravedad, las pupilas miran
  hacia ahí y, si se inclina hacia delante o atrás, la cara se hunde un poco.
- **Resbalón y caída** (45° o más, `CHIPO_FALL_ANGLE_DEG`): la cara pone cara de susto (ojos enormes, pupilas diminutas,
  boca en "O"), **resbala hacia el lado al que lo inclinas, acelera, se cae y sale de la pantalla** en menos de un
  segundo. Mientras sigas teniéndolo así de inclinado, la pantalla se queda vacía. Al enderezarlo (por debajo de 33°) la
  cara **vuelve a entrar deslizándose desde ese mismo borde**, rebota un poco y hace un "uff" con parpadeo.
  Si lo inclinas hacia delante o hacia atrás (sin tirón hacia un lado) no hay hacia dónde resbalar: solo se asusta.
  Una sacudida o una caída libre también lo asustan, sin que la cara salga de la pantalla.
- **Tumbado** (más de 75° durante 1,5 s): mareado. La cara, si se había caído, **vuelve** con los ojos entrecerrados y
  pupilas que bailan.

### Si cae hacia el lado contrario

El código distingue arriba/abajo solo, pero no puede saber qué lado del sensor es la derecha de la pantalla.
Si al inclinar CHIPO hacia su derecha la cara se desliza hacia la izquierda, pon `CHIPO_TILT_MIRROR_X` a `1`
en `chipo_config.h` (o `-DCHIPO_TILT_MIRROR_X=1`) y vuelve a compilar.

Esto también decide hacia qué lado cae. Para comprobarlo, con el monitor serie abierto verás una línea por segundo:

```
tilt: calibrated=1 angle=20.0 push_x=0.34 lean_z=0.00 lying=0 mood=0
```

`push_x` positivo debe salir al inclinar hacia la **derecha** de la pantalla. Cuando todo funcione,
pon `CHIPO_DEBUG_TILT_LOG` a `0` para quitar esa línea.

### Si no hay sensor

Si el QMI8658 no responde, CHIPO lo anota en el registro y sigue funcionando sin reacciones a la inclinación.
`CHIPO_ENABLE_IMU=0` lo desactiva del todo.

## Giroscopio (gestos)

El giroscopio va configurado a ±512 °/s (registros CTRL3, CTRL5 y CTRL7 y escala de 64 cuentas por °/s, tomados de
SensorLib, `SensorQMI8658_Reg.hpp`). Si el sensor lo rechaza, se queda solo con el acelerómetro, como antes;
`CHIPO_ENABLE_GYRO=0` lo apaga. Solo se usa lo que no depende de hacia dónde apunten los ejes del sensor: el tamaño del giro
y cuántas veces cambia de sentido, nunca el sentido.

| Gesto | Cómo se reconoce | Qué hace CHIPO |
|---|---|---|
| **Girar** (como un volante) | rotación rápida (más de 200 °/s) alrededor de la normal de la pantalla durante 250 ms | cara **Whee** mientras gira (pupilas dando vueltas, boca grande, brillo violeta) |
| **Girar mucho rato** | el giro anterior dura 2,5 s o más | al parar queda **mareado** 2,8 s |
| **Asentir** ("sí") | vaivén rápido alrededor del eje horizontal de la pantalla: tres cambios de sentido seguidos (más de 90 °/s) | **Happy** 1,8 s y una pequeña cabezada |
| **Negar** ("no") | lo mismo alrededor del eje vertical de la pantalla | **No** 1,8 s (pupilas de un lado a otro) |

Asentir y negar solo funcionan cuando CHIPO ya ha averiguado su postura de casa (ver arriba). Un giro suelto o un
movimiento lento no cuentan, ni un meneo en diagonal (ambos ejes a la vez); entre un gesto y el siguiente pasan 1,5 s.
Todos despiertan a CHIPO si estaba dormido.

## Emociones

Se ponen encima de Reposo, Escuchando y Hablando (hablando, la boca sigue la voz y cambian los ojos y el brillo). No se ven
en Pensando, Conectando, Error ni Dormido, ni mientras está asustado o mareado. Terminan solas y despiertan a CHIPO.

| Emoción | Cómo se ve |
|---|---|
| Happy | ojos entrecerrados, sonrisa ancha, brillo cálido |
| Laugh | ojos entrecerrados, boca que rebota |
| Wink | ojo derecho cerrado, sonrisa de lado |
| Love | brillo rosa que late, pupilas grandes y redondas |
| Sad | pupilas hacia abajo, boca pequeña y baja, brillo apagado |
| Tired | ojos a medias y un bostezo cada 5 s; sale sola los últimos 20 s antes de dormirse (`CHIPO_TIRED_BEFORE_SLEEP_MS`) |
| Suspicious | un ojo más estrecho, mirando de lado, brillo ámbar |
| Surprised | ojos muy abiertos, pupilas pequeñas, boca en "O" |
| Whee | ver Girar |
| No | ver Negar |
| Grumpy | ojos planos, boca plana y baja, brillo rojo |

- **Solas:** en reposo, de vez en cuando (cada 25–50 s, la primera a los 30–50 s) guiña o sonríe 1,4 s. No cuenta como actividad:
  CHIPO se duerme igual. `CHIPO_IDLE_QUIRKS=0` o `SetIdleQuirks(false)` lo quitan.
- **Desde código:** `face->ShowEmotion(ChipoFace::Emotion::Love, 3000)`, o con el nombre que manda el asistente:
  `face->ShowEmotionByName("loving")`. Reconoce happy, laughing, funny, silly, winking, loving, kissy, sad, crying,
  embarrassed, sleepy, confused, surprised, shocked, angry (más delicious, relaxed, confident y cool como Happy);
  `neutral` y los nombres desconocidos no hacen nada.
- **Pendiente:** todavía **no** está conectado a los mensajes de emoción de XiaoZhi (el campo `emotion` de los mensajes
  `llm`): `scripts/integrate_chipo.py` no lo engancha, y la lista de nombres sale de memoria, no de comprobar el código de XiaoZhi.
  Hasta entonces las emociones salen de los gestos, de las manías en reposo, del cansancio y de `ShowEmotion`.

## Pruebas

`tests/host/run.sh` (con `LVGL_DIR` apuntando a LVGL 9.5) ejecuta en un PC, con AddressSanitizer y UBSan:

- `test_gyro`: los gestos del giroscopio (girar, asentir, negar), sus cooldowns, ejes espejados, ruido y entradas absurdas.
- `test_tilt`: la matemática de la inclinación con los cuatro montajes posibles del sensor, ruido, caída libre,
  tumbado, cambio de postura de reposo y arranque tumbado.
- `test_face`: la cara completa sobre LVGL real con reloj, acelerómetro y giroscopio simulados: parpadeos, transiciones suaves,
  Pensando, Hablando, caída a ambos lados, balanceo, mareo, sueño, errores, parada a medias, gestos del giroscopio, las once
  emociones (forma, color, que no tapen Pensando ni Error), nombres de emoción, guiños en reposo y cansancio antes de dormir.

GitHub Actions las ejecuta antes de compilar el firmware.

Lo que estas pruebas **no** pueden confirmar: la lectura real del sensor (acelerómetro y giroscopio), hacia qué lado cae en la placa
(el ajuste de espejo), si los umbrales de los gestos (200 °/s para girar, 90 °/s para asentir y negar) son cómodos con la
placa en la mano, y la fluidez en la pantalla AMOLED. Eso solo se ve con la placa.

## Animaciones extra (corazones, lágrimas, disfraces, baile…)
Ver [CHIPO_ANIMACIONES.md](CHIPO_ANIMACIONES.md). API nueva de `ChipoFace`: `LookAt/StopLookAt`, `Celebrate(Burst)`, `SlowBlink`, `Cry`,
`SoreLoser`, `SetCostume/SetDateSource`, `SetDance/SetLevelSource`; emoción nueva `Emotion::Dance`.

## Estilo «bonito» (ojos lilas y sonrisa) sobre negro

Pensado para aprovechar el negro real del AMOLED: fondo negro, ojos lilas con pupila negra y dos brillos, y una
boca de media luna rosa con lengua roja (la boca es una ventana que recorta la mitad inferior de una elipse).
El color del estado (escuchando, error, amor…) sigue tiñendo ojos y boca. Imagen: `docs/img/chipo_cute_skin.png`
(simulación en PC). **Sin ver en la pantalla real.** Las mejillas se probaron y se quitaron.

## Revisión de diseño (2.16", 480×480)

- Bocas según la expresión: sonrisa de media luna (con interior y lengua si está abierta), mueca hacia abajo
  (triste, enfadado, error, "no"), "o" (sorpresa, bostezo, mareo, susto, dormido) y línea (pensando, conectando,
  sospecha).
- Ojos cerrados en arco: ^ ^ al estar feliz o reír (melocotón) y u u al dormir (lila).
- Rubor rosa solo cuando le gusta algo (feliz, risa, amor, guiño, baile).
- Paleta unificada: lila/violeta como color principal y rosa suave de acento (cara, arranque, ajustes);
  el rojo queda para avisos. El arranque muestra los ojos de CHIPO.
- Animaciones revisadas en GIF a 480×480: `docs/img/chipo_216_anim_*.gif` (simulación en PC).
- Límite conocido: el halo de brillo de los ojos no se escala con la cara (sombras grandes colgaban el render).
- Brillo de los ojos: ya no usa sombras de LVGL (en 480×480 un fotograma podía tardar segundos). Ahora es un aro de
  color en el borde del ojo más dos discos translúcidos detrás, que crecen con la cara. Aro cian al escuchar y
  hablar, rojo en error, del color de la emoción en las demás; en reposo, pensando y dormido los ojos van limpios.
- Caída al inclinar: en la 2.16" la cara sale del todo por el borde (antes se quedaba asomando) y vuelve al
  enderezar. GIF: `docs/img/chipo_216_anim_fall.gif`.
- Drive: la cara sube un poco y el sol es más pequeño y más bajo; la sonrisa queda cerrada (sin lengua).
- Ajustes: en pantallas anchas las barras y botones se alargan (hasta 440 px). Clima: márgenes mayores para
  no tocar las esquinas redondeadas de la 2.16".
- Reloj de mesa en la 2.16": GIF `docs/img/chipo_216_anim_desk_clock.gif` (CHIPO tumbado 90° a la izquierda →
  la cara se cae → a los 10 s sale la hora en grande, legible con la pantalla de lado → al levantarlo, mareo).
  En el GIF la espera de 10 s va acelerada. Arreglado: mientras la cara está fuera de la pantalla ya no aparece
  una barra de desplazamiento gris en el borde.
- Sensor (QMI8658) en la 2.16": Waveshare lo pone en el mismo bus I2C (GPIO15 SDA / GPIO14 SCL, puerto 0 que abre
  XiaoZhi). La dirección no está documentada; CHIPO prueba 0x6B y 0x6A, las dos únicas que admite el chip. El eje
  vertical se detecta solo. Sin probar en la placa: si cae hacia el lado contrario, CHIPO_TILT_MIRROR_X=1; si el
  reloj sale con el lado equivocado, cambiar el signo de CHIPO_DESK_CLOCK_SIDE.
- Reloj (y demás pantallas BUDDY: despertador, temporizador, noche): al aparecer, los segmentos se encienden
  dígito a dígito como tubos de neón (subida rápida, parpadeo y encendido completo, ~0,6 s); al desaparecer se
  apagan en orden inverso, de derecha a izquierda, con los dos puntos al final (~0,4 s). Solo cambia la opacidad
  de cada pieza (sin capas), barato para el ESP32. Al levantar CHIPO el reloj se apaga todavía girado y después
  la vista vuelve a vertical.
- Cara más grande: ojos y boca ocupan ~58 % del ancho de la pantalla (escala 1,76 en la 2,16").
  El halo no crece con la escala (borde suave, no mancha). En drive la cara sube lo justo para que la boca quede
  sobre el sol. `Debug()` informa los tamaños en píxeles de diseño y `face_scale` aparte.
