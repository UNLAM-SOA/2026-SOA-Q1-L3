#pragma once
#include "Utils.h"
#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

extern QueueHandle_t colaEventos;
#define MAX_REINTENTOS 10
#define TIEMPO_REINTENTO 5000

extern char payloadGlobal[512];
extern volatile bool hayMensajeNuevo;

void callbackMQTT(char *topic, byte *payload, unsigned int length);
int obtenerPaginaDesdeJson(const String &jsonPayload);
class GestorMQTT {
private:
  const char *ssid;
  const char *wifi_pass;
  const char *mqtt_broker;
  const char *mqtt_usuario;
  const char *mqtt_clave;
  const int mqtt_puerto = 8883; // Puerto seguro obligatorio

  WiFiClientSecure clienteSeguro;
  PubSubClient clienteMQTT;

  TickType_t ultimoIntentoMQTT = 0;
  const TickType_t INTERVALO_REINTENTO = pdMS_TO_TICKS(TIEMPO_REINTENTO);
  // Agregamos la variable para guardar la referencia a la cola
  QueueHandle_t colaEventos;

  String obtenerIdDispositivo() {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    return "nono_" + mac;
  }

public:
  // El constructor inyecta el cliente seguro dentro del cliente MQTT
  GestorMQTT(const char *red, const char *passRed, const char *broker,
             const char *user, const char *passBroker, QueueHandle_t cola)
      : clienteMQTT(clienteSeguro) {
    ssid = red;
    wifi_pass = passRed;
    mqtt_broker = broker;
    mqtt_usuario = user;
    mqtt_clave = passBroker;

    // 3. Guardamos la referencia para usarla después
    colaEventos = cola;
  }

  void iniciarWiFi() {
    int cont = 0;
    Serial.print("\n[Red] Conectando a WiFi: ");
    Serial.println(ssid);
    WiFi.begin(ssid, wifi_pass);

    while (WiFi.status() != WL_CONNECTED && cont < MAX_REINTENTOS) {
      // Usamos vTaskDelay sin miedo, el setup() corre en una tarea
      vTaskDelay(pdMS_TO_TICKS(500));
      Serial.print(".");
      cont++;
    }
    if (cont < MAX_REINTENTOS) {
      Serial.println("\n[Red] WiFi conectado con éxito.");
      // Omitimos la validación del certificado raíz del servidor
      Serial.print("[Red] Dirección IP del ESP32: ");
      Serial.println(WiFi.localIP());
      clienteSeguro.setInsecure();
      clienteMQTT.setServer(mqtt_broker, mqtt_puerto);
    } else {
      Serial.println("\n[Red] Hubo un error en la conexión al WiFi");
    }
  }

  bool conectarBroker() {
    if (clienteMQTT.connected())
      return true;
    clienteMQTT.setBufferSize(512);
    Serial.print("[MQTT] Conectando a HiveMQ Cloud...");
    String clientId = "ESP32_Nonofono_" + String(random(0xffff), HEX);

    if (clienteMQTT.connect(clientId.c_str(), mqtt_usuario, mqtt_clave)) {
      Serial.println(" ¡Conexión TLS establecida!");

      // --- LAS 5 SUSCRIPCIONES ---
      clienteMQTT.subscribe("nonofono/config/contactos");
      clienteMQTT.subscribe("nonofono/encontrar");
      clienteMQTT.subscribe("nonofono/mensajes/audio/ack");
      clienteMQTT.subscribe("nonofono/mensajes/predeterminado/ack");
      // ---------------------------

      Serial.println("[MQTT] Suscrito a todos los tópicos de operación.");
      return true;
    } else {
      Serial.print(" Falló. rc: ");
      Serial.print(clienteMQTT.state());
      Serial.println(" -> Reintentando...");
      return false;
    }
  }

