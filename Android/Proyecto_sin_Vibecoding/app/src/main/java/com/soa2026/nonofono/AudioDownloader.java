package com.soa2026.nonofono;
import android.content.Context;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

public class AudioDownloader {

    public static boolean descargar(String ip, String ruta, String archivo) {

        // 1. Declaramos la conexión y el archivo ANTES del try
        HttpURLConnection conn = null;
        File file = new File(ruta, archivo);

        try {
            String urlStr = "http://" + ip + "/descargar_audio";
            Log.d("AudioDownloader", "Descargando desde: " + urlStr);

            URL url = new URL(urlStr);
            conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("GET");
            conn.connect();

            if (conn.getResponseCode() != 200) {
                Log.e("AudioDownloader", "Error HTTP: " + conn.getResponseCode());
                return false;
            }

            // 2. ACÁ arranca el Try-with-resources que cierra los flujos automáticamente
            try (InputStream input = conn.getInputStream();
                 FileOutputStream output = new FileOutputStream(file)) {

                byte[] buffer = new byte[4096];
                int len;
                while ((len = input.read(buffer)) != -1) {
                    output.write(buffer, 0, len);
                }

                Log.d("AudioDownloader", "Audio descargado correctamente");
                return true;
            }

        } catch (Exception e) {
            Log.e("AudioDownloader", "Error descargando audio", e);

            // Si falló a la mitad, borramos el archivo corrupto para que no ocupe espacio
            if (file.exists()) {
                file.delete();
            }
            return false;

        } finally {
            // 3. La conexión HTTP la cerramos manualmente pase lo que pase
            if (conn != null) {
                conn.disconnect();
            }
        }
    }

}