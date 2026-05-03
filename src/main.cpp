#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
// #include <driver/i2s.h> // Se usará en el futuro para audio

#include "GestorInterfaz.h"
#include "GestorBoton.h"
#include "GestorDeRed.h"
#include "GestorMicrofono.h"

#define SERIAL_ENABLED 1
#if SERIAL_ENABLED
#define SerialPrint(str) Serial.println(str)
#else
#define SerialPrint(str)
#endif



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
const int MIC_WS = 15;  // LRC / L/R Clock
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

unsigned long ultimoTiempoRebote = 0;
const unsigned long TIEMPO_DEBOUNCE = 200;

// ==========================================
// DEFINICIÓN DE LA MÁQUINA DE ESTADOS
// ==========================================

//----------------------------------------------
// EVENTOS
//----------------------------------------------
enum TipoEvento
{
    EV_CONTINUE,
    EV_BTN_ARRIBA,
    EV_BTN_ABAJO,
    EV_BTN_CONFIRMAR,
    EV_BTN_CANCELAR,
    EV_BTN_GRABAR,
    EV_BTN_EMERGENCIA,
    EV_TIMEOUT,
    EV_WIFI_EXITO,
    EV_WIFI_ERROR
};



struct Evento
{
    TipoEvento tipo;
};

Evento evento;

//----------------------------------------------
// ESTADOS
//----------------------------------------------

enum EstadoFSM
{
    INIT,
    IDLE,
    NAVEGANDO,
    CONFIRMAR_CONTACTO,
    GRABANDO,
    CONFIRMAR_AUDIO,
    MENSAJE_PREDEFINIDO,
    EMERGENCIA,
    ESPERANDO_WIFI,
    MOSTRANDO_EXITO,
    MOSTRANDO_ERROR
};

void taskEvento(void *pvParameters);
void taskFSM(void *pvParameters);
String eventoToString(TipoEvento evento);
String estadoToString(EstadoFSM estado);

// ==========================================
// VARIABLES GLOBALES Y MOCKUPS
// ==========================================

EstadoFSM estadoActual = INIT;
EstadoFSM estadoAnterior = INIT;

// --- Variables de Control de Tiempo (Timeouts) ---
unsigned long tiempoUltimaAccion = 0;
unsigned long tiempoInicioGrabacion = 0;
unsigned long tiempoInicioEmergencia = 0;

const unsigned long TIMEOUT_GENERAL = 30000;   // 30 segundos
const unsigned long TIMEOUT_GRABACION = 60000; // 60 segundos por mensaje

// --- Variables de Datos (Mockups) ---
String listaContactos[10] = {
    "Hijo - Lucas", "Dra. Garcia", "Emergencias", "Vecino Juan",
    "Farmacia", "Hija - Maria", "Sobrino Alex", "Cuidado 24hs",
    "Bomberos", "Taxi Confianza"};
String listaMensajes[3] = {"Llamame", "Todo bien", "Ven a casa"};

int indiceContacto = 0;
int indiceActual = 0;
int indiceMensajeActual = 0;
int indiceMensaje = 0;
bool mensajeCorrecto = true;
bool esperandoLiberacionInicial = true;
GestorDeRed::EstadoRespuesta respuestaMensaje;

// ==========================================
// INSTANCIACIÓN DE GESTORES
// ==========================================
const unsigned long BTN_GRABAR_TIEMPO = 1000;
const unsigned long BTN_EMERGENCIA_TIEMPO = 2000;
const unsigned long BTN_CANCELAR_TIEMPO = 2000;
const unsigned long DELAY_INIT = 3000;
const unsigned long TIMEOUT_EXITO_FRACASO = 4000;

// Botón de grabar en el pin 4 (1 segundo)
GestorBoton gestorBotonGrabar(BTN_GRABAR, BTN_GRABAR_TIEMPO);
// Botón de emergencia en el pin 5 (5 segundos para evitar falsas alarmas, puesto en 2 segundos para testing)
GestorBoton gestorBotonEmergencia(BTN_EMERGENCIA, BTN_EMERGENCIA_TIEMPO);
GestorBoton gestorBotonCancelar(BTN_CANCELAR, BTN_CANCELAR_TIEMPO);

GestorDeRed gestorRed;
GestorDeMicrofono gestorAudio(BUZZER_PIN, LED_PIN, POT_PIN, FREC_BUZZER, MIC_SCK, MIC_WS, MIC_SD);

