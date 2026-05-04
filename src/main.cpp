#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
// #include <driver/i2s.h> // Se usará en el futuro para audio

#include "GestorInterfaz.h"
#include "GestorBoton.h"
#include "GestorDeRed.h"
#include "GestorAlmacenamiento.h"
#include "GestorMicrofono.h"

// MODO DEBUG
#define SERIAL_ENABLED 1
#if SERIAL_ENABLED
#define SerialPrint(str) Serial.println(str)
#else
#define SerialPrint(str)
#endif

#define MAX_EVENTOS 11
#define MAX_CONTACTOS 10
#define MAX_MENSAJES 3

// --- Variables de Control de Tiempo (Timeouts) ---
#define TIMEOUT_GENERAL (SERIAL_ENABLED == 1) ? 4000 : 30000   // 30 segundos
#define TIMEOUT_GRABACION (SERIAL_ENABLED == 1) ? 6000 : 60000 // 60 segundos por mensaje
#define TIMEOUT_BUZZER 1000
#define BTN_EMERGENCIA_TIEMPO (SERIAL_ENABLED == 1) ? 2000 : 5000
#define DELAY_INIT 3000
#define TIMEOUT_EXITO_FRACASO 4000

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
    EV_BUZZER_FIN,
    EV_BTN_EMERGENCIA,
    EV_BTN_EMERGENCIA_PRESS,
    EV_TIMEOUT,
    EV_WIFI_EXITO,
    EV_WIFI_ERROR
};

struct Evento
{
    TipoEvento tipo;
};

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
    INICIANDO_GRABACION,
    CONFIRMAR_AUDIO,
    MENSAJE_PREDEFINIDO,
    EMERGENCIA,
    ESPERANDO_WIFI,
    MOSTRANDO_EXITO,
    MOSTRANDO_ERROR
};

// ==========================================
// VARIABLES GLOBALES Y MOCKUPS
// ==========================================

Evento evento, eventoAnterior;

EstadoFSM estadoActual = INIT;
EstadoFSM estadoAnterior = INIT;

// --- Variables de Datos (Mockups) ---
// Esto va a llegar desde nodeRED o desde android, por ahora se hardcodea
String listaContactos[MAX_CONTACTOS] = {
    "Hijo - Lucas", "Dra. Garcia", "Emergencias", "Vecino Juan",
    "Farmacia", "Hija - Maria", "Sobrino Alex", "Cuidado 24hs",
    "Bomberos", "Taxi Confianza"};
String listaMensajes[MAX_MENSAJES] = {"Llamame", "Todo bien", "Ven a casa"};

int indiceContacto = 0;
int indiceActual = 0;
int indiceMensajeActual = 0;
int indiceMensaje = 0;

// ==========================================
// INSTANCIACIÓN DE GESTORES
// ==========================================

GestorBoton gestorBotonEmergencia(BTN_EMERGENCIA, BTN_EMERGENCIA_TIEMPO);

GestorDeRed gestorRed;
GestorAlmacenamiento gestorSD(SD_CS, SD_SCK, SD_MISO, SD_MOSI);
GestorDeMicrofono gestorAudio(BUZZER_PIN, LED_PIN, POT_PIN, FREC_BUZZER, MIC_SCK, MIC_WS, MIC_SD);

QueueHandle_t colaEventos;
TimerHandle_t xTimeoutTimer, xRecordingTimer, xBuzzerTimer;
TaskHandle_t getEventHandler;

// ==========================================
// PROTOTIPOS DE FUNCIONES
// ==========================================

void vTimeoutCallback(TimerHandle_t xTimer);
void vBuzzerCallback(TimerHandle_t xTimer);
bool leerBotonUnClic(int pin);
void setup();
void loop();
void taskEvento(void *pvParameters);
void taskFSM(void *pvParameters);
String eventoToString(TipoEvento evento);
String estadoToString(EstadoFSM estado);

// ==========================================
// CALLBACKS Y FUNCIONES AUXILIARES
// ==========================================
/*Tenemos las señales de timeout y de finalizacion del buzzer, para diferenciarlas
usamos un bit */
void vTimeoutCallback(TimerHandle_t xTimer)
{
    xTaskNotify(getEventHandler, BIT_TIMEOUT, eSetBits);
}

