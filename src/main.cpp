#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
// #include <driver/i2s.h> // Se descomenta cuando configures el audio

#include "GestorInterfaz.h" // Asumiendo que guardaste la clase aquí
#include "GestorBoton.h"
#include "GestorDeRed.h"
#include "GestorMicrofono.h"

// ==========================================
// 📍 DEFINICIÓN DE PINES (ESP32 30-Pines)
// ==========================================

// --- Pantalla LCRD (I2C) ---
const int LCD_SDA = 21;
const int LCD_SCL = 22;
//hola

// --- Tarjeta SD (SPI) ---
const int SD_CS = 5;
const int SD_SCK = 18;
const int SD_MISO = 19;
const int SD_MOSI = 23;

// --- Micrófono INMP441 (I2S) ---
// Nota: Estos pines se usan en la estructura de config I2S, no en pinMode()
const int MIC_SCK = 14; // BCLK
const int MIC_WS = 15;  // LRC / L/R Clock
const int MIC_SD = 32;  // DIN / Data

// --- Actuadores ---
const int BUZZER_PIN = 26; // Va a la base del transistor NPN
const int LED_PIN = 27;
const int FREC_BUZZER = 2000; 

// --- Sensor Analógico ---
const int POT_PIN = 33; // Potenciómetro (ADC1)

// --- Pulsadores ---
// IMPORTANTE: Los pines 34, 35, 36 y 39 requieren resistencia física externa.
const int BTN_ARRIBA = 36;
const int BTN_ABAJO = 39;
const int BTN_CONFIRMAR = 34;
const int BTN_CANCELAR = 35;

// Estos pines sí tienen resistencia Pull-up interna en el ESP32.
const int BTN_GRABAR = 4;
const int BTN_EMERGENCIA = 13;

unsigned long ultimoTiempoRebote = 0;
const unsigned long TIEMPO_DEBOUNCE = 200; // Los mismos 200ms que tenías

// ==========================================
// 🧠 DEFINICIÓN DE LA MÁQUINA DE ESTADOS
// ==========================================

