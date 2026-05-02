#pragma once
#include <Arduino.h>

class GestorDeMicrofono {
private:
    // --- Atributos de Hardware ---
    int pinBuzzer;
    int pinLed;
    int pinVolumen;
    int frecuenciaPitido;

    // --- Pines del Micrófono I2S (Para uso futuro) ---
    int pinMicBCLK;
    int pinMicWS;
    int pinMicDATA;

    // --- Estado Interno ---
    int volumenActual;

public:
    GestorDeMicrofono(int pBuzzer, int pLed, int pVol, int frec, int bclk, int ws, int data) {
        pinBuzzer = pBuzzer;
        pinLed = pLed;
        pinVolumen = pVol;
        frecuenciaPitido = frec;
        
        pinMicBCLK = bclk;
        pinMicWS = ws;
        pinMicDATA = data;
        
        volumenActual = 0;
    }

    void iniciar() {
        pinMode(pinLed, OUTPUT);
        digitalWrite(pinLed, LOW); 

        pinMode(pinBuzzer, OUTPUT);
        digitalWrite(pinBuzzer, LOW); 

        analogReadResolution(12);

        // TODO Aquí irá la inicialización del driver I2S: i2s_driver_install()
        Serial.println("Gestor de Micrófono y Feedback inicializado.");
    }

    // ==========================================
    // CONTROL DE VOLUMEN
    // ==========================================
    
    // Lee el potenciómetro, lo mapea a porcentaje (0-100) y lo devuelve
    int leerYActualizarVolumen() {
        int lecturaCruda = analogRead(pinVolumen);
        
        // Mapeamos el valor crudo (0 - 4095) a un porcentaje amigable (0 - 100)
        volumenActual = map(lecturaCruda, 0, 4095, 0, 100);
        
        return volumenActual;
    }

    // Devuelve el último volumen leído sin volver a consultar el hardware
    int obtenerVolumen() {
        return volumenActual;
    }

    // ==========================================
    // FEEDBACK VISUAL Y AUDITIVO
    // ==========================================

    void encenderLed() {
        digitalWrite(pinLed, HIGH);
    }

    void apagarLed() {
        digitalWrite(pinLed, LOW);
    }

    void encenderBuzzer() {
        // tone() genera una onda cuadrada a la frecuencia especificada
        tone(pinBuzzer, frecuenciaPitido);
    }

    void apagarBuzzer() {
        noTone(pinBuzzer);
    }

    // ==========================================
    // PLACEHOLDERS PARA EL AUDIO I2S
    // ==========================================

    void confirmarInicioGrabacion() {
        apagarLed(); // El LED avisa que se está grabando
        encenderBuzzer();         
        Serial.println("Boton de \"Grabar\" presionado");
        // TODO Lógica de lectura I2S y escritura en SD
    }
    void iniciarGrabacion() {
        encenderLed(); // El LED avisa que se está grabando
        apagarBuzzer();         
        Serial.println("Grabando audio...");
        // TODO Lógica de lectura I2S y escritura en SD
    }

    void detenerGrabacion() {
        apagarLed();
        Serial.println("Grabación detenida.");
        // TODO Lógica para cerrar el archivo WAV
    }
};