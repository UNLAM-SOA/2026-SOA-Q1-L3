#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

#define TAMANO_DATO 2
#define AJUSTE_POR_POS 256

class GestorAlmacenamiento
{
    private:
    //Pines tarjeta SD
    int pinCS, pinSCK, pinMISO, pinMOSI;
    File archivoAbierto;
    String ruta;

    public:
    GestorAlmacenamiento(int SD_CS, int SD_SCK, int SD_MISO, int SD_MOSI)
    {
        pinCS = SD_CS;
        pinSCK = SD_SCK;
        pinMISO = SD_MISO;
        pinMOSI = SD_MOSI;
    }

    bool iniciarSD()
    {
        SPI.begin(pinSCK, pinMISO, pinMOSI, pinCS);

        return SD.begin(pinCS);
    }

    void abrirArchivo(const String archivo)
    {
        if (archivoAbierto)
            return;

        archivoAbierto = SD.open(archivo, FILE_WRITE);
        ruta = archivo;
    }

    void guardarDato(const uint8_t *dato)
    {
        //Escribimos en bloques de 16 bits porque nos conviene
        archivoAbierto.write(dato, TAMANO_DATO);
        archivoAbierto.flush(); //Limpiar el buffer obliga que los datos se escriban
    }
    
    void cerrarArchivo()
    {
        archivoAbierto.close();
    }

    //Al no poder enviar mensajes por wi-fi, nos conformamos con leer los datos
    void leerArchivo()
    {
        //Nuevamente, leemos en bloques de 16 bits porque así escribimos 
        //(es arbitrario, es el tamaño que leemos del potenciómetro)
        //Si vamos a leer el archivo, hay que abrirlo para solo lectura
        File archivoLeer = SD.open(ruta, FILE_READ);

        uint8_t lectura[TAMANO_DATO];
        uint16_t datoFinal;
        while (archivoLeer.available())
        {
            archivoLeer.read(lectura, TAMANO_DATO);
            datoFinal = lectura[0] + lectura[1] * AJUSTE_POR_POS;
            //Mostramos el "audio" guardado en el archivo
            Serial.printf("%d\n\r", datoFinal);
        }

        archivoLeer.close();
    }

void eliminarArchivo()
    {
        if (ruta.length() > 1) 
        {
            if (SD.exists(ruta))
            {
                SD.remove(ruta);
            }
            ruta = ""; 
        }
    }

    ~GestorAlmacenamiento(){
        if (archivoAbierto){
            archivoAbierto.close();
        }

        if (ruta.length() > 1) 
        {
            if (SD.exists(ruta))
            {
                SD.remove(ruta);
            }
        }
    }
};