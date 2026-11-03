#pragma once
#include "tipos.hpp"

bool es_casilla_valida(int fila,int columna);
bool son_adyacentes(Posicion a,Posicion b);

void tablero_iniciar(Tablero& tablero,const Configuracion& config);
void tablero_destruir(Tablero& tablero);

// todas estas toman el candado del tablero por dentro
bool tablero_mover_perro(Tablero& tablero,int indice,Posicion destino);
bool tablero_mover_puma(Tablero& tablero,Posicion destino);
bool tablero_saltar_puma(Tablero& tablero,Posicion destino,int& perro_capturado);

bool tablero_puma_acorralado(Tablero& tablero);
std::string tablero_a_texto(Tablero& tablero);
