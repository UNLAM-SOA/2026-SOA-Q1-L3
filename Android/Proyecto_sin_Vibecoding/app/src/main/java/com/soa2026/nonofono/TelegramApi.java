package com.soa2026.nonofono;

import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;

public class TelegramApi {
    private final String token;
    private final String urlBase;

    public TelegramApi(String token) {
        this.token = token;
        this.urlBase = "https://api.telegram.org/bot" + token + "/";
    }

    /**
     * Envía un mensaje al chat indicado.
     */
    public String enviarMensaje(String idChat, String mensaje) {

        try {

            mensaje = URLEncoder.encode(mensaje, "UTF-8");

            URL url = new URL(urlBase +
                    "sendMessage?chat_id=" +
                    idChat +
                    "&text=" +
                    mensaje);

            HttpURLConnection conexion =
                    (HttpURLConnection) url.openConnection();

            conexion.setRequestMethod("GET");

            BufferedReader br = new BufferedReader(
                    new InputStreamReader(conexion.getInputStream()));

            StringBuilder respuesta = new StringBuilder();

            String linea;

            while ((linea = br.readLine()) != null) {
                respuesta.append(linea);
            }

            br.close();
            conexion.disconnect();

            return respuesta.toString();

        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    /**
     * Obtiene todos los mensajes pendientes.
     */
    public String getUpdates() {

        try {

            URL url = new URL(urlBase + "getUpdates");

            HttpURLConnection conexion =
                    (HttpURLConnection) url.openConnection();

            conexion.setRequestMethod("GET");

            BufferedReader br = new BufferedReader(
                    new InputStreamReader(conexion.getInputStream()));

            StringBuilder respuesta = new StringBuilder();

            String linea;

            while ((linea = br.readLine()) != null) {
                respuesta.append(linea);
            }

            br.close();
            conexion.disconnect();

            return respuesta.toString();

        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    /**
     * Obtiene solo los mensajes posteriores al offset.
     */
    public String getUpdates(long offset) {

        try {

            URL url = new URL(urlBase +
                    "getUpdates?offset=" +
                    offset);

            HttpURLConnection conexion =
                    (HttpURLConnection) url.openConnection();

            conexion.setRequestMethod("GET");

            BufferedReader br = new BufferedReader(
                    new InputStreamReader(conexion.getInputStream()));

            StringBuilder respuesta = new StringBuilder();

            String linea;

            while ((linea = br.readLine()) != null) {
                respuesta.append(linea);
            }

            br.close();
            conexion.disconnect();

            return respuesta.toString();

        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    public String enviarAudio(String chatId, File audio) {

        String boundary = "===" + System.currentTimeMillis() + "===";

        try {

            URL url = new URL(urlBase + "sendAudio");

            HttpURLConnection conn =
                    (HttpURLConnection) url.openConnection();

            conn.setRequestMethod("POST");
            conn.setDoOutput(true);
            conn.setDoInput(true);
            conn.setUseCaches(false);

            conn.setRequestProperty(
                    "Content-Type",
                    "multipart/form-data; boundary=" + boundary);

            DataOutputStream request =
                    new DataOutputStream(conn.getOutputStream());

            // chat_id
            request.writeBytes("--" + boundary + "\r\n");
            request.writeBytes(
                    "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n");
            request.writeBytes(chatId + "\r\n");

            // Archivo
            request.writeBytes("--" + boundary + "\r\n");
            request.writeBytes(
                    "Content-Disposition: form-data; name=\"audio\"; filename=\"" +
                            audio.getName() + "\"\r\n");

            request.writeBytes(
                    "Content-Type: audio/mpeg\r\n\r\n");

            FileInputStream inputStream =
                    new FileInputStream(audio);

            byte[] buffer = new byte[4096];

            int bytesRead;

            while ((bytesRead = inputStream.read(buffer)) != -1) {

                request.write(buffer, 0, bytesRead);

            }

            inputStream.close();

            request.writeBytes("\r\n");

            request.writeBytes("--" + boundary + "--\r\n");

            request.flush();
            request.close();

            BufferedReader br =
                    new BufferedReader(
                            new InputStreamReader(conn.getInputStream()));

            StringBuilder respuesta = new StringBuilder();

            String linea;

            while ((linea = br.readLine()) != null) {

                respuesta.append(linea);

            }

            br.close();
            conn.disconnect();

            return respuesta.toString();

        } catch (Exception e) {

            e.printStackTrace();
            return null;

        }
    }
}