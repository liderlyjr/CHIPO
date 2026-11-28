
## 0.7.6

- Fluidez: la pantalla usa dos franjas DMA (dibuja una mientras se envía la otra). Si no cabe en RAM interna, vuelve sola a una franja (como XiaoZhi original). No medido en la placa.
- Menú y panel: cada app tiene un icono propio (80x80, solo alfa, teñido con el color de la app) generado con `scripts/gen_icons.py` en lugar del círculo de color.
- Ejes: bola de gravedad y giro lateral del horizonte con el signo comprobado en la placa (0.7.5).
