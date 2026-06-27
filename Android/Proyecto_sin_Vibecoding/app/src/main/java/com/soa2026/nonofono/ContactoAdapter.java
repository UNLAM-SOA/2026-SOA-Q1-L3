package com.soa2026.nonofono;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;

public class ContactoAdapter extends RecyclerView.Adapter<ContactoAdapter.ViewHolder> {

    public interface OnDeleteClickListener {
        void onDelete(int position);
    }

    private ArrayList<Contacto> lista;
    private OnDeleteClickListener listener;

    public ContactoAdapter(ArrayList<Contacto> lista,
                           OnDeleteClickListener listener) {
        this.lista = lista;
        this.listener = listener;
    }

    public static class ViewHolder extends RecyclerView.ViewHolder {

        TextView nombre;
        TextView telefono;
        Button eliminar;

        public ViewHolder(View itemView) {
            super(itemView);

            nombre = itemView.findViewById(R.id.tvNombre);
            telefono = itemView.findViewById(R.id.tvTelefono);
            eliminar = itemView.findViewById(R.id.btnEliminar);
        }
    }

    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(
            @NonNull ViewGroup parent,
            int viewType) {

        View view = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.contacto_layout, parent, false);

        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(
            @NonNull ViewHolder holder,
            int position) {

        Contacto contacto = lista.get(position);

        holder.nombre.setText(contacto.getNombre());
        holder.telefono.setText(contacto.getTelefono());

        holder.eliminar.setOnClickListener(v ->
                listener.onDelete(holder.getAbsoluteAdapterPosition()));
    }

    @Override
    public int getItemCount() {
        return lista.size();
    }
}