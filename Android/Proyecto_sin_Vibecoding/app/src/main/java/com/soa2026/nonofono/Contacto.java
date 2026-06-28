package com.soa2026.nonofono;

public class Contacto {
    private String nombre;
    private String telefono;
    private long chatId;

    public Contacto(String nombre, String telefono) {
        this.nombre = nombre;
        this.telefono = telefono;
    }

    public Contacto(String nombre, String telefono, long chatId) {
        this.nombre = nombre;
        this.telefono = telefono;
        this.chatId = chatId;
    }

    public String getNombre() {
        return nombre;
    }

    public String getTelefono() {
        return telefono;
    }

    public Long getChatId(){
        return chatId;
    }

    public void setChatId(long chatId){
        this.chatId = chatId;
    }
}