void vBuzzerCallback(TimerHandle_t xTimer)
{
    xTaskNotify(getEventHandler, BIT_BUZZER_FIN, eSetBits);
}
/*
esta funcion detecta el flanco descendente del click, junto con los 50ms del vTaskDelay en taskEvento
permite leer correctamente los clicks
*/
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

    if (!gestorSD.iniciarSD())
    {
        Serial.println("ERROR: No se detectó Tarjeta SD o falló el montaje.");
    }
    else
    {
        Serial.println("Tarjeta SD montada correctamente.");
        gestorAudio.setAlmacenamiento(&gestorSD);
    }

    Serial.println("Setup completado. Iniciando FSM...");
    gestorUI.iniciar();

    // Creamos una cola capaz de almacenar todos los eventos
    colaEventos = xQueueCreate(MAX_EVENTOS, sizeof(TipoEvento));

    if (colaEventos != NULL)
    {
        xTaskCreate(taskEvento, "GeneradorEventos", 2048, NULL, 1, &getEventHandler);
        xTaskCreate(taskFSM, "MaquinaEstados", 4096, NULL, 2, NULL);
        Serial.println("Tareas creadas");
    }

    // Inicializacion de timers
    xTimeoutTimer = xTimerCreate(
        "TimeoutTimer",
        pdMS_TO_TICKS(TIMEOUT_GENERAL),
        pdFALSE,
        NULL,
        vTimeoutCallback);

    xRecordingTimer = xTimerCreate(
        "RecordingTimer",
        pdMS_TO_TICKS(TIMEOUT_GRABACION),
        pdFALSE,
        NULL,
        vTimeoutCallback);

    xBuzzerTimer = xTimerCreate(
        "BuzzerTimer",
        pdMS_TO_TICKS(TIMEOUT_BUZZER),
        pdFALSE,
        NULL,
        vBuzzerCallback);
}

void loop()
{
    vTaskDelete(NULL);
}

// ==========================================
// GENERADOR DE EVENTOS
// ==========================================

