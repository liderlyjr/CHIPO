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
fuente: ejemplo `92_qmi8658_imu` de Waveshare). CHIPO solo lee el acelerómetro (el giroscopio queda apagado)
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

## Pruebas

`tests/host/run.sh` (con `LVGL_DIR` apuntando a LVGL 9.5) ejecuta en un PC, con AddressSanitizer y UBSan:

- `test_tilt`: la matemática de la inclinación con los cuatro montajes posibles del sensor, ruido, caída libre,
  tumbado, cambio de postura de reposo y arranque tumbado.
- `test_face`: la cara completa sobre LVGL real con reloj y acelerómetro simulados: parpadeos, transiciones suaves,
  Pensando, Hablando, caída a ambos lados, balanceo, mareo, sueño, errores y parada a medias.

GitHub Actions las ejecuta antes de compilar el firmware.

Lo que estas pruebas **no** pueden confirmar: la lectura real del sensor, hacia qué lado cae en la placa
(el ajuste de espejo) y la fluidez en la pantalla AMOLED. Eso solo se ve con la placa.