//----------------------------------------------
// EVENTOS
//----------------------------------------------
enum TipoEvento {
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

struct Evento {
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
    ESPERANDO_WIFI,    // El que ya tenías
    MOSTRANDO_EXITO,   // NUEVO
    MOSTRANDO_ERROR    // NUEVO
};

// ==========================================
// 📦 VARIABLES GLOBALES Y MOCKUPS
// ==========================================

EstadoFSM estadoActual = INIT;
EstadoFSM estadoAnterior = INIT; // Para saber cuándo redibujar la pantalla

// --- Variables de Control de Tiempo (Timeouts) ---
unsigned long tiempoUltimaAccion = 0;
unsigned long tiempoInicioGrabacion = 0;
unsigned long tiempoInicioEmergencia = 0;

const unsigned long TIMEOUT_NAVEGANDO = 30000;    // 30 segundos
const unsigned long TIMEOUT_CONFIRMAR = 15000;    // 15 segundos
const unsigned long MAX_TIEMPO_GRABACION = 60000; // 60 segundos por mensaje

// --- Variables de Datos (Mockups) ---
String listaContactos[10] = {
    "Hijo - Lucas", "Dra. Garcia", "Emergencias", "Vecino Juan",
    "Farmacia", "Hija - Maria", "Sobrino Alex", "Cuidado 24hs",
    "Bomberos", "Taxi Confianza"
};
String listaMensajes[3] = {"Llamame", "Todo bien", "Ven a casa"};

int indiceContacto = 0;
int indiceActual = 0;
int indiceMensajeActual = 0;
int indiceMensaje = 0;
bool mensajeCorrecto = true;
bool esperandoLiberacionInicial = true;
GestorDeRed::EstadoRespuesta respuestaMensaje;

// ==========================================
// 🧱 INSTANCIACIÓN DE GESTORES (Fuera del setup/loop)
// ==========================================

// 1. Instanciamos los objetos fuera del setup/loop
// Botón de grabar en el pin 4 (1 segundo)
GestorBoton gestorBotonGrabar(BTN_GRABAR, 1000);
// Botón de emergencia en el pin 5 (3 segundos para evitar falsas alarmas)
GestorBoton gestorBotonEmergencia(BTN_EMERGENCIA, 2000);
GestorBoton gestorBotonCancelar(BTN_CANCELAR, 2000);

GestorDeRed gestorRed;
// Instanciamos el gestor pasando tus constantes exactas
GestorDeMicrofono gestorAudio(BUZZER_PIN, LED_PIN, POT_PIN, FREC_BUZZER, MIC_SCK, MIC_WS, MIC_SD);

// ==========================================
// 🛠️ FUNCIÓN AUXILIAR DE BOTONES (Flanco de bajada)
// ==========================================
// Esta función evita que dejar apretado un botón haga que el menú pase a la velocidad de la luz.
// Nota: En un entorno real, es mejor usar la librería Bounce2 para el anti-rebote (debouncing).

// bool leerBotonUnClic(int pin)
// {
    // Lógica simplificada: asume botones conectados a GND con resistencia Pull-up (activos en LOW)
    // if (digitalRead(pin) == LOW)
    // {
        // delay(201); // Pequeño delay anti-rebote
        // return true;
    // }
    // return false;
// }

bool leerBotonUnClic(int pin) {
  static bool estadoAnterior[40]; // uno por pin

  bool estadoActual = digitalRead(pin);

  if (estadoAnterior[pin] == HIGH && estadoActual == LOW) {
    estadoAnterior[pin] = estadoActual;
    return true; // CLICK detectado
  }

  estadoAnterior[pin] = estadoActual;
  return false;
}

// ==========================================
// 🚀 INICIALIZACIÓN (VOID SETUP)
// ==========================================

void setup()
{
    // 1. Inicializar Puerto Serial para depuración
    Serial.begin(115200);
    Serial.println("Iniciando Sistema de Gerontotecnología...");

    // 2. Configuración de Actuadores (Salidas)
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW); // Apagado por defecto

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW); // Transistor apagado por defecto (Seguridad)

    // 3. Configuración de Botones "Sordos" (Solo Entrada)
    // Debes tener resistencias de 10k conectadas a 3.3V (Pull-up físico) o GND (Pull-down físico)
    pinMode(BTN_ARRIBA, INPUT);
    pinMode(BTN_ABAJO, INPUT);
    pinMode(BTN_CONFIRMAR, INPUT);
    pinMode(BTN_CANCELAR, INPUT);

    // 4. Configuración de Botones con Pull-up Interno
    // Al usar INPUT_PULLUP, el pin lee HIGH por defecto.
    // Cuando el abuelo presiona el botón (conectado a GND), el pin leerá LOW.
    pinMode(BTN_GRABAR, INPUT_PULLUP);
    pinMode(BTN_EMERGENCIA, INPUT_PULLUP);

    // 5. Configuración del Potenciómetro
    // En el ESP32 no es estrictamente necesario hacer pinMode() para pines analógicos,
    // pero sí es buena práctica definir la resolución de lectura (12 bits = 0 a 4095).
    analogReadResolution(12);

    // 6. Inicialización del Bus I2C (Pantalla LCD)
    Wire.begin(LCD_SDA, LCD_SCL);
    // Aquí iría tu inicialización de la librería LCD, ej: lcd.init(); lcd.backlight();
    
    // 7. Inicialización del Bus SPI y Lector SD
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS))
    {
        Serial.println("❌ ERROR: No se detectó Tarjeta SD o falló el montaje.");
        // Aquí podrías hacer sonar el buzzer 3 veces rápido como alerta de error
    }
    else
    {
        Serial.println("✅ Tarjeta SD montada correctamente.");
    }

    // 8. Inicialización del Micrófono (I2S)
    // El audio no usa pinMode. Se inicializa instalando el driver de Espressif.
    // setup_i2s_microphone(); // Llamada a tu futura función de configuración I2S

    Serial.println("✅ Setup completado. Iniciando FSM...");
    gestorUI.iniciar();
}

// ==========================================
// GENERADOR DE EVENTOS
// ==========================================

void generaEvento(unsigned long tiempoActual) {

    // Emergencia tiene prioridad máxima
    if (gestorBotonEmergencia.estaMantenido()) {
        evento.tipo = EV_BTN_EMERGENCIA;
        return;
    }

    if (leerBotonUnClic(BTN_ARRIBA)) {
        evento.tipo = EV_BTN_ARRIBA;
        return;
    }

    if (leerBotonUnClic(BTN_ABAJO)) {
        evento.tipo = EV_BTN_ABAJO;
        return;
    }

    if (leerBotonUnClic(BTN_CONFIRMAR)) {
        evento.tipo = EV_BTN_CONFIRMAR;
        return;
    }

    if (leerBotonUnClic(BTN_CANCELAR)) {
        evento.tipo = EV_BTN_CANCELAR;
        return;
    }

    if (leerBotonUnClic(BTN_GRABAR)) {
        evento.tipo = EV_BTN_GRABAR;
        return;
    }

    // Eventos internos (WiFi)
    if (estadoActual == ESPERANDO_WIFI) {
        auto r = gestorRed.actualizar();

        if (r == GestorDeRed::EXITO) {
            evento.tipo = EV_WIFI_EXITO;
            return;
        }
        if (r == GestorDeRed::ERROR) {
            evento.tipo = EV_WIFI_ERROR;
            return;
        }
    }

    evento.tipo = EV_CONTINUE;
}

