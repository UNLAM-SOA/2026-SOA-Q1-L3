package com.soa2026.nonofono;

import android.content.Context;
import android.content.SharedPreferences;

import java.lang.reflect.Type;
import java.util.ArrayList;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;

public class ContactosManager {
    private static final String PREFS = "contactos";
    private static final String KEY = "lista_contactos";

    public static ArrayList<Contacto> cargar(Context context) {

        ArrayList<Contacto> contactos = new ArrayList<>();

        SharedPreferences prefs =
                context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);

        String json = prefs.getString(KEY, null);

        if (json == null)
            return contactos;

        try {

            JSONArray array = new JSONArray(json);

            for (int i = 0; i < array.length(); i++) {

                JSONObject obj = array.getJSONObject(i);

                String nombre = obj.getString("nombre");
                String telefono = obj.getString("telefono");
                long chatId = obj.getLong("chatId");

                contactos.add(
                        new Contacto(nombre, telefono, chatId));
            }

        } catch (Exception e) {
            e.printStackTrace();
        }

        return contactos;
    }

    public static void guardar(Context context,
                               ArrayList<Contacto> contactos) {

        JSONArray array = new JSONArray();

        try {

            for (Contacto c : contactos) {

                JSONObject obj = new JSONObject();

                obj.put("nombre", c.getNombre());
                obj.put("telefono", c.getTelefono());
                obj.put("chatId", c.getChatId());

                array.put(obj);
            }

            SharedPreferences prefs =
                    context.getSharedPreferences(PREFS,
                            Context.MODE_PRIVATE);

            prefs.edit()
                    .putString(KEY, array.toString())
                    .apply();

        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static void agregar(Context context,
                               Contacto contacto) {

        ArrayList<Contacto> contactos = cargar(context);

        contactos.add(contacto);

        guardar(context, contactos);
    }

    public static void eliminar(Context context,
                                int posicion) {

        ArrayList<Contacto> contactos = cargar(context);

        if (posicion >= 0 && posicion < contactos.size()) {

            contactos.remove(posicion);

            guardar(context, contactos);
        }
    }

    public static void limpiar(Context context) {

        SharedPreferences prefs =
                context.getSharedPreferences(PREFS,
                        Context.MODE_PRIVATE);

        prefs.edit()
                .remove(KEY)
                .apply();
    }
}