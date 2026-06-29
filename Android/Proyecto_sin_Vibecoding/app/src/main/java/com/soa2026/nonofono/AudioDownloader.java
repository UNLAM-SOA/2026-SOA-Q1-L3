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

        try {

            String urlStr =
                    "http://" + ip + "/descargar_audio";

            Log.d("HTTP", "Descargando desde: " + urlStr);

            URL url = new URL(urlStr);
            HttpURLConnection conn =
                    (HttpURLConnection) url.openConnection();

            conn.setRequestMethod("GET");
            conn.connect();

            if (conn.getResponseCode() != 200) {
                Log.e("HTTP", "Error HTTP: " + conn.getResponseCode());
                return false; // ¡Avisamos que falló!
            }

            InputStream input = conn.getInputStream();

            File file = new File(
                    ruta,
                    archivo
            );

            FileOutputStream output =
                    new FileOutputStream(file);

            byte[] buffer = new byte[4096];
            int len;

            while ((len = input.read(buffer)) != -1) {
                output.write(buffer, 0, len);
            }

            output.close();
            input.close();

            Log.d("HTTP", "Audio descargado correctamente");

            return true; // ¡Éxito!
        } catch (Exception e) {
            Log.e("HTTP", "Error descargando audio", e);
            return false; // Hubo un error de red
        }

}}