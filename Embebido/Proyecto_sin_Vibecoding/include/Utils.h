#include "Definiciones.h"
#include "Config.h"
#include <Arduino.h>
#include <Preferences.h>

extern char payloadGlobal[512];
extern volatile bool hayMensajeNuevo;

extern Contacto listaContactos[];
String eventoToString(TipoEvento evento);
String estadoToString(EstadoFSM estado);

String eventoToString(TipoEvento evento) {
  switch (evento) {
  case EV_CONTINUE:
    return "EV_CONTINUE";
  case EV_BTN_ARRIBA:
    return "EV_BTN_ARRIBA";
  case EV_BTN_ABAJO:
    return "EV_BTN_ABAJO";
  case EV_BTN_CONFIRMAR:
    return "EV_BTN_CONFIRMAR";
  case EV_BTN_CANCELAR:
    return "EV_BTN_CANCELAR";
  case EV_BTN_GRABAR:
    return "EV_BTN_GRABAR";
  case EV_BUZZER_FIN:
    return "EV_BUZZER_FIN";
  case EV_BTN_EMERGENCIA:
    return "EV_BTN_EMERGENCIA";
  case EV_BTN_EMERGENCIA_PRESS:
    return "EV_BTN_EMERGENCIA_PRESS";
  case EV_TIMEOUT:
    return "EV_TIMEOUT";
  case EV_WIFI_EXITO:
    return "EV_WIFI_EXITO";
  case EV_WIFI_ERROR:
    return "EV_WIFI_ERROR";
  case EV_ENCONTRAR_DISPOSITIVO:
    return "EV_ENCONTRAR_DISPOSITIVO";
  case EV_RECIBIR_CONTACTOS:
    return "EV_RECIBIR_CONTACTOS";
  default:
    return "EV_DESCONOCIDO";
  }
}

String estadoToString(EstadoFSM estado) {
  switch (estado) {
  case INIT:
    return "INIT";
  case IDLE:
    return "IDLE";
  case NAVEGANDO:
    return "NAVEGANDO";
  case CONFIRMAR_CONTACTO:
    return "CONFIRMAR_CONTACTO";
  case CONFIRMAR_AUDIO:
    return "CONFIRMAR_AUDIO";
  case GRABANDO:
    return "GRABANDO";
  case INICIANDO_GRABACION:
    return "INICIANDO_GRABACION";
  case MENSAJE_PREDEFINIDO:
    return "MENSAJE_PREDEFINIDO";
  case EMERGENCIA:
    return "EMERGENCIA";
  case ESPERANDO_WIFI:
    return "ESPERANDO_WIFI";
  case MOSTRANDO_EXITO:
    return "MOSTRANDO_EXITO";
  case MOSTRANDO_ERROR:
    return "MOSTRANDO_ERROR";
  default:
    return "DESCONOCIDO";
  }
}
// Permite generar un ID único persistente, aunque se apague el ESP32. Se guarda
// en la memoria flash del ESP32 usando la librería Preferences.
long generarIdMensajePersistente() {
  Preferences preferencias;

  // El false significa que lo abrimos en modo Lectura/Escritura.
  preferencias.begin("nonofono", false);

  // Busca la clave "id_msg". Si no existe (es la primera vez que se enciende),
  // devuelve 0.
  long ultimoId = preferencias.getLong("id_msg", 0);
  long nuevoId = ultimoId + 1;
  // Guardamos el nuevo valor en la memoria flash
  preferencias.putLong("id_msg", nuevoId);

  // Cerramos el espacio para liberar recursos
  preferencias.end();
  return nuevoId;
}


String extraerValorJson(const String &json, const String &clave) {
  String claveBuscada = String("\"") + clave + String("\"");
  int indiceClave = json.indexOf(claveBuscada);
  if (indiceClave < 0) {
    indiceClave = json.indexOf(clave);
  }

  if (indiceClave < 0) {
    return String("");
  }

  int indiceDosPuntos = json.indexOf(':', indiceClave);
  if (indiceDosPuntos < 0) {
    return String("");
  }

  int inicio = indiceDosPuntos + 1;
  while (inicio < json.length() &&
         (json[inicio] == ' ' || json[inicio] == '\t' || json[inicio] == '\r' ||
          json[inicio] == '\n')) {
    inicio++;
  }

  if (inicio >= json.length()) {
    return String("");
  }

  if (json[inicio] == '"') {
    inicio++;
    int fin = json.indexOf('"', inicio);
    if (fin < 0) {
      fin = json.length();
    }
    return json.substring(inicio, fin);
  }

  int fin = inicio;
  while (fin < json.length() && json[fin] != ',' && json[fin] != '}') {
    fin++;
  }

  return json.substring(inicio, fin);
}

void cargarContactosDesdeMemoriaPersistente() {
  Preferences preferencias;
  preferencias.begin("nonofono", true);

  for (int i = 0; i < MAX_CONTACTOS; i++) {
    listaContactos[i] = {String(""), String("")};
  }

  uint8_t cantidad = preferencias.getUChar("contact_count", 0);
  for (uint8_t i = 0; i < cantidad && i < MAX_CONTACTOS; i++) {
    String nombre = preferencias.getString(
        ("contact_" + String(i) + "_name").c_str(), String(""));
    String telefono = preferencias.getString(
        ("contact_" + String(i) + "_phone").c_str(), String(""));

    listaContactos[i] = {nombre, telefono};
  }

  preferencias.end();
}

void guardarContactosEnMemoriaPersistente() {
  Preferences preferencias;
  preferencias.begin("nonofono", false);

  int cantidad = 0;
  for (int i = 0; i < MAX_CONTACTOS; i++) {
    if (listaContactos[i].nombre.length() > 0) {
      preferencias.putString(("contact_" + String(i) + "_name").c_str(),
                             listaContactos[i].nombre);
      preferencias.putString(("contact_" + String(i) + "_phone").c_str(),
                             listaContactos[i].telefono);
      cantidad++;
    } else {
      preferencias.remove(("contact_" + String(i) + "_name").c_str());
      preferencias.remove(("contact_" + String(i) + "_phone").c_str());
    }
  }

  preferencias.putUChar("contact_count", cantidad);
  preferencias.end();
}

void procesarContactosDesdeJson(const String &json) {
  for (int i = 0; i < MAX_CONTACTOS; i++) {
    listaContactos[i] = {String(""), String("")};
  }

  int indiceObjeto = json.indexOf('{');
  int contador = 0;

  while (indiceObjeto >= 0 && contador < MAX_CONTACTOS) {
    int indiceCierre = json.indexOf('}', indiceObjeto);
    if (indiceCierre < 0) {
      break;
    }

    String objeto = json.substring(indiceObjeto, indiceCierre + 1);
    String nombre = extraerValorJson(objeto, "name");
    String telefono = extraerValorJson(objeto, "phone");
    nombre.trim();
    telefono.trim();

    if (nombre.length() > 0) {
      listaContactos[contador] = {nombre, telefono};
      contador++;
    }

    indiceObjeto = json.indexOf('{', indiceCierre + 1);
  }

  guardarContactosEnMemoriaPersistente();
}

void consumirPayloadGlobal() {
  String mensaje = String(payloadGlobal);
  mensaje.trim();

  if (mensaje.length() == 0) {
    hayMensajeNuevo = false;
    return;
  }

  Serial.print("[MQTT] Procesando payload: ");
  Serial.println(mensaje);

  procesarContactosDesdeJson(mensaje);

  memset(payloadGlobal, 0, sizeof(payloadGlobal));
  hayMensajeNuevo = false;
}
