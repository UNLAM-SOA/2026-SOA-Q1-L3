package com.soa2026.nonofono;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;

public class NonofonoService extends Service {

    private static final String CHANNEL_ID = "NonofonoChannel";
    private MQTTManager mqttManager;

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i("NonofonoService", "Servicio creado. Inicializando MQTT...");

        // El servicio ES un Contexto, así que se lo pasamos al Manager
        mqttManager = MQTTManager.getInstance(this);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // 1. Crear la notificación obligatoria
        crearCanalNotificacion();
        Notification notificacion = new NotificationCompat.Builder(this, CHANNEL_ID)
                .setContentTitle("Nonófono Activo")
                .setContentText("Escuchando emergencias en segundo plano...")
                .setSmallIcon(R.mipmap.ic_launcher) // Cambialo por tu ícono
                .setPriority(NotificationCompat.PRIORITY_LOW)
                .build();

        // 2. Iniciar como Foreground Service
        startForeground(1, notificacion);

        // 3. Conectar el MQTT (en un hilo separado para no bloquear el servicio)
        new Thread(() -> {
            try {
                mqttManager.conectar();
                mqttManager.suscribirse();
                Log.i("NonofonoService", "MQTT Conectado y suscrito desde el Servicio");
            } catch (Exception e) {
                Log.e("NonofonoService", "Error iniciando MQTT: " + e.getMessage());
            }
        }).start();

        // START_STICKY hace que el servicio reviva si el sistema lo mata por falta de memoria
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.w("NonofonoService", "Servicio destruido");
        // Acá podrías agregar una lógica para desconectar MQTT limpiamente si el usuario cierra la app a propósito
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        // No usamos "Binding" en este diseño, devolvemos null
        return null;
    }

    private void crearCanalNotificacion() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel serviceChannel = new NotificationChannel(
                    CHANNEL_ID,
                    "Canal de Servicio Nonófono",
                    NotificationManager.IMPORTANCE_LOW // Low para que no haga ruido todo el tiempo
            );
            NotificationManager manager = getSystemService(NotificationManager.class);
            if (manager != null) {
                manager.createNotificationChannel(serviceChannel);
            }
        }
    }
}