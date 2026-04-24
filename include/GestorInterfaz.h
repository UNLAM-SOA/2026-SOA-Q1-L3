#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Objeto global de la pantalla (Dirección I2C habitual 0x27, 20 columnas, 4 filas)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ==========================================
// 🖥️ GESTOR DE INTERFAZ (LCD y Avisos)
// ==========================================
class GestorDeInterfaz {
public:
    // Configuración inicial
    void iniciar() {
        lcd.init();
        lcd.backlight();
    }

    // 1. Estado: Init
    void mostrarPantallaInit() {
        lcd.clear();
        lcd.setCursor(5, 0); lcd.print("SISTEMA DE");
        lcd.setCursor(5, 1); lcd.print("ASISTENCIA");
        lcd.setCursor(6, 2); lcd.print("NONOFONO");
        // Nota: Dejamos la fila 3 vacía o para una barra de carga futura
    }

    // 2. Estado: Idle
    void mostrarPantallaReposo() {
        lcd.clear();
        lcd.setCursor(1, 0); lcd.print("BIENVENIDO CARLOS");
        lcd.setCursor(0, 1); lcd.print("PRESIONE ARRIBA ^");
        lcd.setCursor(0, 2); lcd.print("O ABAJO v PARA");
        lcd.setCursor(0, 3); lcd.print("ENVIAR MENSAJES");
    }

    // 3. Estado: Navegando (Contactos)
    // Recibe un array de contactos y el índice actual para dibujar el cursor "<-"
void mostrarNavegandoContactos(String contactos[10], int indiceSeleccionado) {
    // 1. Limpiar y dibujar el encabezado estático
    lcd.clear();
    lcd.setCursor(0, 0); 
    lcd.print("SELECCIONE CONTACTO:");

    // 2. Calcular qué contacto va en la primera fila (indiceTop)
    // Si el seleccionado es menor a 3, la pantalla arranca desde el índice 0.
    // Si es 3 o mayor, empujamos la lista para que el seleccionado quede siempre al final.
    int indiceTop = 0;
    if (indiceSeleccionado >= 3) {
        indiceTop = indiceSeleccionado - 2;
    }

    // 3. Dibujar las 3 filas disponibles en el display
    for (int i = 0; i < 3; i++) {
        int indiceActual = indiceTop + i;
        
        // Evitar desbordamientos de memoria si llegamos al límite del arreglo
        if (indiceActual < 10) { 
            lcd.setCursor(0, i + 1);
            lcd.print(contactos[indiceActual]);
            
            // Dibujar el cursor si la fila actual coincide con la seleccionada
            if (indiceActual == indiceSeleccionado) {
                lcd.print(" <-"); 
            }
        }
    }
}

    // 4. Estado: Confirmar contacto
    void mostrarConfirmarContacto(String contactoSeleccionado) {
        lcd.clear();
        lcd.setCursor(0, 1); lcd.print("CONTACTO ELEGIDO:");
        lcd.setCursor(0, 2); lcd.print(contactoSeleccionado);
    }

    // 5. Estado: Grabando
    void mostrarGrabando(String contactoSeleccionado) {
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("* GRABANDO... *");
        lcd.setCursor(0, 1); lcd.print("PARA:");
        lcd.setCursor(0, 2); lcd.print(contactoSeleccionado);
    }

    // 6. Estado: Confirmar mensaje
    void mostrarConfirmarMensaje() {
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print(char(168)); // Símbolo '¿' en ASCII extendido
        lcd.print("DESEA CONFIRMAR");
        lcd.setCursor(0, 1); lcd.print("EL MENSAJE?");
        lcd.setCursor(0, 2); lcd.print("   CONFIRMAR");
        lcd.setCursor(0, 3); lcd.print("   CANCELAR");
    }

    // 7. Estado: Mensaje Predefinido (Navegación secundaria)
void mostrarMensajesPredefinidos(String contacto, String mensajes[3], int indiceSeleccionado) {
    lcd.clear();
    
    // 1. Dibujamos la cabecera
    lcd.setCursor(0, 0); 
    lcd.print("ENVIAR A ");
    lcd.print(contacto);
    lcd.print(":");
    
    // 2. Seguridad de memoria: Aseguramos que el índice nunca se salga del rango 0-2
    // La función constrain() de Arduino es perfecta para esto.
    int indiceSeguro = constrain(indiceSeleccionado, 0, 2);

    // 3. Dibujamos las 3 opciones estáticas y el cursor dinámico
    for (int i = 0; i < 3; i++) {
        lcd.setCursor(0, i + 1);
        
        lcd.print(mensajes[i]);
        if (i == indiceSeguro) {
            lcd.print("<-"); // Agregué un espacio extra para que no quede pegado al texto
        } else {
            lcd.print("  "); // Mantenemos la misma cantidad de espacios en blanco
        }
        
    }
}

    // 8. Estado: EMERGENCIA (Prioridad absoluta)
    void mostrarEmergencia() {
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("!!! EMERGENCIA !!!");
        lcd.setCursor(0, 1); lcd.print("LLAMANDO A");
        lcd.setCursor(0, 2); lcd.print("EMERGENCIAS Y A");
        lcd.setCursor(0, 3); lcd.print("SUS FAMILIARES");
    }
    
    // Método utilitario: Limpiar pantalla rápido
    void limpiarPantalla() {
        lcd.clear();
    }
    void mostrarEnviando() {
        lcd.clear();
        lcd.setCursor(2, 0); // Centrado aproximado
        lcd.print("ENVIANDO MENSAJE");
        
        lcd.setCursor(0, 2);
        lcd.print(" POR FAVOR, ESPERE");
        
        lcd.setCursor(8, 3);
        lcd.print("...");
    }

    void mostrarExito() {
        lcd.clear();
        
        // Dejamos la primera y última línea vacías para enmarcar el texto
        // y que sea más fácil de leer.
        lcd.setCursor(2, 1);
        lcd.print("MENSAJE ENVIADO");
        
        lcd.setCursor(4, 2);
        lcd.print("CON EXITO!");
    }

    void mostrarError() {
        lcd.clear();
        
        lcd.setCursor(2, 0);
        lcd.print("!!! ATENCION !!!");
        
        lcd.setCursor(0, 1);
        lcd.print("NO SE PUDO ENVIAR");
        
        lcd.setCursor(0, 2);
        lcd.print("EL MENSAJE.");
        
        lcd.setCursor(0, 3);
        lcd.print("INTENTE DE NUEVO.");
    }
};

// Instancia global del gestor para usar en el loop()
GestorDeInterfaz gestorUI;