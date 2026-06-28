package com.soa2026.nonofono;

import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import androidx.appcompat.app.AppCompatActivity;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

public class MainActivity extends AppCompatActivity {

    TextView txtTitulo, txtIcono, txtDescripcion, txtEstado;
    Button btnAgregarContacto, btnLocalizarDispositivo;
    TelegramApi telegram;

    private MQTTManager mqttManager;

    private final ExecutorService executor =
            Executors.newSingleThreadExecutor();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        txtTitulo = findViewById(R.id.txtTitulo);
        txtIcono = findViewById(R.id.txtIcono);
        txtDescripcion = findViewById(R.id.txtDescripcion);
        txtEstado = findViewById(R.id.txtEstado);
        btnAgregarContacto = findViewById(R.id.btnAgregarContacto);
        btnLocalizarDispositivo = findViewById(R.id.btnLocalizarDispositivo);

        btnAgregarContacto.setOnClickListener(v -> startActivity(new Intent(
                MainActivity.this,
                AgregarContactoActivity.class)));

        btnLocalizarDispositivo.setOnClickListener(v -> startActivity(new Intent(
                MainActivity.this,
                LocalizarDispositivoActivity.class)));

        mqttManager = MQTTManager.getInstance(this);

        conectarMQTT();
        telegram = new TelegramApi("8322654023:AAHhtylDzV06H71m-Xko3mhbg2TQ3J5brdA");
    }

    @Override
    protected void onStart() {
        super.onStart();

        new Thread(() -> {
            //Recuperamos el conjunto de contactos
            ArrayList<Contacto> contactos = ContactosManager.cargar(this);

            SharedPreferences prefs = getSharedPreferences("offsetAPI", MODE_PRIVATE);
            long offset = prefs.getLong("offset", 0);
            HashMap<String, Long> telefonosChatId = new HashMap<>();
            HashSet<Long> idsAsociadas = new HashSet<>();
            HashSet<Long> chatIds = new HashSet<>();

            for (Contacto cont : contactos){
                Long chatId = cont.getChatId();

                if(chatId != null) {
                    idsAsociadas.add(chatId);
                }
            }

            String json = telegram.getUpdates(offset);

            System.out.println(json);

            try {
                JSONObject objeto = new JSONObject(json);
                JSONArray resultados = objeto.getJSONArray("result");
                long updateId = 0;
                String regexTelefono = "^\\+?\\d{8,15}$";

                for (int i = 0; i < resultados.length(); i++) {
                    JSONObject update = resultados.getJSONObject(i);
                    updateId = update.getLong("update_id");

                    JSONObject message = update.getJSONObject("message");

                    long chatId = message.getJSONObject("chat").getLong("id");

                    String texto = message.getString("text");

                    if (!idsAsociadas.contains(chatId) && texto.trim().matches(regexTelefono)) {
                        telefonosChatId.put(texto.replace(" ", ""), chatId);
                    }

                    chatIds.add(chatId);
                }

                Set<Long> idsConNumero = new HashSet<>(telefonosChatId.values());
                chatIds.removeAll(idsConNumero);
                chatIds.removeAll(idsAsociadas);

                for (long id : chatIds) {
                    telegram.enviarMensaje(String.valueOf(id), "Por favor, envíe un mensaje que SOLAMENTE CONTENGA su número telefónico");
                }

                for (long id : idsConNumero) {
                    telegram.enviarMensaje(String.valueOf(id), "Gracias por colaborar, ya puede recibir mensajes y noticias del abuelo");
                }

                for (Contacto cont : contactos) {
                    Long chatId = telefonosChatId.getOrDefault(cont.getTelefono(), null);

                    if (chatId != null) {
                        System.out.println("CONTACTO CON CHATID ACTUALIZADO: " + cont.getNombre());
                        cont.setChatId(chatId);
                    }
                }

                prefs.edit().putLong("offset", updateId + 1).apply();
            } catch (JSONException ex) {
                System.out.println("Error con la API de Telegram D:");
            }

            ContactosManager.guardar(this, contactos);
        }).start();
    }

    private void conectarMQTT() {

        executor.execute(() -> {
            try {
                mqttManager.conectar();
                mqttManager.suscribirse();
            } catch (Exception e) {
                System.err.println("Error principal: " + e.getMessage());
                if (e.getCause() != null) {
                    System.err.println("Causa raíz (IMPORTANTE): " + e.getCause().getMessage());
                    e.getCause().printStackTrace();
                }
            }
        });
    }
}