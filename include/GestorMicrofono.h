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

    //Para almacenar el audio
    GestorAlmacenamiento *gestorAlmacenamiento;

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

    void setAlmacenamiento(GestorAlmacenamiento *gestor){
        gestorAlmacenamiento = gestor;
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
    void leerYActualizarVolumen() {
        int lecturaCruda = analogRead(pinVolumen);
        
        // Mapeamos el valor crudo (0 - 4095) a un porcentaje amigable (0 - 100)
        volumenActual = map(lecturaCruda, 0, 4095, 0, 100);
        
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
        // 3. Mapeamos el porcentaje (1-100) a un rango de frecuencias audibles.
        // 500 Hz es un tono grave (volumen bajo) y 2500 Hz es agudo (volumen alto).
        int frecuenciaDinamica = map(volumenActual, 1, 100, 500, 2500);
        
        // 4. Emitimos el tono calculado
        tone(pinBuzzer, frecuenciaDinamica);
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

    void iniciarGrabacion(const String ruta) {
        encenderLed(); // El LED avisa que se está grabando
        apagarBuzzer();         
        Serial.println("Grabando audio...");
        // TODO Lógica de lectura I2S y escritura en SD
        gestorAlmacenamiento->abrirArchivo(ruta);
    }

    //Como no tenemos micrófono, tomamos una medida del potenciómetro que lo simula en cada ciclo de la FSM
    void registrarMedida()
    {
        uint16_t lectura = analogRead(pinMicDATA);
        gestorAlmacenamiento->guardarDato((uint8_t*)&lectura);
    }

    void detenerGrabacion() {
        apagarLed();
        gestorAlmacenamiento->cerrarArchivo();
        Serial.println("Grabación detenida.");
    }
};