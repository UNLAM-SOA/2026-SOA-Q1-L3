package com.soa2026.nonofono;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.widget.Button;
import android.widget.EditText;

import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;

public class AgregarContactoActivity extends AppCompatActivity {
    private ArrayList<Contacto> contactos;
    private ContactoAdapter adapter;

    SharedPreferences prefs;

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_agregar_contacto);

        contactos = new ArrayList<>();
        adapter = new ContactoAdapter(contactos, position -> {

            contactos.remove(position);
            adapter.notifyItemRemoved(position);
        });

        EditText etNombre = findViewById(R.id.etNombre);
        EditText etTelefono = findViewById(R.id.etTelefono);
        Button btnAgregar = findViewById(R.id.btnAgregar);
        RecyclerView recycler = findViewById(R.id.recyclerView);

        recycler.setLayoutManager(new LinearLayoutManager(this));
        recycler.setAdapter(adapter);

        SharedPreferences prefs = getSharedPreferences("contactos", MODE_PRIVATE);
        Set<String> contactosSet = prefs.getStringSet("Contactos", null);

        if(contactosSet != null){
            for (String cont : contactosSet){
                String[] pedazos = cont.split(";");

                String nombre = pedazos[0], numero = pedazos[1];

                Contacto nuevo = new Contacto(nombre, numero);
                contactos.add(nuevo);
                adapter.notifyItemInserted(contactos.size() - 1);
            }
        }

        btnAgregar.setOnClickListener(v -> {

            if (contactos.size() < 10) {
                String nombre = etNombre.getText().toString().trim();
                nombre = nombre.substring(0, 1).toUpperCase() + nombre.substring(1).toLowerCase();
                String telefono = etTelefono.getText().toString().trim();

                if (!nombre.isEmpty() && !telefono.isEmpty()) {
                    contactos.add(new Contacto(nombre, telefono));

                    adapter.notifyItemInserted(contactos.size() - 1);

                    etNombre.setText("");
                    etTelefono.setText("");
                }
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        SharedPreferences prefs = getSharedPreferences("contactos", MODE_PRIVATE);
        Set<String> contactosSet = new HashSet<>();

        for (Contacto cont : contactos){
            contactosSet.add(cont.getNombre() + ";" + cont.getTelefono());
        }

        SharedPreferences.Editor editor = prefs.edit();
        editor.putStringSet("Contactos", contactosSet);
        editor.apply();
    }
}
