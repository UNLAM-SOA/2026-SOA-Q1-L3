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
#include "gestorMQTT.h"

// ==========================================
// CONFIGURACIÓN DE RED Y MQTT
// ==========================================

// Credenciales WiFi (Hotspot del celular)
const char* WIFI_SSID = "motorola edge 40_5723";
const char* WIFI_PASS = "SecretoNonofono8";

// Credenciales del Broker (HiveMQ Cloud)
const char* MQTT_BROKER = "e10a0f3769d14449b0472d6e60e344a9.s1.eu.hivemq.cloud";
const char* MQTT_USER   = "admin_prueba";
const char* MQTT_PASS   = "Nonofono8";

// ==========================================
// INSTANCIAS GLOBALES  
// ==========================================

// Instanciamos nuestro gestor inyectando las constantes
GestorMQTT gestorMQTT(
    WIFI_SSID, 
    WIFI_PASS, 
    MQTT_BROKER, 
    MQTT_USER, 
    MQTT_PASS
);
// MODO DEBUG
#define SERIAL_ENABLED 0
#if SERIAL_ENABLED
#define SerialPrint(str) Serial.println(str)
#else
#define SerialPrint(str)
#endif

#define MAX_EVENTOS 11
#define MAX_CONTACTOS 10
#define MAX_MENSAJES 3
#define CANT_PINES 40

// --- Variables de Control de Tiempo (Timeouts) ---
#define TIMEOUT_GENERAL (SERIAL_ENABLED == 1) ? 4000 : 30000   // 30 segundos
#define TIMEOUT_GRABACION (SERIAL_ENABLED == 1) ? 6000 : 60000 // 60 segundos por mensaje
#define TIMEOUT_BUZZER 1000
#define BTN_EMERGENCIA_TIEMPO (SERIAL_ENABLED == 1) ? 2000 : 5000
#define DELAY_INIT 3000
#define TIMEOUT_EXITO_FRACASO 4000
#define BITS_RESOLUCION 12
#define TAM_STACK_GET_EVENT 4096
#define TAM_STACK_FSM 8192

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

