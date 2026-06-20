#ifndef GESTOR_HTTP_H
#define GESTOR_HTTP_H

#include <Arduino.h>
#include <WebServer.h>
#include <FS.h>
#include <SD.h>

class GestorHTTP {
private:
    WebServer servidor;
    String rutaArchivoAudio;

    // Configuración interna de los Endpoints
    void configurarRutas() {
        
        // Endpoint de diagnóstico (Ping)
        servidor.on("/ping", HTTP_GET, [this]() {
            servidor.sendHeader("Access-Control-Allow-Origin", "*");
            servidor.send(200, "text/plain", "El ESP32 esta vivo y escuchando HTTP.");
        });

        // Endpoint para la transferencia del archivo físico desde la SD
        servidor.on("/descargar_audio", HTTP_GET, [this]() {
            
            // Abrimos el archivo utilizando el objeto global SD
            File archivoAudio = SD.open(rutaArchivoAudio, FILE_READ);
            if (!archivoAudio) {
                servidor.send(404, "text/plain", "Error: Archivo WAV no encontrado en la SD.");
                return;
            }
            //Permite que cualquier IP pueda descargar el audio, para mas info busca CORS
            servidor.sendHeader("Access-Control-Allow-Origin", "*");

            // streamFile transmite el archivo en bloques optimizados para el heap del ESP32
            servidor.streamFile(archivoAudio, "audio/wav");
            
            archivoAudio.close();
        });
    }

public:
    GestorHTTP(int puerto = 80, String rutaWav = "/archivoAudio.wav") 
        : servidor(puerto), rutaArchivoAudio(rutaWav) {}

    void iniciar() {
        configurarRutas();
        servidor.begin();
        Serial.println("[HTTP] Servidor Web inicializado correctamente.");
    }

    void atenderClientes() {
        servidor.handleClient();
    }
};

#endif // GESTOR_HTTP_H