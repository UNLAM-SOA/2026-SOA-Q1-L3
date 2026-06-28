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

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_agregar_contacto);

        contactos = ContactosManager.cargar(this);
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

        btnAgregar.setOnClickListener(v -> {

            if (contactos.size() < 10) {
                String nombre = etNombre.getText().toString().trim();
                nombre = nombre.substring(0, 1).toUpperCase() + nombre.substring(1).toLowerCase();
                String telefono = etTelefono.getText().toString().trim().replace(" ", "");
                String regexTelefono = "^\\+?\\d{8,15}$";

                if (!nombre.isEmpty() && !telefono.isEmpty() && telefono.matches(regexTelefono)) {
                    contactos.add(new Contacto(nombre, telefono));

                    adapter.notifyItemInserted(contactos.size() - 1);

                    etNombre.setText("");
                    etTelefono.setText("");
                }
            }
        });
    }

    @Override
    protected void onPause(){
        super.onPause();
        ContactosManager.guardar(this, contactos);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }
}
