# Compilar CHIPO OS en Windows

## Requisitos

- Windows 10/11 de 64 bits.
- Git for Windows.
- Espressif ESP-IDF **6.0.1 o superior**. La rama 6.1 es la recomendada por la versión actual de XiaoZhi.
- Python y CMake instalados por el instalador de ESP-IDF.
- Conexión a Internet durante la primera compilación para descargar XiaoZhi y sus componentes.

## Compilación automática

1. Descarga este repositorio CHIPO en tu PC (botón **Code → Download ZIP**) y descomprime la carpeta.
2. Abre **ESP-IDF 6.1 PowerShell** desde el menú Inicio de Espressif. No uses una consola normal sin el entorno ESP-IDF cargado.
3. Entra en la carpeta CHIPO que descomprimiste.
4. Ejecuta:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\scripts\build_chipo.ps1
   ```

El script descarga XiaoZhi oficial en `build/xiaozhi-esp32` (el commit fijado en `XIAOZHI_REF`, el mismo que usa GitHub Actions; otro con `-XiaoZhiRef <commit o rama>`), copia los componentes CHIPO, aplica los cambios de integración y ejecuta `idf.py build` para la **Waveshare ESP32-S3 Touch AMOLED 2.16** (480×480), la única placa soportada.

Si el script termina correctamente, los binarios quedan en:

`build/xiaozhi-esp32/build/`

El archivo habitual para flashear es `xiaozhi.bin` o el binario de aplicación que indique el resumen final de ESP-IDF. Para grabar firmware y particiones, utiliza el comando de flash que genere esa misma compilación; no flashees únicamente el binario de aplicación si la tabla de particiones o los assets requieren actualización.

## Importante

- El script exige que las anclas de integración coincidan exactamente. Si XiaoZhi cambia su estructura, se detiene en lugar de modificar archivos de forma aproximada.
- El primer build puede tardar bastante y descargar varios componentes.
- Esta integración inicial coloca la cara de CHIPO como una capa LVGL encima de la pantalla de XiaoZhi. Todavía no sustituye todos los paneles ni rediseña el flujo completo.
- No se ha ejecutado una compilación real en este entorno ni se ha probado en la placa física. El script automatiza el build local, pero el resultado solo debe considerarse válido si `idf.py build` termina con código 0.
- La 2,16" trae altavoz integrado (ver `docs/HARDWARE_2_16.md`).
