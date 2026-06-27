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

        // Manejador del Preflight CORS (Petición OPTIONS)
        servidor.on("/descargar_audio", HTTP_OPTIONS, [this]() {
            servidor.sendHeader("Access-Control-Allow-Origin", "*");
            servidor.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
            servidor.sendHeader("Access-Control-Allow-Headers", "ngrok-skip-browser-warning");
            servidor.send(204); // 204 = No Content (Respuesta exitosa vacía)
        });

        // Tu endpoint original queda igual
        servidor.on("/descargar_audio", HTTP_GET, [this]() {
            File archivoAudio = SD.open(rutaArchivoAudio, FILE_READ);
            if (!archivoAudio) {
                servidor.send(404, "text/plain", "Error: Archivo WAV no encontrado en la SD.");
                return;
            }
            servidor.sendHeader("Access-Control-Allow-Origin", "*");
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