void taskEvento(void *pvParameters)
{
    while (1)
    {
        #if SERIAL_ENABLED
                eventoAnterior.tipo = evento.tipo;
        #endif
        evento.tipo = EV_CONTINUE;
        uint32_t notificaciones = 0;

        
        if (gestorBotonEmergencia.estaMantenido())
        {
            evento.tipo = EV_BTN_EMERGENCIA;
        }
        else if (leerBotonUnClic(BTN_EMERGENCIA))
        {
            evento.tipo = EV_BTN_EMERGENCIA_PRESS;
        }

        xTaskNotifyWait(0, 0xFFFFFFFF, &notificaciones, 0);

        if (notificaciones & BIT_TIMEOUT)
        {
            evento.tipo = EV_TIMEOUT;
        }
        else if (notificaciones & BIT_BUZZER_FIN)
        {
            evento.tipo = EV_BUZZER_FIN;
        }
        else if (leerBotonUnClic(BTN_ARRIBA))
        {
            evento.tipo = EV_BTN_ARRIBA;
        }
        else if (leerBotonUnClic(BTN_ABAJO))
        {
            evento.tipo = EV_BTN_ABAJO;
        }
        else if (leerBotonUnClic(BTN_CONFIRMAR))
        {
            evento.tipo = EV_BTN_CONFIRMAR;
        }
        else if (leerBotonUnClic(BTN_CANCELAR))
        {
            evento.tipo = EV_BTN_CANCELAR;
        }
        else if (leerBotonUnClic(BTN_GRABAR))
        {
            evento.tipo = EV_BTN_GRABAR;
        }
        // Eventos internos (WiFi)
        else if (estadoActual == ESPERANDO_WIFI)
        {
            auto r = gestorRed.actualizar();
            if (r == GestorDeRed::EXITO)
            {
                evento.tipo = EV_WIFI_EXITO;
            }
            else if (r == GestorDeRed::ERROR)
            {
                evento.tipo = EV_WIFI_ERROR;
            }
        }

        xQueueSend(colaEventos, &evento.tipo, portMAX_DELAY);

        #if SERIAL_ENABLED
                if (eventoAnterior.tipo != evento.tipo)
                    SerialPrint("Evento detectado y encolado: " + eventoToString(eventoAnterior.tipo));
        #endif
        // Verifica cada 50ms, de este modo reducimos el uso del CPU
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ==========================================
// LOOP PRINCIPAL (FSM)
// ==========================================

void taskFSM(void *pvParameters)
{
    TipoEvento eventoRecibido;
    while (1)
    {
        if (xQueueReceive(colaEventos, &eventoRecibido, portMAX_DELAY) == pdPASS)
        {
            evento.tipo = eventoRecibido;
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
                    estadoActual = NAVEGANDO;
                    xTimerStart(xTimeoutTimer, 0);
                    break;

                case EV_BTN_EMERGENCIA:
                    estadoActual = EMERGENCIA;
                    xTimerStop(xTimeoutTimer, 0);
                    break;
                
                case EV_BTN_EMERGENCIA_PRESS:
                    xTimerReset(xTimeoutTimer, 0);
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
                    xTimerReset(xTimeoutTimer, 0);
                    break;

                case EV_BTN_ARRIBA:
                    indiceContacto = max(indiceContacto - 1, 0);
                    xTimerReset(xTimeoutTimer, 0);
                    break;

                case EV_BTN_CONFIRMAR:
                    estadoActual = CONFIRMAR_CONTACTO;
                    xTimerReset(xTimeoutTimer, 0);
                    break;

                case EV_BTN_EMERGENCIA:
                    estadoActual = EMERGENCIA;
                    xTimerStop(xTimeoutTimer, 0);
                    break;
                case EV_BTN_EMERGENCIA_PRESS:
                    xTimerReset(xTimeoutTimer, 0);
                    break;

                case EV_TIMEOUT:
                    estadoActual = IDLE;
                    xTimerStop(xTimeoutTimer, 0);
                    break;
                }
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
                    xTimerReset(xTimeoutTimer, 0);
                    break;

                case EV_BTN_CONFIRMAR:
                    estadoActual = MENSAJE_PREDEFINIDO;
                    xTimerReset(xTimeoutTimer, 0);
                    break;

                case EV_BTN_GRABAR:
                    gestorAudio.confirmarInicioGrabacion();
                    xTimerStop(xTimeoutTimer, 0);
                    xTimerStart(xBuzzerTimer, 0);
                    gestorAudio.leerYActualizarVolumen();
                    estadoActual = INICIANDO_GRABACION;
                    break;

                case EV_BTN_EMERGENCIA:
                    estadoActual = EMERGENCIA;
                    xTimerStop(xTimeoutTimer, 0);
                    break;

                case EV_BTN_EMERGENCIA_PRESS:
                    xTimerReset(xTimeoutTimer, 0);
                    break;
                case EV_TIMEOUT:
                    estadoActual = IDLE;
                    xTimerStop(xTimeoutTimer, 0);
                    break;
                }
            }
            break;

            case INICIANDO_GRABACION:
            {
                switch (evento.tipo)
                {
                case EV_CONTINUE:
                    if (cambioEstado)
                    {
                        gestorUI.mostrarConfirmarContacto(listaContactos[indiceContacto]);
                        gestorAudio.leerYActualizarVolumen();
                    }
                    break;

                case EV_BUZZER_FIN:
                    gestorAudio.iniciarGrabacion("/archivoAudio.bin");
                    estadoActual = GRABANDO;
                    xTimerStop(xBuzzerTimer, 0);
                    xTimerStart(xRecordingTimer, 0);
                    break;

                case EV_BTN_CANCELAR:
                    gestorAudio.apagarBuzzer();
                    estadoActual = CONFIRMAR_CONTACTO;
                    xTimerStop(xBuzzerTimer, 0);
                    xTimerStart(xTimeoutTimer, 0);
                    break;
                }
                // No hay tiempo suficiente para activar el boton de emergencia, por tanto no es necesario modelarlo.
            }
            break;

            //------------------------------------------
            case GRABANDO:
            {
                switch (evento.tipo)
                {

                case EV_CONTINUE:
                    // Simulamos la grabación del micrófono
                    gestorAudio.registrarMedida();
                    if (cambioEstado)
                        gestorUI.mostrarGrabando(listaContactos[indiceContacto]);
                    break;

                case EV_BTN_GRABAR:
                    gestorAudio.detenerGrabacion();
                    // gestorRed.iniciarEnvioMensaje();
                    estadoActual = CONFIRMAR_AUDIO;
                    xTimerStop(xRecordingTimer, 0);
                    xTimerStart(xTimeoutTimer, 0);
                    break;

                case EV_BTN_EMERGENCIA:
                    gestorAudio.detenerGrabacion();
                    estadoActual = EMERGENCIA;
                    xTimerStop(xRecordingTimer, 0);
                    break;
                case EV_TIMEOUT:
                    gestorAudio.detenerGrabacion();
                    estadoActual = CONFIRMAR_AUDIO;
                    xTimerStop(xRecordingTimer, 0);
                    xTimerStart(xTimeoutTimer, 0);
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
                    xTimerReset(xTimeoutTimer, 0);
                    break;

                case EV_BTN_ARRIBA:
                    indiceMensaje = (indiceMensaje == 0) ? 0 : indiceMensaje - 1;
                    xTimerReset(xTimeoutTimer, 0);
                    break;

                case EV_BTN_CONFIRMAR:
                    estadoActual = ESPERANDO_WIFI;
                    xTimerStop(xTimeoutTimer, 0);
                    break;

                case EV_BTN_CANCELAR:
                    estadoActual = CONFIRMAR_CONTACTO;
                    xTimerReset(xTimeoutTimer, 0);
                    break;

                case EV_BTN_EMERGENCIA:
                    estadoActual = EMERGENCIA;
                    xTimerStop(xTimeoutTimer, 0);
                    break;
                case EV_BTN_EMERGENCIA_PRESS:
                    xTimerReset(xTimeoutTimer, 0);
                    break;
                case EV_TIMEOUT:
                    estadoActual = IDLE;
                    xTimerStop(xTimeoutTimer, 0);
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
                    gestorSD.leerArchivo();
                    xTimerStop(xTimeoutTimer, 0);
                    break;

                case EV_BTN_CANCELAR:
                    estadoActual = CONFIRMAR_CONTACTO;
                    gestorSD.eliminarArchivo();
                    xTimerReset(xTimeoutTimer, 0);
                    break;

                case EV_BTN_EMERGENCIA:
                    estadoActual = EMERGENCIA;
                    gestorSD.eliminarArchivo();
                    xTimerStop(xTimeoutTimer, 0);
                    break;
                case EV_BTN_EMERGENCIA_PRESS:
                    xTimerReset(xTimeoutTimer, 0);
                    break;
                case EV_TIMEOUT:
                    estadoActual = ESPERANDO_WIFI;
                    xTimerStop(xTimeoutTimer, 0);
                    break;
                }
            }
            break;

            //------------------------------------------
            case ESPERANDO_WIFI:
            {
                //TODO implementar un timeout para wifi y que se pueda llegar a emergencias si no se resolvio el mensaje
                switch (evento.tipo)
                {
                // Funcion mockeada esperando a ser implementada a futuro
                case EV_CONTINUE:
                    estadoActual = MOSTRANDO_EXITO;
                    break;

                case EV_WIFI_EXITO:
                    estadoActual = MOSTRANDO_EXITO;
                    break;

                case EV_WIFI_ERROR:
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
                        delay(TIMEOUT_EXITO_FRACASO);
                        estadoActual = NAVEGANDO;
                        gestorSD.eliminarArchivo();
                        xTimerStart(xTimeoutTimer, 0);
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
                        delay(TIMEOUT_EXITO_FRACASO);
                        estadoActual = NAVEGANDO;
                        gestorSD.eliminarArchivo();
                        xTimerStart(xTimeoutTimer, 0);
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

// ==========================================
// FUNCIONES DE DEPURACIÓN (TO-STRING)
// ==========================================

String eventoToString(TipoEvento evento)
{
    switch (evento)
    {
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
    default:
        return "EV_DESCONOCIDO";
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