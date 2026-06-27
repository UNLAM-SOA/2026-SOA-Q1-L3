package com.soa2026.nonofono;
import org.eclipse.paho.client.mqttv3.*;
import javax.net.ssl.SSLSocketFactory;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;
import org.json.JSONException;
import org.json.JSONObject;

public class MQTTManager {

    private MqttClient client;
    private static MQTTManager instance;

    private String ip;

    public static synchronized MQTTManager getInstance() {

        if (instance == null)
            instance = new MQTTManager();

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
            //TODO Notificar a todo el mundo por mensaje o notificacion
        }
        if(evento.equals("ip"))
        {
            ip = jsonContenido.getString("ip");
            System.out.println("IP: " + ip);
        }
        if (evento.equals("audio")) {
            String audio = jsonContenido.getString("audio");
            AudioDownloader.descargar(ip,audio);
            System.out.println("Mensaje recibido :D");
        }

        System.out.println("Mensaje: " + mensaje);
    }
}