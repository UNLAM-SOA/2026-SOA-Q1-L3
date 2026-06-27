#include <Preferences.h>
#include <Arduino.h>
#include "Definiciones.h"


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
case EV_RECIBIR_CONTACTOS_1:
    return "EV_RECIBIR_CONTACTOS_1";
case EV_RECIBIR_CONTACTOS_2:
    return "EV_RECIBIR_CONTACTOS_2";

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
//Permite generar un ID único persistente, aunque se apague el ESP32. Se guarda en la memoria flash del ESP32 usando la librería Preferences.
long generarIdMensajePersistente() {
    Preferences preferencias;
    
    // El false significa que lo abrimos en modo Lectura/Escritura.
    preferencias.begin("nonofono", false); 

    // Busca la clave "id_msg". Si no existe (es la primera vez que se enciende), devuelve 0.
    long ultimoId = preferencias.getLong("id_msg", 0); 
    long nuevoId = ultimoId + 1; 
    // Guardamos el nuevo valor en la memoria flash
    preferencias.putLong("id_msg", nuevoId); 
    
    // Cerramos el espacio para liberar recursos
    preferencias.end(); 
    return nuevoId;
}