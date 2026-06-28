package com.soa2026.nonofono;
import android.content.Context;

import org.eclipse.paho.client.mqttv3.*;
import javax.net.ssl.SSLSocketFactory;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.ArrayList;

public class MQTTManager {

    private MqttClient client;
    private static MQTTManager instance;
    private TelegramApi apiTelegram;
    private Context contexto;
    private String ip;

    public MQTTManager(Context contexto){
        apiTelegram = new TelegramApi("8322654023:AAHhtylDzV06H71m-Xko3mhbg2TQ3J5brdA");
        this.contexto = contexto;
    }

    public static synchronized MQTTManager getInstance(Context contexto) {

        if (instance == null)
            instance = new MQTTManager(contexto);

        return instance;
    }

    public void conectar() throws MqttException {

        if (client != null && client.isConnected())
            return;

        String broker =
                "ssl://e10a0f3769d14449b0472d6e60e344a9.s1.eu.hivemq.cloud:8883";

        MemoryPersistence persistence = new MemoryPersistence();

        client = new MqttClient(
                broker,
                MqttClient.generateClientId(),
                persistence
        );

        MqttConnectOptions options =
                new MqttConnectOptions();

        options.setAutomaticReconnect(true);
        options.setCleanSession(false);
        options.setUserName("admin_prueba");
        options.setPassword("Nonofono8".toCharArray());

        options.setSocketFactory(SSLSocketFactory.getDefault());

        client.setCallback(new MqttCallback() {

            @Override
            public void connectionLost(Throwable cause) {
                System.out.println("MQTT desconectado");
            }

            @Override
            public void messageArrived(String topic, MqttMessage message) throws JSONException {

                String payload =
                        new String(message.getPayload());

                procesarMensaje(payload);
            }

            @Override
            public void deliveryComplete(IMqttDeliveryToken token) {}
        });

        client.connect(options);
    }

    public void suscribirse() throws MqttException {

        if (client == null || !client.isConnected())
            return;

        client.subscribe("nonofono/ip");
        client.subscribe("nonofono/emergencias");
        client.subscribe("nonofono/mensajes/audio");
        client.subscribe("nonofono/mensajes/predeterminado");
    }

    public void publicar(String topic, String mensaje) throws MqttException {

        if (client == null || !client.isConnected()) {
            throw new IllegalStateException("MQTT no está conectado");
        }

        MqttMessage mqttMessage = new MqttMessage();
        mqttMessage.setPayload(mensaje.getBytes());
        mqttMessage.setQos(1);          // Entrega al menos una vez
        mqttMessage.setRetained(false); // No guardar el mensaje en el broker

        client.publish(topic, mqttMessage);
    }

    private void procesarMensaje(String mensaje) throws JSONException {

        JSONObject json =
                new JSONObject(mensaje);

        String evento =
                json.getString("evento");

        String contenido = json.getString("contenido");
        JSONObject jsonContenido =
                new JSONObject(contenido);

        if(evento.equals("emergencia"))
        {
            String mensajeEmergencia = jsonContenido.getString("mensaje");

            new Thread(() -> {
                ArrayList<Contacto> contactos = ContactosManager.cargar(contexto);

                for(Contacto cont : contactos){
                    Long chatId = cont.getChatId();

                    if(chatId != null){
                        apiTelegram.enviarMensaje(String.valueOf(chatId), mensajeEmergencia);
                    }
                }

            }).start();
        }
        if(evento.equals("ip"))
        {
            ip = jsonContenido.getString("ip");
            System.out.println("IP: " + ip);
        }
        if (evento.equals("audio")) {
            String audio = jsonContenido.getString("audio");

            new Thread(() -> {
                AudioDownloader.descargar(ip,"/data/data/com.soa2026.nonofono/files/", audio);
                System.out.println("Mensaje recibido :D");

                File archAudio = new File("/data/data/com.soa2026.nonofono/files/", audio);
                String telefono = null;
                try {
                    telefono = jsonContenido.getString("destinatario");
                    Contacto destino = buscarContacto(telefono);

                    if (destino == null){
                        System.err.println("No se encontró el destinatario del mensaje");
                    } else {
                        JSONObject respuesta = new JSONObject(apiTelegram.enviarAudio(
                                String.valueOf(destino.getChatId()),
                                archAudio));


                        if(respuesta.getBoolean("ok")){
                            publicar("nonofono/mensajes/audio/ack", "{\"exito\":\"Ok\"}");
                        }

                        System.out.println(respuesta);
                    }
                } catch (JSONException | MqttException e) {
                    throw new RuntimeException(e);
                }
            }).start();
        }
        if(evento.equals("mensaje-predefinido")){
            String telefono = jsonContenido.getString("telefono");
            String msj = jsonContenido.getString("contenido");

            Contacto destino = buscarContacto(telefono);

            if (destino != null){
                JSONObject respuesta = new JSONObject(apiTelegram.enviarMensaje(String.valueOf(destino.getChatId()), msj));

                if (respuesta.getBoolean("ok")){
                    try {
                        publicar("nonofono/mensajes/predeterminado/ack", "{\"exito\":\"Ok\"}");
                    } catch (MqttException e) {
                        throw new RuntimeException(e);
                    }
                }
            } else {
                System.err.println("No se encontró el destinatario del mensaje");
            }
        }

        System.out.println("Mensaje: " + mensaje);
    }

    private Contacto buscarContacto(String telefono){
        ArrayList<Contacto> contactos = ContactosManager.cargar(contexto);
        int i = 0, tam = contactos.size();

        while(i < tam && !contactos.get(i).getTelefono().equals(telefono)){
            i++;
        }

        if (i < tam)
            return contactos.get(i);

        return null;
    }
}