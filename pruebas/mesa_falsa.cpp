#include "mesa.hpp"
#include <unistd.h>
#include <iostream>
using namespace std;

Mesa* mesa_crear(int id,const Configuracion &config,Jugador puma,Jugador perros,
                 AlTerminarMesa al_terminar,void *contexto){
    Mesa *mesa=new Mesa();
    mesa->id=id;
    mesa->config=config;
    mesa->jugador_puma=puma;
    mesa->jugador_perros=perros;
    mesa->detener=false;
    mesa->al_terminar=al_terminar;
    mesa->contexto_servidor=contexto;
    return mesa;
}

void* hilo_juez(void *arg){
    Mesa *mesa=(Mesa*)arg;
    // la partida falsa dura 1 segundo o menos si le piden parar
    for(int i=0;i<10 && !mesa->detener;i++) usleep(100000);
    cout<<"mesa "<<mesa->id<<" termina"<<endl;
    mesa->al_terminar(mesa->contexto_servidor,mesa);
    return nullptr;
}

void mesa_iniciar(Mesa *mesa){ pthread_create(&mesa->hilo_juez,nullptr,hilo_juez,mesa); }
void mesa_pedir_parada(Mesa *mesa){ mesa->detener=true; }
void mesa_esperar_fin(Mesa *mesa){ pthread_join(mesa->hilo_juez,nullptr); }
void mesa_destruir(Mesa *mesa){ delete mesa; }
