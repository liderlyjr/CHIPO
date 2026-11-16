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
3. Espera a que el trabajo **Compile CHIPO for Waveshare** termine.
4. Si aparece un círculo verde con una marca, la compilación terminó correctamente.
5. Si aparece una X roja, la compilación falló: no descargues ni grabes ningún archivo. Comparte una captura del error o el enlace de esa ejecución para corregirlo.

La primera compilación puede tardar varios minutos.

## 5. Descargar el firmware

Cuando la ejecución esté en verde:

1. Abre la ejecución completada.
2. Baja hasta **Artifacts**, al final de la página.
3. Descarga **CHIPO-OS-Waveshare-ESP32-S3-AMOLED-1.8-V2**.
4. Windows descargará un ZIP.
5. Descomprime el ZIP en una carpeta fácil de encontrar, por ejemplo el Escritorio.

El ZIP contiene los binarios generados y los archivos de argumentos de grabación de ESP-IDF. No cambies los nombres ni el contenido de esos archivos.

## 6. Grabar con ESPTool-JS

Usaremos https://espressif.github.io/esptool-js/

Antes de grabar, hay que comprobar qué archivos BIN produjo la compilación y sus direcciones de memoria. No elijas direcciones al azar ni grabes solo un BIN sin revisar el mapa de grabación.

Conecta la Waveshare al PC mediante un cable USB que transmita datos. En la página seleccionaremos el puerto serie y prepararemos la grabación usando los archivos y offsets exactos que generó la compilación. Haremos esta parte juntos cuando tengas el ZIP.

## Qué significa el resultado

- **Build verde + Artifact disponible:** GitHub ha generado los binarios.
- **Build rojo:** no hay firmware listo para grabar; primero hay que reparar el error.
- **Placa arrancando correctamente:** solo se confirma después de grabar y probar físicamente el dispositivo.

## Importante

Esta compilación es para **Waveshare ESP32-S3 Touch AMOLED 1.8 V2**. No uses binarios de otra revisión de placa. La compilación todavía debe superar una ejecución real en GitHub y una prueba en la placa antes de considerar CHIPO OS validado.
