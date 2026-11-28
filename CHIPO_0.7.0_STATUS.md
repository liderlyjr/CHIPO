
## 0.7.6

- Fluidez: la pantalla usa dos franjas DMA (dibuja una mientras se envía la otra). Si no cabe en RAM interna, vuelve sola a una franja (como XiaoZhi original). No medido en la placa.
- Menú y panel: cada app tiene un icono propio (80x80, solo alfa, teñido con el color de la app) generado con `scripts/gen_icons.py` en lugar del círculo de color.
- Ejes: bola de gravedad y giro lateral del horizonte con el signo comprobado en la placa (0.7.5).

## 0.7.7

- Panel de modos: el paso de página ya no depende del gesto de LVGL (no funcionaba en el panel); se mide el desplazamiento del dedo. Los puntitos también cambian de página al tocarlos. Cuadrícula más pequeña para que no tape los puntos.
- Bola de gravedad: izquierda/derecha invertido (arriba/abajo ya estaba bien).
- Pizarra: una pulsación que empieza poco después de soltar y cerca del último punto continúa el trazo (antes salían puntos sueltos). Causa probable: el controlador táctil entrega el dedo en movimiento como soltar+pulsar. No comprobado en la placa.
