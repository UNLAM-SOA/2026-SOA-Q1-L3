/*
Estas dos funciones no van a funcionar desde vscode, las tenes que pegar si o si en el navegador
Ambas funciones apuntan a hacer un fetch al endpoint descargar_audio o ping respectivamente
Como detalle, para que funcione tenés que estar en la misma red que el esp32 porque no tiene una IP pública
*/

async function descargarAudioEsp32(ipEsp32) {
    try {
        console.log(`[HTTP] Conectando a http://${ipEsp32}/descargar_audio...`);
        
        // 1. Realizamos el GET request al endpoint del ESP32
        const respuesta = await fetch(`http://${ipEsp32}/descargar_audio`, {
            method: 'GET'
        });

        // Verificamos si el servidor respondió correctamente (Status 200)
        if (!respuesta.ok) {
            throw new Error(`El ESP32 respondio con error: ${respuesta.status}`);
        }

        // 2. TRUCO CLAVE: Consumimos la respuesta como un BLOB binario pura
        const blobAudio = await respuesta.blob();
        console.log(`[HTTP] Archivo recibido. Tamaño: ${blobAudio.size} bytes.`);

        // 3. Forzamos la descarga del archivo en el navegador del usuario
        // Creamos un puntero temporal en la memoria del navegador hacia ese bloque de bytes
        const urlMemoria = URL.createObjectURL(blobAudio);
        
        // Creamos un elemento de enlace <a> invisible, le hacemos clic y lo destruimos
        const enlaceDescarga = document.createElement('a');
        enlaceDescarga.href = urlMemoria;
        enlaceDescarga.download = 'alerta_audio_nono.wav'; // Nombre con el que se guardará en la PC
        document.body.appendChild(enlaceDescarga);
        enlaceDescarga.click();
        
        // Limpieza de recursos de memoria en el navegador
        document.body.removeChild(enlaceDescarga);
        URL.revokeObjectURL(urlMemoria);

        console.log("[Éxito] Descarga completada.");

    } catch (error) {
        console.error("[ERROR] No se pudo obtener el archivo de audio del dispositivo:", error);
    }
}

// Para ejecutarlo en la consola de tu navegador de pruebas, simplemente llamarías:
// descargarAudioEsp32("1192.168.1.50"); // Reemplaza por la IP real que te tire el serial del ESP32

async function probarPingEsp32(ipEsp32) {
    try {
        console.log(`[HTTP] Enviando ping a http://${ipEsp32}/ping...`);
        
        // Realizamos la petición GET al endpoint de prueba
        const respuesta = await fetch(`http://${ipEsp32}/ping`, {
            method: 'GET'
        });

        if (!respuesta.ok) {
            throw new Error(`El ESP32 respondió con código de error: ${respuesta.status}`);
        }

        // Como el endpoint devuelve texto plano ("text/plain"), lo leemos como texto
        const mensaje = await respuesta.text();
        
        console.log(`%c[ÉXITO] Respuesta recibida del ESP32: "${mensaje}"`, "color: green; font-weight: bold;");

    } catch (error) {
        console.error("%c[ERROR] El ping falló rotundamente:", "color: red; font-weight: bold;", error.message);
        console.log("Tip: Verifica que la IP sea correcta, que tu PC esté en la misma red que el ESP32 y que hayas iniciado el servidor HTTP en el código de la placa.");
    }
}

// Invoca la función pasando la IP que te muestre tu monitor serial
//probarPingEsp32("192.168.43.45");