  // Debe llamarse continuamente en un loop/tarea para mantener vivo el ping con
  // el servidor Usamos el tipo de dato nativo del RTOS
  void mantenerConexion() {
    if (WiFi.status() == WL_CONNECTED) {
      if (!clienteMQTT.connected()) {

        TickType_t tiempoActual = xTaskGetTickCount();

        // Lo hace cada 5 segundos porque conectarse al broker es una acción
        // costosa
        // y para que en caso de error le damos un respiro a la topología para
        // que conecte correctamente
        if (tiempoActual - ultimoIntentoMQTT >= INTERVALO_REINTENTO) {
          ultimoIntentoMQTT = tiempoActual;

          Serial.println("[MQTT] Intentando reconexión asíncrona...");
          conectarBroker();
        }
      } else {
        clienteMQTT.loop();
      }
    }
  }

  void publicarAlerta(const char *topic, const char *payload) {
    if (clienteMQTT.connected()) {
      // Validamos si la librería realmente aceptó enviar el paquete
      if (clienteMQTT.publish(topic, payload)) {
        Serial.println("[MQTT] Mensaje publicado con éxito.");
      } else {
        Serial.println(
            "[MQTT] ERROR CRÍTICO: PubSubClient rechazó el paquete.");
        Serial.print("Tamaño del payload intentado: ");
        Serial.println(strlen(payload));
      }
    } else {
      Serial.println(
          "[MQTT] Error: Imposible publicar, el broker está desconectado.");
    }
  }
  void configurarReceptor(MQTT_CALLBACK_SIGNATURE) {
    clienteMQTT.setCallback(callback);
  }

  String obtenerIpString() {
    return WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString()
                                         : String("0.0.0.0");
  }

  void notificarLive() {

    String contenido="{}";
    String payload =
        "{\"evento\": \"live\", \"contenido\": " + contenido + "}";
    publicarAlerta("nonofono/live", payload.c_str());
  }

  void notificarAudio(String destinatario, String endpoint, long tamanioBytes) {
    long idMensaje = generarIdMensajePersistente();
    String idDispositivo = obtenerIdDispositivo();
    String ip = obtenerIpString();

    String contenido = "{\"id_dispositivo\": \"" + idDispositivo + "\"" +
                       ", \"id_mensaje\": " + String(idMensaje) +
                       ", \"destinatario\": \"" + destinatario + "\"" +
                       ", \"endpoint\": \"" + endpoint + "\"" +
                       ", \"tamanio_bytes\": " + String(tamanioBytes) +
                       ", \"timestamp\": " + String(millis() / 1000) +
                       ", \"ip\": \"" + ip + "\"" + "}";

    String payload =
        "{\"evento\": \"audio\", \"contenido\": " + contenido + "}";
    publicarAlerta("nonofono/mensajes/audio", payload.c_str());
  }

  // 3. Enviar Mensaje Predeterminado
  void notificarMensajePredeterminado(String numeroTelefono, String contenido) {
    long idMensaje = generarIdMensajePersistente();
    String idDispositivo = obtenerIdDispositivo();
    String ip = obtenerIpString();

    String datos = "{\"id_dispositivo\": \"" + idDispositivo + "\"" +
                   ", \"id_mensaje\": " + String(idMensaje) +
                   ", \"telefono\": \"" + numeroTelefono + "\"" +
                   ", \"contenido\": \"" + contenido + "\"" + "}";

    String payload =
        "{\"evento\": \"mensaje-predefinido\", \"contenido\": " + datos + "}";
    publicarAlerta("nonofono/mensajes/predeterminado", payload.c_str());
  }

