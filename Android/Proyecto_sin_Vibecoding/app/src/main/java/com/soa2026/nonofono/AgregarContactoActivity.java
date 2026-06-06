package com.soa2026.nonofono;

import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TableLayout;
import android.widget.TableRow;
import android.widget.TextView;
import java.util.HashMap;
import java.util.Locale;

import androidx.appcompat.app.AppCompatActivity;

public class AgregarContactoActivity extends AppCompatActivity {
    private EditText etNombre;
    private EditText etTelefono;
    private Button btnAgregar;
    private TableLayout tabla;
    private HashMap<String, String> contactos;
    private TextView txtError;

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_agregar_contacto);
        etNombre = findViewById(R.id.etNombre);
        etTelefono = findViewById(R.id.etTelefono);
        btnAgregar = findViewById(R.id.btnAgregar);
        tabla = findViewById(R.id.tabla);
        txtError = findViewById(R.id.txtError);
        contactos = new HashMap<String, String>();

        btnAgregar.setOnClickListener(v -> {
            String nombre = etNombre.getText().toString().strip().toLowerCase();
            //Ponemos solo la primer letra en mayúscula
            String nro = etTelefono.getText().toString().strip();
            //TODO: Agregar validaciones sobre el nombre y el nro

            if (contactos.size() < 10 ) {
                if(!nombre.isEmpty() && !nro.isEmpty()) {
                    nombre = nombre.substring(0, 1).toUpperCase() + nombre.substring(1);

                    if (contactos.containsKey(nombre)){
                        txtError.setText("Ese nombre de contacto ya está registrado");
                    }
                    else {
                        txtError.setText("");

                        TableRow fila = new TableRow(this);
                        TextView celdaNombre = new TextView(this);
                        TextView celdaNro = new TextView(this);

                        celdaNombre.setText(nombre);
                        celdaNro.setText(nro);
                        fila.addView(celdaNombre);
                        fila.addView(celdaNro);
                        tabla.addView(fila);

                        contactos.put(nombre, nro);
                    }
                }
                else{
                    txtError.setText("Al menos uno de los campos está vacío");
                }
            }
            else{
                txtError.setText("No puede agregar más de 10 contactos");
            }
        });
    }
}
