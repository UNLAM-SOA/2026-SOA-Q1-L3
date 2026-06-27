#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>


#include "GestorAlmacenamiento.h"
#include "GestorBoton.h"
#include "GestorDeRed.h"
#include "GestorInterfaz.h"
#include "GestorMicrofono.h"
#include "GestorMQTT.h"
#include "GestorHTTP.h"
#include "Definiciones.h"


// ==========================================
// CONFIGURACIÓN DE RED Y MQTT
// ==========================================
// Credenciales WiFi (Hotspot del celular)
const char *WIFI_SSID = "motorola edge 40_5723";
const char *WIFI_PASS = "SecretoNonofono8";

// Credenciales del Broker (HiveMQ Cloud)
const char *MQTT_BROKER = "e10a0f3769d14449b0472d6e60e344a9.s1.eu.hivemq.cloud";
const char *MQTT_USER = "admin_prueba";
const char *MQTT_PASS = "Nonofono8";

// ==========================================
// INSTANCIAS GLOBALES
// ==========================================

// Instanciamos nuestro gestor inyectando las constantes

#define ACTIVAR_RED
//Principalmente metemos este define para poder usar el simulador desactivando todas las funciones de mqtt


// MODO DEBUG
#define SERIAL_ENABLED 1
#if SERIAL_ENABLED
  #define SerialPrint(str) Serial.println(str)
#else
  #define SerialPrint(str)
#endif

#define MAX_EVENTOS 40
#define MAX_CONTACTOS 10
#define MAX_MENSAJES 3
#define CANT_PINES 40

// --- Variables de Control de Tiempo (Timeouts) ---
#define TIMEOUT_GENERAL (SERIAL_ENABLED == 1) ? 4000 : 30000 // 30 segundos
#define TIMEOUT_GRABACION                                                      \
  (SERIAL_ENABLED == 1) ? 6000 : 60000 // 60 segundos por mensaje
#define TIMEOUT_BUZZER 1000
#define BTN_EMERGENCIA_TIEMPO (SERIAL_ENABLED == 1) ? 2000 : 5000
#define DELAY_INIT 3000
#define DELAY_ENCONTRAR_DISPOSITIVO 5000
#define TIMEOUT_EXITO_FRACASO 4000
#define BITS_RESOLUCION 12
#define TAM_STACK_GET_EVENT 4096
#define TAM_STACK_FSM 8192
#define TAM_STACK_SD 4096
#define TAM_STACK_RED 8192


// Timers
#define BIT_TIMEOUT (1 << 0)
#define BIT_BUZZER_FIN (1 << 1)

// ==========================================
// DEFINICIÓN DE PINES (ESP32 30-Pines)
// ==========================================

// --- Pantalla LCRD (I2C) ---
const int LCD_SDA = 21;
const int LCD_SCL = 22;

// --- Tarjeta SD (SPI) ---
const int SD_CS = 5;
const int SD_SCK = 18;
const int SD_MISO = 19;
const int SD_MOSI = 23;

// --- Micrófono INMP441 (I2S) ---
const int MIC_SCK = 14; // BCLK
const int MIC_WS = 25;  // LRC / L/R Clock
const int MIC_SD = 32;  // DIN / Data

// --- Actuadores ---
const int BUZZER_PIN = 26;
const int LED_PIN = 27;
const int FREC_BUZZER = 2000;

// --- Sensor Analógico ---
const int POT_PIN = 33; // Potenciómetro (ADC1)

// --- Pulsadores ---
// Los pines 34, 35, 36 y 39 requieren resistencia física externa.
const int BTN_ARRIBA = 36;
const int BTN_ABAJO = 39;
const int BTN_CONFIRMAR = 34;
const int BTN_CANCELAR = 35;

// Estos pines sí tienen resistencia Pull-up interna en el ESP32.
const int BTN_GRABAR = 4;
const int BTN_EMERGENCIA = 13;