QueueHandle_t colaEventos;
bool leerBotonUnClic(int pin)
{
    static bool estadoAnterior[40]; // uno por pin

    bool estadoActual = digitalRead(pin);

    if (estadoAnterior[pin] == HIGH && estadoActual == LOW)
    {
        estadoAnterior[pin] = estadoActual;
        return true; // CLICK detectado
    }

    estadoAnterior[pin] = estadoActual;
    return false;
}

// ==========================================
// INICIALIZACIÓN (VOID SETUP)
// ==========================================

void setup()
{
    Serial.begin(115200);
    Serial.println("Iniciando Sistema de Gerontotecnología...");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    pinMode(BTN_ARRIBA, INPUT);
    pinMode(BTN_ABAJO, INPUT);
    pinMode(BTN_CONFIRMAR, INPUT);
    pinMode(BTN_CANCELAR, INPUT);

    pinMode(BTN_GRABAR, INPUT_PULLUP);
    pinMode(BTN_EMERGENCIA, INPUT_PULLUP);

    // Resolucion 12 bits, 0 a 4096
    analogReadResolution(12);

    // Inicialización del Bus I2C (Pantalla LCD)
    Wire.begin(LCD_SDA, LCD_SCL);

    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS))
    {
        Serial.println("ERROR: No se detectó Tarjeta SD o falló el montaje.");
    }
    else
    {
        Serial.println("Tarjeta SD montada correctamente.");
    }

    // TODO Inicialización del Micrófono (I2S)

    Serial.println("Setup completado. Iniciando FSM...");
    gestorUI.iniciar();

    // Creamos una cola capaz de almacenar hasta 10 eventos
    colaEventos = xQueueCreate(10, sizeof(TipoEvento));

    if (colaEventos != NULL)
    {

        xTaskCreate(taskEvento, "GeneradorEventos", 2048, NULL, 1, NULL);
        xTaskCreate(taskFSM, "MaquinaEstados", 4096, NULL, 2, NULL);
        Serial.println("Tareas creadas");
    }
}

// ==========================================
// GENERADOR DE EVENTOS
// ==========================================

