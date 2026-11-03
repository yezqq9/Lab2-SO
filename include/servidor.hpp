#pragma once
#include "tipos.hpp"

bool configuracion_cargar(const std::string& ruta,Configuracion& config);

void servidor_iniciar(Servidor& servidor,int total_clientes,
                      int max_mesas,const Configuracion& config);
void servidor_ejecutar(Servidor& servidor);   // matchmaker: empareja y abre mesas
void servidor_detener(Servidor& servidor);    // avisa a las mesas activas que paren
void servidor_destruir(Servidor& servidor);

void servidor_mesa_terminada(void* contexto,Mesa* mesa);
void instalar_manejador_sigint(Servidor* servidor);
