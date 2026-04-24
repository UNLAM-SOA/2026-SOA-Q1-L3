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

enum EstadoFSM
{
    INIT,
    IDLE,
    NAVEGANDO,
    CONFIRMAR_CONTACTO,
    GRABANDO,
    CONFIRMAR_MENSAJE,
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
bool leerBotonUnClic(int pin)
{
    // Lógica simplificada: asume botones conectados a GND con resistencia Pull-up (activos en LOW)
    if (digitalRead(pin) == LOW)
    {
        delay(201); // Pequeño delay anti-rebote
        return true;
    }
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
// ⚙️ LOOP PRINCIPAL (FSM)
// ==========================================

void loop()
{
    unsigned long tiempoActual = millis();
    bool dibujarPantalla = (estadoActual != estadoAnterior);
    estadoAnterior = estadoActual; // Actualizamos el historial

    // --------------------------------------------------------
    // 🚨 INTERRUPCIÓN GLOBAL: EVENTO DE EMERGENCIA (EEM)
    // --------------------------------------------------------
    // Si presiona el botón por 5 segundos, interrumpe todo.
    if (gestorBotonEmergencia.estaMantenido())
    {
        // gestorEmergencia.dispararAlerta();
        estadoActual = EMERGENCIA;
    }

    // --------------------------------------------------------
    // 🧭 MÁQUINA DE ESTADOS PRINCIPAL
    // --------------------------------------------------------
    switch (estadoActual)
    {

        case INIT:
            Serial.println(dibujarPantalla);
            gestorUI.mostrarPantallaInit();
            delay(3000); // Único lugar donde se permite delay (logo de inicio)
            estadoActual = IDLE;
            break;

        case IDLE:
            if (dibujarPantalla)
            {
                gestorUI.mostrarPantallaReposo();
                indiceContacto = 0; // Reiniciamos el cursor
            }

            // Transición
            if (leerBotonUnClic(BTN_ARRIBA) || leerBotonUnClic(BTN_ABAJO))
            {
                tiempoUltimaAccion = tiempoActual;
                estadoActual = NAVEGANDO;
            }
            break;

        case NAVEGANDO:
            if (dibujarPantalla || indiceActual != indiceContacto)
                gestorUI.mostrarNavegandoContactos(listaContactos, indiceContacto);
            
            indiceActual = indiceContacto;
            
            if (leerBotonUnClic(BTN_ABAJO))
            {
                indiceContacto = (indiceContacto == 10) ? 10 : indiceContacto + 1;
                estadoActual = NAVEGANDO; // Forzamos redibujo
                tiempoUltimaAccion = tiempoActual;
            }
            if (leerBotonUnClic(BTN_ARRIBA))
            {
                indiceContacto = (indiceContacto == 0) ? 0 : indiceContacto - 1;
                estadoActual = NAVEGANDO; // Forzamos redibujo
                tiempoUltimaAccion = tiempoActual;
            }
            if (leerBotonUnClic(BTN_CONFIRMAR))
            {
                tiempoUltimaAccion = tiempoActual;
                estadoActual = CONFIRMAR_CONTACTO;
            }

            // Timeout
            if (tiempoActual - tiempoUltimaAccion >= TIMEOUT_NAVEGANDO)
            {
                estadoActual = IDLE;
            }
            break;

        case CONFIRMAR_CONTACTO:
            if (dibujarPantalla)
                gestorUI.mostrarConfirmarContacto(listaContactos[indiceContacto]);

            if (leerBotonUnClic(BTN_CANCELAR))
            {
                estadoActual = NAVEGANDO;
            }
            if (leerBotonUnClic(BTN_CONFIRMAR))
            {
                tiempoUltimaAccion = tiempoActual;
                estadoActual = MENSAJE_PREDEFINIDO;
            }
            
            // Transición a grabar (asumiendo que mantiene presionado 1 segundo)
            if (leerBotonUnClic(BTN_GRABAR))
                gestorAudio.confirmarInicioGrabacion();
            
            if (gestorBotonGrabar.estaMantenido())
            {
                tiempoInicioGrabacion = tiempoActual;
                gestorAudio.iniciarGrabacion();
                // gestorAudio.iniciarGrabacion(listaContactos[indiceContacto]);
                estadoActual = GRABANDO;
            }
            else if(!leerBotonUnClic(BTN_GRABAR))
            {
              gestorAudio.apagarBuzzer();
            }

            // Timeout
            if (tiempoActual - tiempoUltimaAccion >= TIMEOUT_CONFIRMAR)
            {
                estadoActual = IDLE;
            }
            break;

        case GRABANDO:
            if (dibujarPantalla)
                gestorUI.mostrarGrabando(listaContactos[indiceContacto]);

            // 1. Verificamos si la persona ya soltó el botón después del inicio
            if (esperandoLiberacionInicial)
            {
                if (digitalRead(BTN_GRABAR) == HIGH)
                {
                    delay(50); // ESTA ES LA MAGIA: 50ms matan cualquier rebote del resorte.

                    // Hacemos una lectura en falso para purgar cualquier "clic" fantasma
                    // que se haya generado durante el rebote.
                    leerBotonUnClic(BTN_GRABAR);

                    esperandoLiberacionInicial = false; // Desbloqueamos
                }
                break; // Salimos del case, no evaluamos el corte todavía
            }

            // 2. Lógica normal de corte (solo llega acá si ya soltó el dedo antes)
            if (leerBotonUnClic(BTN_GRABAR) || (tiempoActual - tiempoInicioGrabacion >= MAX_TIEMPO_GRABACION))
            {
                esperandoLiberacionInicial = true;
                gestorAudio.detenerGrabacion();
                gestorRed.iniciarEnvioMensaje();
                estadoActual = CONFIRMAR_MENSAJE;
            }
            break;

        case CONFIRMAR_MENSAJE:
            if (dibujarPantalla)
                gestorUI.mostrarConfirmarMensaje();

            if (leerBotonUnClic(BTN_CONFIRMAR))
            {
                // gestorSD.encolarMensajeParaEnvio(archivoAudioActual, listaContactos[indiceContacto]);
                gestorRed.iniciarEnvioMensaje();
                estadoActual = ESPERANDO_WIFI;
            }
            if (leerBotonUnClic(BTN_CANCELAR))
            {
                // gestorSD.borrarUltimoArchivo();
                estadoActual = CONFIRMAR_CONTACTO;
            }
            break;

        case MENSAJE_PREDEFINIDO:
            if (dibujarPantalla || indiceMensaje != indiceMensajeActual)
                gestorUI.mostrarMensajesPredefinidos(listaContactos[indiceContacto], listaMensajes, indiceMensaje);
            
            indiceMensajeActual = indiceMensaje;

            if (leerBotonUnClic(BTN_ABAJO))
            {
                indiceMensaje = indiceMensaje == 2 ? 2 : indiceMensaje + 1;
                estadoActual = MENSAJE_PREDEFINIDO;
                tiempoUltimaAccion = tiempoActual;
            }
            if (leerBotonUnClic(BTN_ARRIBA))
            {
                indiceMensaje = indiceMensaje == 0 ? 0 : indiceMensaje - 1;
                estadoActual = MENSAJE_PREDEFINIDO;
                tiempoUltimaAccion = tiempoActual;
            }
            if (leerBotonUnClic(BTN_CONFIRMAR))
            {
                // gestorRed.enviarMensajeTexto(listaContactos[indiceContacto], listaMensajes[indiceMensaje]);
                gestorRed.iniciarEnvioMensaje();
                estadoActual = ESPERANDO_WIFI;
            }
            if (leerBotonUnClic(BTN_CANCELAR))
            {
                tiempoUltimaAccion = tiempoActual;
                estadoActual = CONFIRMAR_CONTACTO;
            }
            break;

        case ESPERANDO_WIFI:
        {   // Las llaves aquí son buena práctica en C++ al declarar variables dentro de un case
            respuestaMensaje = gestorRed.actualizar();

            if (respuestaMensaje == GestorDeRed::EXITO) 
            {
              Serial.println("Printeo exito");
                tiempoUltimaAccion = tiempoActual; // Arrancamos el cronómetro de 4 seg
                estadoActual = MOSTRANDO_EXITO;
            } 
            else if (respuestaMensaje == GestorDeRed::ERROR) 
            {

              Serial.println("Printeo fracaso");
                tiempoUltimaAccion = tiempoActual; // Arrancamos el cronómetro de 4 seg
                estadoActual = MOSTRANDO_ERROR;
            }
            // Si sigue en ENVIANDO, no hace nada y el loop da otra vuelta rapidísimo
        }
        break;

        case MOSTRANDO_EXITO:
            if (dibujarPantalla) gestorUI.mostrarExito(); // Se dibuja una sola vez

            // Evaluación no bloqueante: ¿Ya pasaron 4000 ms?
            if (tiempoActual - tiempoUltimaAccion >= 4000) 
            {
                gestorRed.reconocerRespuesta(); // Limpiamos el gestor
                estadoActual = IDLE;
            }
            break;

        case MOSTRANDO_ERROR:
            if (dibujarPantalla) gestorUI.mostrarError(); // Se dibuja una sola vez

            // Evaluación no bloqueante: ¿Ya pasaron 4000 ms?
            if (tiempoActual - tiempoUltimaAccion >= 4000) 
            {
                gestorRed.reconocerRespuesta(); // Limpiamos el gestor
                estadoActual = IDLE;
            }
            break;

        case EMERGENCIA:
            if (dibujarPantalla)
            {
                gestorUI.mostrarEmergencia();
                // digitalWrite(BUZZER_PIN, HIGH); // Encender sirena
            }

            // Estado Enclavado: Solo se sale manteniendo "Cancelar" por 5 segundos
            if (gestorBotonCancelar.estaMantenido())
            {
                // gestorEmergencia.dispararAlerta();
                estadoActual = IDLE;
            }
            break;
    }
}