  void notificarEmergencia() {
    String idDispositivo = obtenerIdDispositivo();
    String ip = obtenerIpString();

    String contenido =
        "{\"mensaje\": \"¡Emergencia! El abuelo necesita ayuda.\"}";
    String payload =
        "{\"evento\": \"emergencia\", \"contenido\": " + contenido + "}";

    publicarAlerta("nonofono/emergencias", payload.c_str());
  }
};
// Parseo manual para obtener el número de página desde el JSON recibido.
// Retorna -1 si no se encuentra.
int obtenerPaginaDesdeJson(const String &jsonPayload) {
  int pageIndex = jsonPayload.indexOf("\"page\"");
  if (pageIndex < 0) {
    return -1;
  }

  pageIndex = jsonPayload.indexOf(':', pageIndex);
  if (pageIndex < 0) {
    return -1;
  }

  pageIndex++;
  while (pageIndex < jsonPayload.length() && isSpace(jsonPayload[pageIndex])) {
    pageIndex++;
  }

  if (pageIndex >= jsonPayload.length()) {
    return -1;
  }

  if (jsonPayload[pageIndex] == '"') {
    pageIndex++;
  }

  int startIndex = pageIndex;
  while (pageIndex < jsonPayload.length() && isDigit(jsonPayload[pageIndex])) {
    pageIndex++;
  }

  if (pageIndex == startIndex) {
    return -1;
  }

  return jsonPayload.substring(startIndex, pageIndex).toInt();
}

// Función global de recepción de MQTT
void callbackMQTT(char *topic, byte *payload, unsigned int length) {
  Evento eventoAEnviar;
  eventoAEnviar.tipo = EV_CONTINUE; // Inicializamos con un valor neutro
  // 1. Convertimos el payload binario a un String de C++ limpio
  String mensaje = "";
  for (unsigned int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }

  Serial.print("[MQTT] Mensaje recibido en [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(mensaje);

  // 2. Enrutamos según el tópico
  String topico = String(topic);
  // TODO priorizar emergencias
  if (topico == "nonofono/config/contactos") {
    Serial.println("[Router] Actualizando libreta de contactos...");
    unsigned int tamCopia = (length < 511) ? length : 511;
    memcpy(payloadGlobal, payload, tamCopia);
    payloadGlobal[tamCopia] = '\0';
    hayMensajeNuevo = true;
    eventoAEnviar.tipo = EV_RECIBIR_CONTACTOS;

  } else if (topico == "nonofono/encontrar") {
    // Lógica para hacer sonar el buzzer
    // Ej: inyectar evento EV_ACTIVAR_BUZZER en la cola de FreeRTOS
    Serial.println("[Router] Orden de localizar dispositivo recibida.");
    eventoAEnviar.tipo = EV_ENCONTRAR_DISPOSITIVO;

  } else if (topico == "nonofono/mensajes/audio/ack") {
    // Lógica para saber si el audio llegó bien o hubo error
    if (mensaje.indexOf("exito") >= 0) {
      Serial.println("[Router] El celular descargó el audio correctamente.");
      // Ej: inyectar EV_WIFI_EXITO
      eventoAEnviar.tipo = EV_WIFI_EXITO;

    } else {
      Serial.println("[Router] El celular reportó un error con el audio.");
      // Ej: inyectar EV_WIFI_ERROR
      eventoAEnviar.tipo = EV_WIFI_ERROR;
    }

  } else if (topico == "nonofono/mensajes/predeterminado/ack") {
    // Lógica para confirmar el mensaje de texto
    if (mensaje.indexOf("exito") >= 0) {
      Serial.println("[Router] El celular envió el mensaje predeterminado.");
      eventoAEnviar.tipo = EV_WIFI_EXITO;

    } else {
      Serial.println("[Router] Fallo al enviar mensaje predeterminado.");
      eventoAEnviar.tipo = EV_WIFI_ERROR;
    }
  }
  if (eventoAEnviar.tipo != EV_CONTINUE) {
    if (colaEventos != NULL) {
      // Enviamos el evento a la cola de FreeRTOS
      if (xQueueSend(colaEventos, &eventoAEnviar, pdMS_TO_TICKS(100)) !=
          pdPASS) {
        Serial.println(
            "[Router] Error: No se pudo enviar el evento a la cola.");
      } else {
        Serial.println("[Router] Evento enviado a la cola con exito.");
      }
    }
  }
}
