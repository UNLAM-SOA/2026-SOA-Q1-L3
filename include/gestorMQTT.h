#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

class GestorMQTT {
private:
    const char* ssid;
    const char* wifi_pass;
    const char* mqtt_broker;
    const char* mqtt_usuario;
    const char* mqtt_clave;
    const int mqtt_puerto = 8883; // Puerto seguro obligatorio

    WiFiClientSecure clienteSeguro;
    PubSubClient clienteMQTT;

public:
    // El constructor inyecta el cliente seguro dentro del cliente MQTT
    GestorMQTT(const char* red, const char* passRed, const char* broker, const char* user, const char* passBroker)
        : clienteMQTT(clienteSeguro) 
    {
        ssid = red;
        wifi_pass = passRed;
        mqtt_broker = broker;
        mqtt_usuario = user;
        mqtt_clave = passBroker;
    }

    void iniciarWiFi() {
        Serial.print("\n[Red] Conectando a WiFi: ");
        Serial.println(ssid);
        WiFi.begin(ssid, wifi_pass);

        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }
        Serial.println("\n[Red] WiFi conectado con éxito.");

        // TRUCO VITAL: Omitimos la validación del certificado raíz del servidor
        // para no agotar la RAM del ESP32 guardando certificados largos.
        clienteSeguro.setInsecure(); 
        
        clienteMQTT.setServer(mqtt_broker, mqtt_puerto);
    }

    bool conectarBroker() {
        if (clienteMQTT.connected()) return true;

        Serial.print("[MQTT] Conectando a HiveMQ Cloud...");
        String clientId = "ESP32_Nonofono_" + String(random(0xffff), HEX);

        if (clienteMQTT.connect(clientId.c_str(), mqtt_usuario, mqtt_clave)) {
            Serial.println(" ¡Conexión TLS establecida!");
            
            // 2. Nos suscribimos al canal de configuración apenas conecta
            clienteMQTT.subscribe("nonofono/config/contactos");
            Serial.println("[MQTT] Suscrito a nonofono/config/contactos");
            
            return true;
        } else {
            Serial.print(" Falló. Código de error (rc): ");
            Serial.print(clienteMQTT.state());
            Serial.println(" -> Reintentando en el próximo ciclo.");
            return false;
        }
    }

    // Debe llamarse continuamente en un loop/tarea para mantener vivo el ping con el servidor
    void mantenerConexion() {
        if (WiFi.status() == WL_CONNECTED) {
            if (!clienteMQTT.connected()) {
                conectarBroker();
            }
            clienteMQTT.loop();
        }
    }

    void publicarAlerta(const char* topic, const char* payload) {
        if (clienteMQTT.connected()) {
            clienteMQTT.publish(topic, payload);
            Serial.println("[MQTT] Mensaje publicado con éxito.");
        } else {
            Serial.println("[MQTT] Error: Imposible publicar, el broker está desconectado.");
        }
    }
    void configurarReceptor(MQTT_CALLBACK_SIGNATURE) {
        clienteMQTT.setCallback(callback);
    }
};