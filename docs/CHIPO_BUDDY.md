# CHIPO BUDDY: temporizador, pomodoro, cronómetro, alarma y modo dormir

Todo se maneja **por voz** con las frases de abajo. CHIPO responde con un sonido corto y un texto en pantalla,
y el asistente del servidor **no** repite la respuesta (igual que en el modo DRIVE). Código en `components/chipo_buddy`.

Estado: probado solo en PC (lógica de voz, tiempo, pantalla y controlador con LVGL, ASan/UBSan) y compilado en
GitHub Actions. **No probado en la placa.** Ver «Qué falta comprobar».

## Qué dice y qué hace

| Frase (con o sin «Chipo,» delante) | Resultado |
| --- | --- |
| «temporizador de 10 minutos», «avísame en media hora», «cuenta atrás de 30 segundos», «temporizador de una hora y media» | Cuenta atrás. Se ve 6 s y se esconde; sigue corriendo. |
| «cuánto queda» | Vuelve a mostrar la cuenta (si no hay nada en marcha, contesta el asistente). |
| «cancela el temporizador», «para el pomodoro» | Cancela. |
| «pomodoro», «modo concentración» | 25 min de concentración, 5 de descanso, descanso largo de 15 min cada 4 rondas. Sonido en cada cambio. |
| «cronómetro», «para el cronómetro» | Cronómetro (queda visible el tiempo final). |
| «despiértame a las 7 y media», «alarma a las 6:45», «alarma a las ocho menos cuarto de la mañana» | Alarma de una sola vez. Necesita la hora real (ver abajo). |
| «cancela la alarma» | Quita la alarma. |
| «buenas noches», «modo dormir» | Modo dormir: pantalla negra con la hora en ámbar tenue (y la alarma si hay). |
| «buenos días», «sal del modo dormir» o **pulsación larga** en pantalla | Sale del modo dormir. |
| «qué hora es» | Muestra la hora 6 s; **la dice el asistente por voz**. |
| «para», «vale», «silencio» | Para el temporizador o alarma que esté sonando. |

Cuando el temporizador o la alarma suenan, la pantalla se pone roja y el sonido se repite cada 4 s (8 veces como
máximo). **Un toque** en la pantalla la para; si es la alarma, **la pospone 9 min** (hasta 3 veces). Si nadie
contesta, el temporizador termina solo y la alarma se pospone sola (hasta 3 veces).

Un toque corto en modo dormir **no** lo despierta, para que un golpe en la mesita no lo quite.

Las frases largas no cuentan como órdenes (más de 16 palabras se ignoran), y «buenos días» o «para» solo se
interpretan como órdenes cuando hay un modo dormir o un sonido activos; en cualquier otro caso contesta el asistente.
No se activa el modo dormir mientras el modo DRIVE está encendido.

## La pantalla

Reloj retro de cuatro dígitos de siete segmentos, dibujado con rectángulos de LVGL (no necesita fuentes). Por debajo
de una hora muestra minutos:segundos; desde una hora, horas:minutos (el texto de abajo lo indica). Colores:
ámbar (concentración, hora), cian (temporizador), verde (descanso, cronómetro), rojo (suena).

## La hora real

La alarma y el reloj necesitan la hora real. XiaoZhi la recibe del servidor al arrancar (con el huso horario ya
sumado) y CHIPO la lee del reloj del sistema. Antes de recibirla (sin Wi-Fi) el reloj no muestra cifras y **la
alarma no suena**; los temporizadores no dependen de ella.

## Sonidos

`buddy_chime`, `buddy_timer_done` y `buddy_alarm` son tonos sintetizados (sin grabaciones ni derechos). Están en
`components/chipo_drive/clips/` (junto a los del modo DRIVE) y se regeneran con `python3 scripts/make_buddy_sfx.py`.

## Límites conocidos

- Las alarmas y temporizadores **no sobreviven a un reinicio** (no se guardan en memoria).
- Un solo temporizador a la vez, más el pomodoro y el cronómetro.
- «alarma a las doce de la noche» es medianoche; «a las doce» a secas es mediodía.
- No hay detección de golpes en la mesa para posponer: se pospone con un toque en la pantalla.
- Si un sonido falta, se muestra el texto y el asistente contesta por voz como siempre.

## Qué falta comprobar con la placa

- Que el reconocimiento devuelve estas frases como se espera (cifras «10» o palabras «diez»; ambas están cubiertas).
- Que el sonido de la alarma se oye bien por el altavoz y a qué volumen.
- Que la hora del sistema llega con el huso horario correcto en tu servidor.
- Consumo en modo dormir (la pantalla AMOLED con fondo negro apenas consume, pero no está medido).