TaskHandle_t xGrabacionTaskHandle = NULL;
volatile bool grabandoAudio = false;
void taskGrabacionAudio(void *pvParameters)
{
    Serial.println("[Audio] Tarea de grabación iniciada en segundo plano.");
    while (grabandoAudio) // <-- Ahora depende de la bandera
    {
        gestorAudio.registrarMedida();
    }

    // Cuando la bandera sea falsa, sale del while y llega aquí
    Serial.println("[Audio] Tarea finalizada limpiamente.");
    vTaskDelete(NULL); // Se autodestruye de forma segura
}
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
void enviarMensajeMQTT();
void procesarMensajeEntrante(char* topic, byte* payload, unsigned int length);

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
    static bool estadoAnterior[CANT_PINES]; // uno por pin

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
    Serial.begin(921600);
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
    analogReadResolution(BITS_RESOLUCION);

    // Inicialización del Bus I2C (Pantalla LCD)
    Wire.begin(LCD_SDA, LCD_SCL);

    // 1. Inyectamos la dependencia siempre, exista o no la SD física en este instante
    gestorAudio.setAlmacenamiento(&gestorSD);

    // 2. Inicializamos el hardware del micrófono (Driver I2S)
    gestorAudio.iniciar();

    Serial.println("Iniciando módulos de red...");
    // Delegamos la conexión inicial al gestor
    gestorMQTT.configurarReceptor(procesarMensajeEntrante); // <--- Agrega esta línea
    gestorMQTT.iniciarWiFi();
    gestorMQTT.conectarBroker();

    // 3. Montamos la tarjeta
    if (!gestorSD.iniciarSD())
    {
        Serial.println("ERROR: No se detectó Tarjeta SD o falló el montaje.");
        // Opcional: Podrías cambiar el estadoActual a un estado de error de hardware aquí
    }
    else
    {
        Serial.println("Tarjeta SD montada correctamente.");
    }

    Serial.println("Setup completado. Iniciando FSM...");
    gestorUI.iniciar();

    // Creamos una cola capaz de almacenar todos los eventos
    colaEventos = xQueueCreate(MAX_EVENTOS, sizeof(TipoEvento));

    if (colaEventos != NULL)
    {
        xTaskCreate(taskEvento, "GeneradorEventos", TAM_STACK_GET_EVENT, NULL, 1, &getEventHandler);
        xTaskCreate(taskFSM, "MaquinaEstados", TAM_STACK_FSM, NULL, 2, NULL);
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

#if SERIAL_ENABLED
        if (evento.tipo != eventoAnterior.tipo)
        {
            SerialPrint("Evento actual: " + eventoToString(evento.tipo));
        }
#endif
        // xQueueSend va debajo porque sino genera una race condition y no permite visualizar correctamente el debug
        xQueueSend(colaEventos, &evento.tipo, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(100));
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
            estadoAnterior = estadoActual;

            if (SERIAL_ENABLED && estadoActual != estadoAnterior)
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
                    gestorUI.mostrarPantallaReposo();
                    indiceContacto = 0;
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
                    gestorMQTT.mantenerConexion();
                    break;
                case EV_BTN_ARRIBA:
                case EV_BTN_ABAJO:
                    estadoActual = NAVEGANDO;
                    xTimerStart(xTimeoutTimer, 0);
                    gestorUI.mostrarNavegandoContactos(listaContactos, indiceContacto);
                    break;

                case EV_BTN_EMERGENCIA:
                    estadoActual = EMERGENCIA;
                    xTimerStop(xTimeoutTimer, 0);
                    gestorUI.mostrarEmergencia();
                    enviarMensajeMQTT();
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
                case EV_BTN_ABAJO:
                    indiceContacto = min(indiceContacto + 1, gestorUI.obtenerCantidadEfectiva(listaContactos) - 1);
                    if (indiceActual != indiceContacto)
                        gestorUI.mostrarNavegandoContactos(listaContactos, indiceContacto);
                    indiceActual = indiceContacto;
                    xTimerReset(xTimeoutTimer, 0);
                    break;

                case EV_BTN_ARRIBA:
                    indiceContacto = max(indiceContacto - 1, 0);
                    if (indiceActual != indiceContacto)
                        gestorUI.mostrarNavegandoContactos(listaContactos, indiceContacto);
                    indiceActual = indiceContacto;
                    xTimerReset(xTimeoutTimer, 0);
                    break;

                case EV_BTN_CONFIRMAR:
                    estadoActual = CONFIRMAR_CONTACTO;
                    xTimerReset(xTimeoutTimer, 0);
                    gestorUI.mostrarConfirmarContacto(listaContactos[indiceContacto]);
                    break;

                case EV_BTN_EMERGENCIA:
                    estadoActual = EMERGENCIA;
                    xTimerStop(xTimeoutTimer, 0);
                    gestorUI.mostrarEmergencia();
                    break;
                case EV_BTN_EMERGENCIA_PRESS:
                    xTimerReset(xTimeoutTimer, 0);
                    break;

                case EV_TIMEOUT:
                    estadoActual = IDLE;
                    xTimerStop(xTimeoutTimer, 0);
                    gestorUI.mostrarPantallaReposo();
                    indiceContacto = 0;
                    break;
                }
            }
            break;

            //------------------------------------------
            case CONFIRMAR_CONTACTO:
            {
                switch (evento.tipo)
                {
                case EV_BTN_CANCELAR:
                    estadoActual = NAVEGANDO;
                    gestorUI.mostrarNavegandoContactos(listaContactos, indiceContacto);
                    xTimerReset(xTimeoutTimer, 0);
                    break;

                case EV_BTN_CONFIRMAR:
                    estadoActual = MENSAJE_PREDEFINIDO;
                    xTimerReset(xTimeoutTimer, 0);
                    gestorUI.mostrarMensajesPredefinidos(listaContactos[indiceContacto], listaMensajes, indiceMensaje);
                    break;

                case EV_BTN_GRABAR:
                    gestorAudio.confirmarInicioGrabacion();
                    xTimerStop(xTimeoutTimer, 0);
                    xTimerStart(xBuzzerTimer, 0);
                    gestorAudio.leerYActualizarVolumen();
                    estadoActual = INICIANDO_GRABACION;
                    gestorUI.mostrarConfirmarContacto(listaContactos[indiceContacto]);
                    gestorAudio.leerYActualizarVolumen();
                    break;

                case EV_BTN_EMERGENCIA:
                    estadoActual = EMERGENCIA;
                    xTimerStop(xTimeoutTimer, 0);
                    gestorUI.mostrarEmergencia();
                    break;

                case EV_BTN_EMERGENCIA_PRESS:
                    xTimerReset(xTimeoutTimer, 0);
                    break;
                case EV_TIMEOUT:
                    estadoActual = IDLE;
                    gestorUI.mostrarPantallaReposo();
                    indiceContacto = 0;
                    xTimerStop(xTimeoutTimer, 0);
                    break;
                }
            }
            break;

            case INICIANDO_GRABACION:
            {
                switch (evento.tipo)
                {
                case EV_BUZZER_FIN:
                    gestorAudio.iniciarGrabacion("/archivoAudio.wav");
                    estadoActual = GRABANDO;
                    xTimerStop(xBuzzerTimer, 0);
                    xTimerStart(xRecordingTimer, 0);
                    gestorUI.mostrarGrabando(listaContactos[indiceContacto]);
                    // Gemini
                    grabandoAudio = true; // Levantamos la bandera
                    xTaskCreate(taskGrabacionAudio, "GrabandoAudio", 4096, NULL, 3, &xGrabacionTaskHandle);
                    break;

                case EV_BTN_CANCELAR:
                    gestorAudio.apagarBuzzer();
                    estadoActual = CONFIRMAR_CONTACTO;
                    xTimerStop(xBuzzerTimer, 0);
                    xTimerStart(xTimeoutTimer, 0);
                    gestorUI.mostrarConfirmarContacto(listaContactos[indiceContacto]);
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
                    // case EV_CONTINUE:
                    //     // Simulamos la grabación del micrófono
                    //     gestorAudio.registrarMedida();
                    //     break;

                case EV_BTN_GRABAR:

                    grabandoAudio = false;
                    // 2. Le damos 50 milisegundos de cortesía para que termine de
                    //    escribir su último bloque en la SD y libere el bus SPI.
                    vTaskDelay(pdMS_TO_TICKS(50));
                    // 3. Ahora sí, cerramos el archivo. La cabecera se escribirá con éxito.
                    gestorAudio.detenerGrabacion();
                    // gestorRed.iniciarEnvioMensaje();
                    estadoActual = CONFIRMAR_AUDIO;
                    gestorUI.mostrarConfirmarAudio();
                    xTimerStop(xRecordingTimer, 0);
                    xTimerStart(xTimeoutTimer, 0);
                    gestorSD.depurarArchivo();
                    break;

                case EV_BTN_EMERGENCIA:
                    grabandoAudio = false;
                    // 2. Le damos 50 milisegundos de cortesía para que termine de
                    //    escribir su último bloque en la SD y libere el bus SPI.
                    vTaskDelay(pdMS_TO_TICKS(50));
                    // 3. Ahora sí, cerramos el archivo. La cabecera se escribirá con éxito.
                    gestorAudio.detenerGrabacion();
                    estadoActual = EMERGENCIA;
                    gestorUI.mostrarEmergencia();
                    xTimerStop(xRecordingTimer, 0);
                    break;
                case EV_TIMEOUT:

                    grabandoAudio = false;
                    // 2. Le damos 50 milisegundos de cortesía para que termine de
                    //    escribir su último bloque en la SD y libere el bus SPI.
                    vTaskDelay(pdMS_TO_TICKS(50));
                    // 3. Ahora sí, cerramos el archivo. La cabecera se escribirá con éxito.
                    gestorAudio.detenerGrabacion();
                    estadoActual = CONFIRMAR_AUDIO;
                    gestorUI.mostrarConfirmarAudio();
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
                case EV_BTN_ABAJO:
                    indiceMensaje = (indiceMensaje == 2) ? 2 : indiceMensaje + 1;
                    if (indiceMensaje != indiceMensajeActual)
                        gestorUI.mostrarMensajesPredefinidos(listaContactos[indiceContacto], listaMensajes, indiceMensaje);
                    indiceMensajeActual = indiceMensaje;
                    xTimerReset(xTimeoutTimer, 0);
                    break;

                case EV_BTN_ARRIBA:
                    indiceMensaje = (indiceMensaje == 0) ? 0 : indiceMensaje - 1;
                    if (indiceMensaje != indiceMensajeActual)
                        gestorUI.mostrarMensajesPredefinidos(listaContactos[indiceContacto], listaMensajes, indiceMensaje);
                    indiceMensajeActual = indiceMensaje;
                    xTimerReset(xTimeoutTimer, 0);
                    break;

                case EV_BTN_CONFIRMAR:
                    estadoActual = ESPERANDO_WIFI;
                    xTimerStop(xTimeoutTimer, 0);
                    break;

                case EV_BTN_CANCELAR:
                    estadoActual = CONFIRMAR_CONTACTO;
                    xTimerReset(xTimeoutTimer, 0);
                    gestorUI.mostrarConfirmarContacto(listaContactos[indiceContacto]);
                    break;

                case EV_BTN_EMERGENCIA:
                    estadoActual = EMERGENCIA;
                    xTimerStop(xTimeoutTimer, 0);
                    gestorUI.mostrarEmergencia();
                    break;
                case EV_BTN_EMERGENCIA_PRESS:
                    xTimerReset(xTimeoutTimer, 0);
                    break;
                case EV_TIMEOUT:
                    estadoActual = IDLE;
                    gestorUI.mostrarPantallaReposo();
                    indiceContacto = 0;
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
                case EV_BTN_CONFIRMAR:
                    estadoActual = ESPERANDO_WIFI;
                    
                    // 1. LE PONEMOS UN BOZAL A LA TAREA DE EVENTOS
                    // Suspendemos taskEvento para que no despierte ni imprima nada en Serial
                    if (getEventHandler != NULL) {
                        vTaskSuspend(getEventHandler);
                    }
                    
                    // 2. Enviamos el archivo con exclusividad absoluta del hardware
                    gestorSD.enviarArchivoPorSerial();
                    
                    // 3. REANUDAMOS la tarea de eventos para que el sistema siga operando
                    if (getEventHandler != NULL) {
                        vTaskResume(getEventHandler);
                    }
                    
                    xTimerStop(xTimeoutTimer, 0);
                    break;

                case EV_BTN_CANCELAR:
                    estadoActual = CONFIRMAR_CONTACTO;
                    gestorSD.eliminarArchivo();
                    xTimerReset(xTimeoutTimer, 0);
                    gestorUI.mostrarConfirmarContacto(listaContactos[indiceContacto]);
                    break;

                case EV_BTN_EMERGENCIA:
                    estadoActual = EMERGENCIA;
                    gestorSD.eliminarArchivo();
                    xTimerStop(xTimeoutTimer, 0);
                    gestorUI.mostrarEmergencia();
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
                // TODO implementar un timeout para wifi y que se pueda llegar a emergencias si no se resolvio el mensaje
                switch (evento.tipo)
                {
                // Funcion mockeada esperando a ser implementada a futuro
                case EV_CONTINUE:
                case EV_WIFI_EXITO:
                    estadoActual = MOSTRANDO_EXITO;
                    gestorUI.mostrarExito();
                    vTaskDelay(TIMEOUT_EXITO_FRACASO);
                    gestorSD.eliminarArchivo();
                    xTimerStart(xTimeoutTimer, 0);
                    break;

                case EV_WIFI_ERROR:
                    estadoActual = MOSTRANDO_ERROR;
                    gestorUI.mostrarError();
                    vTaskDelay(TIMEOUT_EXITO_FRACASO);
                    gestorSD.eliminarArchivo();
                    xTimerStart(xTimeoutTimer, 0);
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
                    estadoActual = NAVEGANDO;
                    gestorUI.mostrarNavegandoContactos(listaContactos, indiceContacto);
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
                    estadoActual = NAVEGANDO;
                    gestorUI.mostrarNavegandoContactos(listaContactos, indiceContacto);
                }
            }
            break;

            //------------------------------------------
            case EMERGENCIA:
            {
                switch (evento.tipo)
                {
                case EV_BTN_CANCELAR:
                    estadoActual = IDLE;
                    gestorUI.mostrarPantallaReposo();
                    indiceContacto = 0;
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

void enviarMensajeMQTT()
{
    // 1. Verificamos que no se haya caído el WiFi o la conexión a HiveMQ
    gestorMQTT.mantenerConexion();

    // 2. Generamos un número aleatorio entre 1000 y 9999 para distinguir el mensaje
    long idRandom = random(1000, 9999);

    // 3. Armamos la carga útil (Payload) inyectando el identificador
    String payload = "{\"alerta\": true, \"mensaje\": \"¡Emergencia! El abuelo necesita ayuda.\", \"id_mensaje\": " + String(idRandom) + "}";

    // 4. El Gestor ejecuta la lógica de negocio y envía el mensaje
    gestorMQTT.publicarAlerta("nonofono/alertas", payload.c_str());
}

void procesarMensajeEntrante(char* topic, byte* payload, unsigned int length) {
    // 1. Convertimos los bytes crudos a un String de C++
    String mensaje = "";
    for (int i = 0; i < length; i++) {
        mensaje += (char)payload[i];
    }

    // 2. Filtramos por el tópico de configuración
    if (String(topic) == "nonofono/config/contactos") {
        
        int indiceInicio = 0;
        int indicePipe = mensaje.indexOf('|');
        int idxContacto = 0; // Índice para recorrer tu array global listaContactos

        Serial.println("\n[Config] --- SOBREESCRIBIENDO LISTA GLOBAL DE CONTACTOS ---");
        
        // 3. Recorremos la cadena buscando el delimitador '|' y cuidando de no desbordar el array
        while (indicePipe != -1 && idxContacto < MAX_CONTACTOS) {
            String contacto = mensaje.substring(indiceInicio, indicePipe);
            contacto.trim(); 

            if (contacto.length() > 0) {
                // Pisamos el valor en la posición actual del array global
                listaContactos[idxContacto] = contacto;
                Serial.printf("listaContactos[%d] actualizado -> %s\n", idxContacto, listaContactos[idxContacto].c_str());
                idxContacto++;
            }

            // Desplazamos los índices para buscar el siguiente elemento
            indiceInicio = indicePipe + 1;
            indicePipe = mensaje.indexOf('|', indiceInicio);
        }

        // 4. Resguardo: Procesamos el último fragmento si el mensaje no terminaba en '|'
        if (indiceInicio < mensaje.length() && idxContacto < MAX_CONTACTOS) {
            String ultimoContacto = mensaje.substring(indiceInicio);
            ultimoContacto.trim();
            if (ultimoContacto.length() > 0) {
                listaContactos[idxContacto] = ultimoContacto;
                Serial.printf("listaContactos[%d] actualizado (final) -> %s\n", idxContacto, listaContactos[idxContacto].c_str());
                idxContacto++;
            }
        }

        // 5. Limpieza residual: Si la nueva lista es más corta que MAX_CONTACTOS,
        // vaciamos las posiciones restantes para eliminar los datos viejos/fantasmas.
        int contactosActualizados = idxContacto; 
        while (idxContacto < MAX_CONTACTOS) {
            listaContactos[idxContacto] = ""; // Se setea como String vacío
            idxContacto++;
        }
        
        Serial.printf("[Config] Actualización terminada. Se cargaron %d contactos nuevos.\n", contactosActualizados);
        Serial.println("-----------------------------------------------------------\n");
    }
}