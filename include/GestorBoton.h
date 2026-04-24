#include <Arduino.h>
class GestorBoton {
  private:
    int pin;
    unsigned long tiempoRequerido;
    unsigned long tiempoInicioPresion;
    bool botonEstaPresionado;

  public:
    // Constructor: inicializa el gestor con el pin y el tiempo específico
    GestorBoton(int pinAsignado, unsigned long tiempoAsignado) {
        pin = pinAsignado;
        tiempoRequerido = tiempoAsignado;
        tiempoInicioPresion = 0;
        botonEstaPresionado = false;
    }

    // Método principal con la lógica de negocio del botón
    bool estaMantenido() {
        int lecturaBoton = digitalRead(pin);

        if (lecturaBoton == LOW) { // Asumiendo lógica pull-up (presionado = LOW)
            if (!botonEstaPresionado) {
                // Se acaba de presionar
                botonEstaPresionado = true;
                tiempoInicioPresion = millis();
            } else {
                // Ya estaba presionado, verificamos el cronómetro
                if (millis() - tiempoInicioPresion >= tiempoRequerido) {
                    botonEstaPresionado = false; // Reiniciamos para evitar múltiples gatillos
                    return true;                 // ¡Se cumplió el tiempo!
                }
            }
        } else {
            // El botón se soltó, reiniciamos el estado
            botonEstaPresionado = false;
        }
        
        return false; // Aún no se cumplió el tiempo o no está presionado
    }
};