#pragma once
#include "tipos.hpp"

Mesa* mesa_crear(int id,const Configuracion& config,
                 Jugador puma,Jugador perros,
                 AlTerminarMesa al_terminar,void* contexto);

void mesa_iniciar(Mesa* mesa);            // lanza el juez, que lanza las fichas
void mesa_pedir_parada(Mesa* mesa);       // usado por SIGINT
void mesa_reportar_abandono(Mesa* mesa,Rol rol);
void mesa_esperar_fin(Mesa* mesa);        // join del juez
void mesa_destruir(Mesa* mesa);

void registrar_evento(Mesa* mesa,const std::string& texto);

void* hilo_juez(void* arg);               // arg = Mesa*
void* hilo_puma(void* arg);               // arg = ArgumentosFicha*
void* hilo_perro(void* arg);              // arg = ArgumentosFicha*
