# Hardware: Waveshare ESP32-S3-Touch-AMOLED-2.16 (referencia 33969)

Datos de la placa que usa CHIPO en la versión 2.16" (480×480). Cada dato dice de dónde sale. Lo que no está
verificado se indica como tal.

## Altavoz

- **Altavoz integrado: sí.** Waveshare Support respondió por correo (petición 265197, 20-nov-2026):
  *"Yes, it has a built-in speaker."* La pegatina de la cara trasera también lista **SPK**.
- La documentación oficial (docs.waveshare.com) solo menciona la reproducción por el códec ES8311 y el control del
  amplificador en `GPIO46` (`PA_CTRL`); **no** dice por escrito que haya altavoz. La confirmación es la del soporte.
- Decisión: **no se usa el altavoz externo Zuahaly**. No hay alojamiento, cableado ni amplificador extra.
- Sin verificar: tamaño, potencia, impedancia y posición del altavoz. El sonido real no se ha probado en la placa.

## Caja

Según el dibujo "Product Dimensions" de la documentación oficial (en mm), la placa se muestra dentro de una caja blanca:

| Dato | Valor |
|---|---|
| Frente de la caja | 46,00 × 46,00 |
| Grosor de la caja | 22,50 |
| Cristal | 43,30 × 43,30 |
| Zona activa de la pantalla | 38,99 × 38,99 |
| Radio de las esquinas | 5,80 |
| Botones (canto superior) | 3, Ø 5,30, separados 10,00 (+/KEY, PWR, BOOT/−) |
| Tornillos de la cara trasera | 4, separados 37,00 × 34,00 |
| USB-C | canto inferior |
| microSD | lateral izquierdo (según la pegatina; la lectura del dibujo es del propio usuario) |

La documentación en texto no menciona la caja ni el contenido del paquete: la caja se ve en el dibujo y la confirma
el usuario, que tiene la placa.

## Batería

- El usuario indica que la placa **ya trae batería** en la caja. No está confirmado en la documentación.
- Capacidad: **desconocida** (pendiente de leerla en la propia batería). No se promete autonomía sin medirla.
- La placa tiene un conector MX1.25 de 2 pines para LiPo de 3,7 V con carga y descarga (documentación oficial).
- El firmware lee porcentaje y estado de carga del AXP2101, sea cual sea la batería.
- Por tanto no hace falta la LiPo 803040 (30 × 40 × 8 mm) prevista antes para la placa de 1,8".

## Para la carcasa Macintosh (OpenSCAD)

Reservar un bloque de **46 × 46 × 22,5 mm** para la caja de la placa, con acceso a los tres botones del canto
superior, al USB-C del canto inferior, a la microSD del lateral y salida de sonido del altavoz de la placa. Las
posiciones exactas de las salidas de sonido y micrófonos no están verificadas.

## Energía: atenuar y apagarse solo

El código de XiaoZhi para esta placa baja el brillo al 20 % tras 60 s sin conversación y **apaga la placa** (AXP2101)
a los 5 min. CHIPO lo impide mientras tiene algo pendiente:

- un temporizador, pomodoro, cronómetro o alarma puesta;
- un recordatorio guardado o algo sonando;
- el reloj de mesa o el modo dormir;
- una pantalla abierta (clima, escenas, lámpara, herramientas) o el modo coche;
- la placa conectada a USB (cargando o sin descargar la batería).

Solo con batería y nada pendiente sigue el comportamiento de XiaoZhi (los minutos se cambian en la configuración web de
XiaoZhi: `sleep_dim_min`, `sleep_off_min`). Con una alarma puesta CHIPO no se apaga, así que gasta batería toda la
noche: para eso conviene dejarlo enchufado. El brillo nocturno lo gestiona el protector de pantalla de CHIPO.
**Sin probar en la placa** (ni el consumo ni si el AXP2101 apaga con el USB conectado).

## Documentación oficial de la placa

- Página de producto y pinout: https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.16
- Recursos y documentos: https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.16/Resources-And-Documents
- Esquema (PDF): https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.16/ESP32-S3-Touch-AMOLED-2.16-Schematic.pdf
- Ejemplos oficiales: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16/tree/main/examples
- ESP32-S3 datasheet: https://documentation.espressif.com/esp32-s3_datasheet_en.pdf
- ESP32-S3 manual técnico: https://documentation.espressif.com/esp32-s3_technical_reference_manual_en.pdf
- QMI8658 (IMU): https://files.waveshare.com/wiki/common/QMI8658C.pdf
- AXP2101 (energía): https://files.waveshare.com/wiki/common/X-power-AXP2101_SWcharge_V1.0.pdf
- ES8311 (códec): https://files.waveshare.com/wiki/common/ES8311.DS.pdf y guía de usuario https://files.waveshare.com/wiki/common/ES8311.user.Guide.pdf

No hay dibujos 2D/3D en la página de recursos.

## Pinout (según la documentación de Waveshare)

| GPIO | Función |
|---|---|
| 0 | BOOT / modo descarga |
| 1, 2, 3, 41 | Tarjeta SD (SPI; 41 = CS) |
| 4, 5, 6, 7 | Pantalla QSPI datos 0–3 |
| 38 | Pantalla QSPI reloj |
| 12 | Pantalla CS |
| 39 | Pantalla reset |
| 11 / 40 | Táctil INT / reset |
| 14 / 15 | I2C compartido SCL / SDA (táctil, AXP2101, QMI8658, PCF85063, ES8311/ES7210) |
| 8, 9, 10, 42, 45 | Audio I2S |
| 46 | Control del amplificador |
| 13 | RTC INT |
| 16 | SYS_OUT |
| 17 / 21 | IMU INT1 / INT2 |
| 18 | KEY3 (botón de usuario, pull-up, a GND al pulsar) |
| 19 / 20 | USB nativo (no usar como GPIO) |
| 43 / 44 | UART0 TX / RX |

Direcciones I2C: AXP2101 0x34 (datasheet), ES8311 0x18 o 0x19 (según CE), ES7210 0x40 (esquema).
Las de QMI8658, PCF85063 y CST9220 no están verificadas.
