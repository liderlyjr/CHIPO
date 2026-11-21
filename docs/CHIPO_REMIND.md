# CHIPO REMIND — recordatorios que sobreviven a los reinicios

Complementa a la alarma y los temporizadores de BUDDY: aquí el recordatorio lleva **texto** y se **guarda** en la memoria
del dispositivo (NVS, espacio `chipo`, clave `reminders`).

## Por voz
| Frase | Resultado |
|---|---|
| «Chipo, recuérdame a las 18:30 sacar al perro» | Una vez, la próxima vez que sean las 18:30 |
| «recuérdame todos los días a las 8 tomar la medicina» | Cada día (también «cada día», «diariamente», antes o después de la hora) |
| «recuérdame a las seis y media de la tarde llamar a mamá» | Admite números en palabras y «y media», «y cuarto», «menos cuarto», «de la tarde/noche/mañana» |
| «qué recordatorios tengo» | Lista los tres primeros |
| «borra el recordatorio de las 18:30» / «borra los recordatorios» | Borra uno o todos |
| «vale» / «gracias» / «para» (mientras suena) | Lo calla |

Al llegar la hora: texto «Recordatorio: …», sonido de alarma y cara de sorpresa; se repite 3 veces cada 20 s (y espera si
el asistente está hablando o escuchando). Hasta **8** recordatorios.

## Límites (a propósito, sin adivinar)
- **No entiende fechas**: «mañana a las 8» o «el viernes a las 8» se rechaza con un aviso («solo sé recordar a una hora de
  hoy o todos los días»). Para eso haría falta un calendario.
- **Calendario**: no se ha hecho. Conectar Google Calendar u otro exige una cuenta y un acceso OAuth que no se puede probar
  aquí; queda fuera.
- Necesita la **hora real** (la obtiene del servidor al conectarse). Sin ella no suena nada. Si CHIPO está **apagado** a la
  hora del recordatorio, ese aviso se pierde. Con recordatorios guardados CHIPO ya no se apaga solo (ver
  [HARDWARE_2_16.md](HARDWARE_2_16.md#energía-atenuar-y-apagarse-solo)); solo se pierde si se queda sin batería o lo
  apagas tú. La placa tiene un reloj RTC (PCF85063), pero CHIPO todavía no lo usa: la hora viene del servidor.
- El texto se guarda **en minúsculas y sin acentos** («llamar a mama») porque así normaliza la frase el reconocimiento; al
  mostrarlo solo se pone la primera letra en mayúscula.
- Las llamadas «avísame en 10 minutos» siguen siendo de BUDDY (temporizador, no se guardan).
- Probado en PC con tiempo y reinicio simulados. No probado en la placa; el reconocimiento real de las frases depende de
  cómo las transcriba el servidor.
