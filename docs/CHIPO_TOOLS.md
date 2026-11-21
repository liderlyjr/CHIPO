# CHIPO TOOLS (V0.6.x, sin probar en la placa)

Herramientas pensadas para la pantalla cuadrada de 480×480. Todo está en `components/chipo_tools`.

| Herramienta | Cómo se usa |
|---|---|
| **Franja de información** | Hora arriba y temperatura abajo, tenues, en la cara en reposo. Se desplazan despacio contra el quemado AMOLED. Voz: «oculta/muestra la información». Guardado: `tools_info` (activa por defecto). |
| **Anillo de pomodoro** | Arco ámbar (trabajo) o verde (descanso) alrededor del borde mientras corre el pomodoro de BUDDY. |
| **Nivel de burbuja** | «Pon el nivel». Con CHIPO plano sobre la mesa muestra la burbuja y los grados; si no está plano lo pide. Toque para salir. |
| **Bola 8 mágica** | «Bola mágica». Agitar o tocar para preguntar; pulsación larga para salir. 20 respuestas en español; la cara reacciona. |
| **Pizarra** | «Vamos a dibujar». Dibujas con el dedo (color distinto por trazo), botones Salir/Borrar. Memoria propia en PSRAM, solo mientras está abierta. |
| **Subtítulos** | «Activa los subtítulos». Muestra lo que dice el asistente bajo la cara. Desactivados por defecto (`tools_subs`). |
| **Botón de usuario (solo 2,16")** | Un clic cambia de herramienta: nivel → bola 8 → pizarra → cerrar. Usa el botón GPIO18 que documenta Waveshare («Key3», pull-up de 10 k, pulsado = GND, «funciones personalizables»). |
| **Agitar** | Con la cara en reposo, agitar tira de nuevo los dados o la moneda de FUN. |

Los datos del tiempo se descargan en silencio 20 s después de arrancar y cada 30 min (reutiliza la pantalla del clima).

## Lo que NO está hecho
- Resto de botones. Análisis: XiaoZhi solo usa **BOOT (GPIO0)**: un clic alterna la conversación (o abre el modo Wi-Fi durante el arranque). **PWR («Key1»)** va al AXP2101 (XiaoZhi lo configura para apagar al mantener 4 s) y no es un GPIO que se pueda leer aquí. «Key2» une GPIO0 y CHIP_PU (arranque/reinicio): no se toca. Por eso solo se ha mapeado GPIO18; no hay mute ni otras funciones.
- Brújula: imposible, el QMI8658 no tiene magnetómetro.

## Sin verificar
Todo en la placa: fluidez, signo de los ejes del QMI8658 (ajustables con `CHIPO_LEVEL_MIRROR_X/Y`), memoria de la pizarra, acentos del tipo de letra de subtítulos, táctil y sonido. Tampoco se ha comprobado que GPIO18 esté libre en la placa real ni que el botón responda. Pasa 2 grupos de pruebas en PC (`tests/host/test_tools_logic.cpp`, `test_tools.cpp`); la parte ESP la valida CI.
