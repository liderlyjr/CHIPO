# CHIPO: idea de lector NFC (solo propuesta)

**Estado: idea. No hay código NFC en el firmware y nada se ha probado en la placa.**

Placa: Waveshare ESP32-S3-Touch-AMOLED-2.16 (480×480, CO5300, CST9220).

## Conclusión

La placa no tiene ningún GPIO libre, pero un lector NFC por **I2C** puede compartir el bus
que ya existe, así que no gasta ningún pin.

## Pines (documentación oficial de Waveshare)

Todos los GPIO de 0 a 18, 21 y 38 a 46 están asignados (pantalla, táctil, audio, SD, IMU, RTC,
botones). GPIO19/20 son el USB nativo y GPIO43/44 el UART0.

- Bus I2C compartido: **GPIO14 (SCL) / GPIO15 (SDA)**, con táctil, AXP2101, QMI8658, PCF85063 y ES8311/ES7210.
- Waveshare avisa de vigilar conflictos de dirección y niveles al añadir dispositivos I2C externos.
- En el esquema hay dos resistencias de 2,2 kΩ junto a SCL/SDA (probablemente pull-ups a 3V3).

## Direcciones I2C (7 bits)

| Chip | Dirección | Estado |
|---|---|---|
| AXP2101 | 0x34 | verificada en el datasheet |
| ES8311 | 0x18 / 0x19 | según patilla CE; strap sin confirmar |
| ES7210 | 0x40 | etiqueta del esquema |
| QMI8658 | 0x6A / 0x6B | de memoria, sin verificar |
| PCF85063 | 0x51 | de memoria, sin verificar |
| CST9220 | ? | sin verificar |

Un PN532 en I2C usa 0x24, que no coincide con ninguna de las anteriores. Para confirmarlo
hay que ejecutar un escáner I2C en la placa.

## Alternativa

UART0 (GPIO43/44): funciona, pero se pierden los logs por serie mientras esté conectado.
El modo SPI no es viable por falta de pines.

## Pendiente de verificar

- Qué pads de la placa exponen I2C y dónde están (la documentación no da su posición).
- Escáner I2C en la placa para confirmar direcciones reales.
- Módulo NFC concreto a usar y su dirección.
- Hueco físico en la carcasa y posición de la antena (bobina) cerca de la zona de lectura.

## Plan si se implementa

1. Soldar 3V3, GND, SDA y SCL al bus I2C compartido.
2. Añadir un componente NFC por I2C (driver del módulo elegido).
3. Integrarlo con los estados de la cara (por ejemplo, reaccionar al leer una etiqueta).

Fuentes: documentación de la placa (docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.16), esquema en PDF
y datasheets enlazados en su página de recursos.