// ==========================================
// ⚙️ LOOP PRINCIPAL (FSM)
// ==========================================

void loop() {

    unsigned long tiempoActual = millis();
    bool cambioEstado = (estadoActual != estadoAnterior);
    estadoAnterior = estadoActual;

    generaEvento(tiempoActual);

    switch (estadoActual) {

    //------------------------------------------
    case INIT:
    {
        switch (evento.tipo) {

            case EV_CONTINUE:
                gestorUI.mostrarPantallaInit();
                delay(3000);
                estadoActual = IDLE;
                break;
        }
    }
    break;

    //------------------------------------------
    case IDLE:
    {
        switch (evento.tipo) {

            case EV_CONTINUE:
                if (cambioEstado) {
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
        switch (evento.tipo) {

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

        if (tiempoActual - tiempoUltimaAccion >= TIMEOUT_NAVEGANDO)
            estadoActual = IDLE;
    }
    break;

    //------------------------------------------
    case CONFIRMAR_CONTACTO:
    {
        switch (evento.tipo) {

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
                if (gestorBotonGrabar.estaMantenido()) {
                    tiempoInicioGrabacion = tiempoActual;
                    gestorAudio.iniciarGrabacion();
                    estadoActual = GRABANDO;
                }
                break;

            case EV_BTN_EMERGENCIA:
                // No sé si sea 100% necesario detener la grabación, pero me suena coherente a pesar de la emergencia
                // gestorAudio.detenerGrabacion();
                estadoActual = EMERGENCIA;
                break;
        }
    }
    break;

    //------------------------------------------
    case GRABANDO:
    {
        switch (evento.tipo) {

            case EV_CONTINUE:
                if (cambioEstado)
                    gestorUI.mostrarGrabando(listaContactos[indiceContacto]);
                break;

            case EV_BTN_GRABAR:
                gestorAudio.detenerGrabacion();
                //gestorRed.iniciarEnvioMensaje();
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
        switch (evento.tipo) {
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
                //gestorRed.iniciarEnvioMensaje();
                //gestorRed.enviarMensajeTexto(listaContactos[indiceContacto], listaMensajes[indiceMensaje]);
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
        switch (evento.tipo) {

            case EV_CONTINUE:
                if (cambioEstado)
                    gestorUI.mostrarConfirmarAudio();
                break;

            case EV_BTN_CONFIRMAR:
                //gestorRed.iniciarEnvioMensaje();
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
        switch (evento.tipo) {
            // WIFI todavía no está implementado asi que por ahora se saltea esto
            case EV_CONTINUE:
                tiempoUltimaAccion = tiempoActual;
                estadoActual = MOSTRANDO_EXITO;
                break;

            // case EV_WIFI_EXITO:
            //     Acá enviaría el mensaje si puede
            //     tiempoUltimaAccion = tiempoActual;
            //     estadoActual = MOSTRANDO_EXITO;
            //     break;

            // case EV_WIFI_ERROR:
            //     Acá podría guardar el mensaje en la SD para enviarlo después, bloquearse y esperar el wifi, ponerlo en segundo plano esperando, no sé
            //     tiempoUltimaAccion = tiempoActual;
            //     estadoActual = MOSTRANDO_ERROR;
            //     break;
        }
    }
    break;

    //------------------------------------------
    case MOSTRANDO_EXITO:
    {
        switch (evento.tipo) {

            case EV_CONTINUE:
                if (cambioEstado) {
                    gestorUI.mostrarExito();
                    delay(3000);
                    estadoActual = NAVEGANDO;
                }

                if (tiempoActual - tiempoUltimaAccion >= 4000) {
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
        switch (evento.tipo) {

            case EV_CONTINUE:
                if (cambioEstado) {
                    gestorUI.mostrarError();
                    estadoActual = NAVEGANDO;
                }

                if (tiempoActual - tiempoUltimaAccion >= 4000) {
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
        switch (evento.tipo) {

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