# CHIPO PET — ánimo de la mascota y juegos por voz

CHIPO tiene un **ánimo** de 0 a 100 que sube cuando le haces caso y baja poco a poco si lo ignoras.
No guarda el ánimo entre reinicios: cada arranque empieza en 70 ("bien").

## Cómo cambia el ánimo
| Qué haces | Efecto |
|---|---|
| Hablarle (cualquier frase reconocida) | +2 |
| Tocar la cara | +3 |
| Cosquillas (3 toques) | +6 |
| Una ronda de juego | +4 |
| Preguntarle "¿cómo estás?" | +8 |
| 30 min sin atención | empieza a bajar: −1 cada 6 min |

Niveles: **feliz** (≥70), **bien** (40–69), **solito** (15–39), **abandonado** (<15).
Si está solito o abandonado, de vez en cuando (máx. cada 15 min) pone cara triste o cansada unos segundos.
Es solo la cara: no hace ruido, no molesta.

## Por voz
- "Chipo, ¿cómo estás?" / "cómo andas" / "qué tal estás": contesta según su ánimo (texto + cara + campanilla).
- **Piedra, papel o tijera**: "juguemos a piedra papel o tijera", luego di "piedra", "papel" o "tijera".
  Muestra qué elige CHIPO, quién gana y el marcador.
- **Adivina el número**: "adivina el número" o "piensa un número". Piensa uno del 1 al 10; di números ("siete", "el 3")
  y responde "más alto" / "más bajo". Tienes 5 intentos.
- "Ya no quiero jugar" / "basta": termina. Un juego abandonado se cierra solo tras 60 s.

## Límites
- Las respuestas son **texto en pantalla + un sonido**; no hay voz hablada local (la voz de CHIPO es del servidor).
- Mientras hay un juego activo, "piedra", "papel", "tijera" y los números 1–10 los atiende CHIPO; fuera del juego no.
- Se probó en un PC con tiempo simulado. No se ha probado en la placa; el reconocimiento real de las frases depende
  de cómo las transcriba el servidor.
- No hay juego de preguntas (quiz): necesitaría un banco de preguntas curado y no se hizo.
