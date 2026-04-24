// GestorDeRed.h
#include <Arduino.h>

class GestorDeRed {
public:
    // 1. Creamos el ENUM público. Estas son las "respuestas" oficiales de esta clase.
    enum EstadoRespuesta {
        REPOSO,   // No está haciendo nada
        ENVIANDO, // Ocupado peleando con el Wi-Fi
        EXITO,    // ¡El mensaje llegó!
        ERROR     // Falló el Wi-Fi o hubo Timeout
    };

private:
    EstadoRespuesta estadoActual = REPOSO;
    unsigned long tiempoInicioPeticion = 0;
    const unsigned long TIMEOUT_WIFI = 5000;

public:
    // Esta función sigue siendo void porque solo da la orden de arranque
    void iniciarEnvioMensaje() {
        if (estadoActual == REPOSO || estadoActual == EXITO || estadoActual == ERROR) {
            estadoActual = ENVIANDO;
            tiempoInicioPeticion = millis();
        }
    }

    // 🌟 AQUÍ ESTÁ EL CAMBIO: Ya no es void. Devuelve un 'EstadoRespuesta'
    EstadoRespuesta actualizar() {
        // Si estamos enviando, calculamos qué está pasando
        if (estadoActual == ENVIANDO) {
            unsigned long tiempoTranscurrido = millis() - tiempoInicioPeticion;

            if (hayRespuestaDelServidor()) {
                estadoActual = EXITO;
            } 
            else if (tiempoTranscurrido >= TIMEOUT_WIFI) {
                estadoActual = ERROR;
            }
        }
        
        // Siempre devolvemos cómo está el gestor en este instante
        return estadoActual; 
    }

    // Función para "limpiar" el estado una vez que la FSM ya leyó el éxito o error
    void reconocerRespuesta() {
        estadoActual = REPOSO;
    }

private:
    bool hayRespuestaDelServidor() {
        // Simulación: 2% de probabilidad de éxito por cada ciclo
        if (random(0, 100) < 2) return true; 
        return false;
    }
};