package com.soa2026.nonofono;

import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {

    TextView txtTitulo, txtIcono, txtDescripcion, txtEstado;
    Button btnAgregarContacto, btnLocalizarDispositivo;

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

        mqttManager = MQTTManager.getInstance();

        conectarMQTT();
    }

    @Override
    protected void onStart() {
        super.onStart();
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