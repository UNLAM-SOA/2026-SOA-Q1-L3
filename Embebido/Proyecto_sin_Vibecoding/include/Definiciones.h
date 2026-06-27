#pragma once
#include <Arduino.h>
enum TipoEvento {
  EV_CONTINUE,
  EV_BTN_ARRIBA,
  EV_BTN_ABAJO,
  EV_BTN_CONFIRMAR,
  EV_BTN_CANCELAR,
  EV_BTN_GRABAR,
  EV_BUZZER_FIN,
  EV_BTN_EMERGENCIA,
  EV_BTN_EMERGENCIA_PRESS,
  EV_TIMEOUT,
  EV_WIFI_EXITO,
  EV_WIFI_ERROR,
  EV_ENCONTRAR_DISPOSITIVO,
  EV_RECIBIR_CONTACTOS,
};

struct Evento {
  TipoEvento tipo;
};



//----------------------------------------------
// ESTADOS
//----------------------------------------------
enum EstadoFSM {
  INIT,
  IDLE,
  NAVEGANDO,
  CONFIRMAR_CONTACTO,
  GRABANDO,
  INICIANDO_GRABACION,
  CONFIRMAR_AUDIO,
  MENSAJE_PREDEFINIDO,
  EMERGENCIA,
  ESPERANDO_WIFI,
  MOSTRANDO_EXITO,
  MOSTRANDO_ERROR
};

struct EventoFSM {
  TipoEvento tipo;
  char* payload; 
};