void taskEvento(void *pvParameters)
{
    while (1)
    {
        evento.tipo = EV_CONTINUE;

        if (gestorBotonEmergencia.estaMantenido()) {
            evento.tipo = EV_BTN_EMERGENCIA;
        }
        else if (leerBotonUnClic(BTN_ARRIBA)) {
            evento.tipo = EV_BTN_ARRIBA;
        }
        else if (leerBotonUnClic(BTN_ABAJO)) {
            evento.tipo = EV_BTN_ABAJO;
        }
        else if (leerBotonUnClic(BTN_CONFIRMAR)) {
            evento.tipo = EV_BTN_CONFIRMAR;
        }
        else if (leerBotonUnClic(BTN_CANCELAR)) {
            evento.tipo = EV_BTN_CANCELAR;
        }
        else if (leerBotonUnClic(BTN_GRABAR)) {
            evento.tipo = EV_BTN_GRABAR;
        }
        // Eventos internos (WiFi)
        else if (estadoActual == ESPERANDO_WIFI) {
            auto r = gestorRed.actualizar();
            if (r == GestorDeRed::EXITO) {
                evento.tipo = EV_WIFI_EXITO;
            }
            else if (r == GestorDeRed::ERROR) {
                evento.tipo = EV_WIFI_ERROR;
            }
        }
        
            xQueueSend(colaEventos, &evento.tipo, portMAX_DELAY);
            
            #if SERIAL_ENABLED 
                SerialPrint("Evento detectado y encolado: " + eventoToString(evento.tipo));
            #endif
        

        // Verifica cada 50ms, de este modo reducimos al maximo el uso del CPU
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}

// ==========================================
// LOOP PRINCIPAL (FSM)
// ==========================================

String eventoToString(TipoEvento evento) 
{
    switch (evento) 
    {
        case EV_CONTINUE:       return "EV_CONTINUE";
        case EV_BTN_ARRIBA:     return "EV_BTN_ARRIBA";
        case EV_BTN_ABAJO:      return "EV_BTN_ABAJO";
        case EV_BTN_CONFIRMAR:  return "EV_BTN_CONFIRMAR";
        case EV_BTN_CANCELAR:   return "EV_BTN_CANCELAR";
        case EV_BTN_GRABAR:     return "EV_BTN_GRABAR";
        case EV_BTN_EMERGENCIA: return "EV_BTN_EMERGENCIA";
        case EV_TIMEOUT:        return "EV_TIMEOUT";
        case EV_WIFI_EXITO:     return "EV_WIFI_EXITO";
        case EV_WIFI_ERROR:     return "EV_WIFI_ERROR";
        default:                return "EV_DESCONOCIDO";
    }
}


String estadoToString(EstadoFSM estado)
{
    switch (estado)
    {
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

void loop()
{
    vTaskDelete(NULL);
}

void taskFSM(void *pvParameters)
{
    TipoEvento eventoRecibido;
    while (1)
    {
        if (xQueueReceive(colaEventos, &eventoRecibido, portMAX_DELAY) == pdPASS)
        {
            unsigned long tiempoActual = millis();
            evento.tipo=eventoRecibido;
            bool cambioEstado = (estadoActual != estadoAnterior);
            estadoAnterior = estadoActual;
            if (SERIAL_ENABLED && cambioEstado)
            {
                SerialPrint("-------------------------------------------------------\n");
                SerialPrint("Estado actual: " + estadoToString(estadoActual) + "\n");
                SerialPrint("-------------------------------------------------------\n");
            }
            switch (estadoActual)
            {

            //------------------------------------------
            case INIT:
            {
                switch (evento.tipo)
                {

                case EV_CONTINUE:
                    gestorUI.mostrarPantallaInit();
                    delay(DELAY_INIT);
                    estadoActual = IDLE;
                    break;
                }
            }
            break;

            //------------------------------------------
            case IDLE:
            {
                switch (evento.tipo)
                {

                case EV_CONTINUE:
                    if (cambioEstado)
                    {
                        gestorUI.mostrarPantallaReposo();
                        indiceContacto = 0;
                    }
                    break;

                case EV_BTN_ARRIBA:
                case EV_BTN_ABAJO:
                    tiempoUltimaAccion = tiempoActual;
                    estadoActual = NAVEGANDO;
                    break;

                case EV_BTN_EMERGENCIA:
                    estadoActual = EMERGENCIA;
                    break;
                }
            }
            break;

            //------------------------------------------
            case NAVEGANDO:
            {
                switch (evento.tipo)
                {

                case EV_CONTINUE:
                    if (cambioEstado || indiceActual != indiceContacto)
                        gestorUI.mostrarNavegandoContactos(listaContactos, indiceContacto);
                    indiceActual = indiceContacto;
                    break;

                case EV_BTN_ABAJO:
                    indiceContacto = min(indiceContacto + 1, 9);
                    tiempoUltimaAccion = tiempoActual;
                    break;

                case EV_BTN_ARRIBA:
                    indiceContacto = max(indiceContacto - 1, 0);
                    tiempoUltimaAccion = tiempoActual;
                    break;

                case EV_BTN_CONFIRMAR:
                    tiempoUltimaAccion = tiempoActual;
                    estadoActual = CONFIRMAR_CONTACTO;
                    break;

                case EV_BTN_EMERGENCIA:
                    estadoActual = EMERGENCIA;
                    break;
                }

                if (tiempoActual - tiempoUltimaAccion >= TIMEOUT_GENERAL)
                    estadoActual = IDLE;
            }
            break;

            //------------------------------------------
            case CONFIRMAR_CONTACTO:
            {
                switch (evento.tipo)
                {

                case EV_CONTINUE:
                    if (cambioEstado)
                        gestorUI.mostrarConfirmarContacto(listaContactos[indiceContacto]);
                    break;

                case EV_BTN_CANCELAR:
                    estadoActual = NAVEGANDO;
                    break;

                case EV_BTN_CONFIRMAR:
                    tiempoUltimaAccion = tiempoActual;
                    estadoActual = MENSAJE_PREDEFINIDO;
                    break;

                case EV_BTN_GRABAR:
                    gestorAudio.confirmarInicioGrabacion();
                    if (gestorBotonGrabar.estaMantenido())
                    {
                        tiempoInicioGrabacion = tiempoActual;
                        gestorAudio.iniciarGrabacion();
                        estadoActual = GRABANDO;
                    }
                    break;

                case EV_BTN_EMERGENCIA:
                    estadoActual = EMERGENCIA;
                    break;
                }
            }
            break;

            //------------------------------------------
            case GRABANDO:
            {
                switch (evento.tipo)
                {

                case EV_CONTINUE:
                    if (cambioEstado)
                        gestorUI.mostrarGrabando(listaContactos[indiceContacto]);
                    break;

                case EV_BTN_GRABAR:
                    gestorAudio.detenerGrabacion();
                    // gestorRed.iniciarEnvioMensaje();
                    estadoActual = CONFIRMAR_AUDIO;
                    break;

                case EV_BTN_EMERGENCIA:
                    gestorAudio.detenerGrabacion();

                    estadoActual = EMERGENCIA;
                    break;
                }
            }
            break;

            case MENSAJE_PREDEFINIDO:
            {
                switch (evento.tipo)
                {
                case EV_CONTINUE:
                    if (cambioEstado || indiceMensaje != indiceMensajeActual)
                        gestorUI.mostrarMensajesPredefinidos(listaContactos[indiceContacto], listaMensajes, indiceMensaje);
                    indiceMensajeActual = indiceMensaje;
                    break;

                case EV_BTN_ABAJO:
                    indiceMensaje = (indiceMensaje == 2) ? 2 : indiceMensaje + 1;
                    tiempoUltimaAccion = tiempoActual;
                    break;

                case EV_BTN_ARRIBA:
                    indiceMensaje = (indiceMensaje == 0) ? 0 : indiceMensaje - 1;
                    tiempoUltimaAccion = tiempoActual;
                    break;

                case EV_BTN_CONFIRMAR:
                    tiempoUltimaAccion = tiempoActual;
                    estadoActual = ESPERANDO_WIFI;
                    break;

                case EV_BTN_CANCELAR:
                    tiempoUltimaAccion = tiempoActual;
                    estadoActual = CONFIRMAR_CONTACTO;
                    break;

                case EV_BTN_EMERGENCIA:
                    estadoActual = EMERGENCIA;
                    break;
                }
            }
            break;

            //------------------------------------------
            case CONFIRMAR_AUDIO:
            {
                switch (evento.tipo)
                {

                case EV_CONTINUE:
                    if (cambioEstado)
                        gestorUI.mostrarConfirmarAudio();
                    break;

                case EV_BTN_CONFIRMAR:
                    estadoActual = ESPERANDO_WIFI;
                    break;

                case EV_BTN_CANCELAR:
                    estadoActual = CONFIRMAR_CONTACTO;
                    break;

                case EV_BTN_EMERGENCIA:
                    estadoActual = EMERGENCIA;
                    break;
                }
            }
            break;

            //------------------------------------------
            case ESPERANDO_WIFI:
            {
                switch (evento.tipo)
                {

                case EV_CONTINUE:
                    tiempoUltimaAccion = tiempoActual;
                    estadoActual = MOSTRANDO_EXITO;
                    break;

                case EV_WIFI_EXITO:
                    tiempoUltimaAccion = tiempoActual;
                    estadoActual = MOSTRANDO_EXITO;
                    break;

                case EV_WIFI_ERROR:
                    tiempoUltimaAccion = tiempoActual;
                    estadoActual = MOSTRANDO_ERROR;
                    break;
                }
            }
            break;

            //------------------------------------------
            case MOSTRANDO_EXITO:
            {
                switch (evento.tipo)
                {

                case EV_CONTINUE:
                    if (cambioEstado)
                    {
                        gestorUI.mostrarExito();
                        delay(3000);
                        estadoActual = NAVEGANDO;
                    }

                    if (tiempoActual - tiempoUltimaAccion >= TIMEOUT_EXITO_FRACASO)
                    {
                        gestorRed.reconocerRespuesta();
                        estadoActual = IDLE;
                    }
                    break;
                }
            }
            break;

            //------------------------------------------
            case MOSTRANDO_ERROR:
            {
                switch (evento.tipo)
                {

                case EV_CONTINUE:
                    if (cambioEstado)
                    {
                        gestorUI.mostrarError();
                        estadoActual = NAVEGANDO;
                    }

                    if (tiempoActual - tiempoUltimaAccion >= TIMEOUT_EXITO_FRACASO)
                    {
                        gestorRed.reconocerRespuesta();
                        estadoActual = IDLE;
                    }
                    break;
                }
            }
            break;

            //------------------------------------------
            case EMERGENCIA:
            {
                switch (evento.tipo)
                {

                case EV_CONTINUE:
                    if (cambioEstado)
                        gestorUI.mostrarEmergencia();
                    break;

                case EV_BTN_CANCELAR:
                    if (gestorBotonCancelar.estaMantenido())
                        estadoActual = IDLE;
                    break;
                }
            }
            break;
            }

            // Consumo del evento
            evento.tipo = EV_CONTINUE;
        }
    }
}