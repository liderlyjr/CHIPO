# Compilar CHIPO OS usando GitHub (sin instalar programas)

Esta es la forma recomendada: **GitHub compila el firmware en sus servidores**. En tu PC solo hace falta un navegador moderno y, para grabar la placa, un cable USB de datos.

## 1. Abrir el repositorio

Entra en https://github.com/liderlyjr/CHIPO y comprueba que estás en la pestaña **Code**.

## 2. Abrir las compilaciones

1. Pulsa la pestaña **Actions** del repositorio.
2. Si GitHub muestra un aviso para habilitar GitHub Actions, pulsa el botón para habilitar los workflows.
3. En la columna izquierda, selecciona **Build CHIPO firmware**.

## 3. Lanzar la compilación

1. Pulsa **Run workflow**.
2. Deja seleccionada la rama **main**.
3. Pulsa el botón verde **Run workflow** para confirmar.

No tienes que escribir comandos ni instalar ESP-IDF. GitHub descargará la base XiaoZhi, aplicará la integración CHIPO y ejecutará la compilación con ESP-IDF 6.1.

## 4. Esperar el resultado

1. Actualiza la página **Actions** si hace falta.
2. Abre la ejecución más reciente.
3. Espera a que el trabajo **Compile CHIPO for Waveshare ESP32-S3 Touch AMOLED 2.16 (480x480)** termine.
4. Si aparece un círculo verde con una marca, la compilación terminó correctamente.
5. Si aparece una X roja, la compilación falló: no descargues ni grabes ningún archivo. Comparte una captura del error o el enlace de esa ejecución para corregirlo.

La primera compilación puede tardar varios minutos.

## 5. Descargar el firmware

Cuando la ejecución esté en verde:

1. Abre la ejecución completada.
2. Baja hasta **Artifacts**, al final de la página.
3. Descarga **CHIPO-OS-Waveshare-ESP32-S3-AMOLED-2.16**.
4. Windows descargará un ZIP.
5. Descomprime el ZIP en una carpeta fácil de encontrar, por ejemplo el Escritorio.

El ZIP contiene los binarios generados y los archivos de argumentos de grabación de ESP-IDF. No cambies los nombres ni el contenido de esos archivos.

## 6. Grabar con ESPTool-JS

Usaremos https://espressif.github.io/esptool-js/

Las direcciones están en el archivo `flash_args` del ZIP: usa siempre las de **tu** compilación. Con la tabla de
particiones actual de XiaoZhi (`partitions/v2/16m.csv`) son estas:

| Archivo | Dirección |
|---|---|
| `bootloader.bin` | `0x0` |
| `partition-table.bin` | `0x8000` |
| `ota_data_initial.bin` | `0xd000` |
| `xiaozhi.bin` | `0x20000` |
| `generated_assets.bin` | `0x800000` |

1. Conecta la Waveshare al PC con un cable USB que transmita datos.
2. En ESPTool-JS pulsa **Connect** y elige el puerto de la placa.
3. Añade los cinco archivos con sus direcciones (comprueba que coinciden con `flash_args`).
4. Pulsa **Program** y espera a que termine. Después desconecta y vuelve a conectar el USB.

Si la placa no entra en modo de grabación, mantén pulsado **BOOT** mientras la conectas.

Solo hace falta grabar así la primera vez: después CHIPO se actualiza solo desde las Releases (ver
[CHIPO_UPDATE.md](CHIPO_UPDATE.md)).

## Qué significa el resultado

- **Build verde + Artifact disponible:** GitHub ha generado los binarios.
- **Build rojo:** no hay firmware listo para grabar; primero hay que reparar el error.
- **Placa arrancando correctamente:** solo se confirma después de grabar y probar físicamente el dispositivo.

## Importante

Esta compilación es para **Waveshare ESP32-S3 Touch AMOLED 2.16**. No uses binarios de otra placa. La compilación todavía debe superar una ejecución real en GitHub y una prueba en la placa antes de considerar CHIPO OS validado.
