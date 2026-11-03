#include "mesa.hpp"
#include "tablero.hpp"
#include <iostream>
#include <unistd.h>
#include <cstdlib>

using namespace std;

static void dormir_ficha(const Configuracion& cfg,unsigned int& semilla){
    int min_ms=cfg.retardo_min_ms;
    int max_ms=cfg.retardo_max_ms;
    if(max_ms<min_ms){
        max_ms=min_ms;
    }
    int espera=min_ms+(rand_r(&semilla)%(max_ms-min_ms+1));
    usleep(espera*1000);
}

Mesa* mesa_crear(int id,const Configuracion& config,Jugador puma,Jugador perros,AlTerminarMesa al_terminar,void* contexto){
    if(config.usar_posiciones_iniciales){
        if(!es_casilla_valida(config.puma_inicial.fila,config.puma_inicial.columna)){
            return nullptr;
        }
        for(size_t i=0;i<config.perros_iniciales.size();i++){
            if(!es_casilla_valida(config.perros_iniciales[i].fila,config.perros_iniciales[i].columna)){
                return nullptr;
            }
        }
    }

    Mesa* mesa=new Mesa();
    Mesa& m=*mesa;

    m.id=id;
    m.config=config;
    m.jugador_puma=puma;
    m.jugador_perros=perros;
    m.detener=false;
    m.abandono_puma=false;
    m.abandono_perros=false;
    m.cantidad_eventos=0;
    m.ultimo_evento="";
    m.ganador=Ganador::Ninguno;
    m.motivo_fin=MotivoFin::Interrupcion;
    m.al_terminar=al_terminar;
    m.contexto_servidor=contexto;

    pthread_mutex_init(&m.candado_eventos,nullptr);
    pthread_cond_init(&m.hay_evento,nullptr);
    tablero_iniciar(m.tablero,config);

    return mesa;
}

void registrar_evento(Mesa* mesa,const string& texto){
    Mesa& m=*mesa;
    pthread_mutex_lock(&m.candado_eventos);
    m.cantidad_eventos++;
    m.ultimo_evento=texto;
    pthread_cond_signal(&m.hay_evento);
    pthread_mutex_unlock(&m.candado_eventos);
}

void mesa_iniciar(Mesa* mesa){
    pthread_create(&mesa->hilo_juez,nullptr,hilo_juez,mesa);
}

void mesa_pedir_parada(Mesa* mesa){
    Mesa& m=*mesa;
    m.detener=true;
    pthread_mutex_lock(&m.candado_eventos);
    pthread_cond_signal(&m.hay_evento);
    pthread_mutex_unlock(&m.candado_eventos);
}

void mesa_reportar_abandono(Mesa* mesa,Rol rol){
    Mesa& m=*mesa;
    if(rol==Rol::Puma){
        m.abandono_puma=true;
    }else{
        m.abandono_perros=true;
    }
    pthread_mutex_lock(&m.candado_eventos);
    pthread_cond_signal(&m.hay_evento);
    pthread_mutex_unlock(&m.candado_eventos);
}

void mesa_esperar_fin(Mesa* mesa){
    pthread_join(mesa->hilo_juez,nullptr);
}

void mesa_destruir(Mesa* mesa){
    Mesa& m=*mesa;
    tablero_destruir(m.tablero);
    pthread_mutex_destroy(&m.candado_eventos);
    pthread_cond_destroy(&m.hay_evento);
    delete mesa;
}

void* hilo_perro(void* arg){
    ArgumentosFicha& info=*(ArgumentosFicha*)arg;
    Mesa& m=*info.mesa;
    int idx=info.indice;
    int df[4]={-1,1,0,0};
    int dc[4]={0,0,-1,1};

    while(!m.detener){
        dormir_ficha(m.config,info.semilla);
        if(m.detener) break;

        pthread_mutex_lock(&m.tablero.candado);
        bool vivo=m.tablero.perro_vivo[idx];
        Posicion actual=m.tablero.posicion_perros[idx];
        pthread_mutex_unlock(&m.tablero.candado);

        if(!vivo) break;

        int orden[4]={0,1,2,3};
        for(int i=0;i<4;i++){
            int r=rand_r(&info.semilla)%4;
            int tmp=orden[i];
            orden[i]=orden[r];
            orden[r]=tmp;
        }

        for(int i=0;i<4;i++){
            int d=orden[i];
            Posicion dest={actual.fila+df[d],actual.columna+dc[d]};
            if(tablero_mover_perro(m.tablero,idx,dest)){
                string msg="perro "+to_string(idx+1)+" se mueve a ("+to_string(dest.fila)+","+to_string(dest.columna)+")";
                registrar_evento(&m,msg);
                break;
            }
        }
    }
    return nullptr;
}

void* hilo_puma(void* arg){
    ArgumentosFicha& info=*(ArgumentosFicha*)arg;
    Mesa& m=*info.mesa;
    int df[4]={-1,1,0,0};
    int dc[4]={0,0,-1,1};

    while(!m.detener){
        dormir_ficha(m.config,info.semilla);
        if(m.detener) break;

        pthread_mutex_lock(&m.tablero.candado);
        Posicion actual=m.tablero.posicion_puma;
        pthread_mutex_unlock(&m.tablero.candado);

        int orden[4]={0,1,2,3};
        for(int i=0;i<4;i++){
            int r=rand_r(&info.semilla)%4;
            int tmp=orden[i];
            orden[i]=orden[r];
            orden[r]=tmp;
        }

        bool movio=false;
        // primero intenta comer saltando un perro
        for(int i=0;i<4;i++){
            int d=orden[i];
            Posicion salto={actual.fila+2*df[d],actual.columna+2*dc[d]};
            int comido=-1;
            if(tablero_saltar_puma(m.tablero,salto,comido)){
                string msg="puma salta a ("+to_string(salto.fila)+","+to_string(salto.columna)+") y captura al perro "+to_string(comido+1);
                registrar_evento(&m,msg);
                movio=true;
                break;
            }
        }

        // si no pudo saltar, intenta moverse a una casilla vacia
        if(!movio){
            for(int i=0;i<4;i++){
                int d=orden[i];
                Posicion dest={actual.fila+df[d],actual.columna+dc[d]};
                if(tablero_mover_puma(m.tablero,dest)){
                    string msg="puma se mueve a ("+to_string(dest.fila)+","+to_string(dest.columna)+")";
                    registrar_evento(&m,msg);
                    break;
                }
            }
        }
    }
    return nullptr;
}

void* hilo_juez(void* arg){
    Mesa& m=*(Mesa*)arg;

    ArgumentosFicha arg_puma={&m,0,m.config.semilla+100+(unsigned int)m.id};
    ArgumentosFicha args_perros[TOTAL_PERROS];

    bool puma_creado=false;
    int perros_creados=0;

    if(pthread_create(&m.hilo_puma,nullptr,hilo_puma,&arg_puma)==0){
        puma_creado=true;
    }else{
        mesa_reportar_abandono(&m,Rol::Puma);
    }

    for(int i=0;i<TOTAL_PERROS;i++){
        args_perros[i]={&m,i,m.config.semilla+200+(unsigned int)(m.id*20+i)};
        if(pthread_create(&m.hilos_perros[i],nullptr,hilo_perro,&args_perros[i])==0){
            perros_creados++;
        }else{
            mesa_reportar_abandono(&m,Rol::Perros);
            break;
        }
    }

    cout<<"\n[mesa "<<m.id<<"] inicia partida\n"<<tablero_a_texto(m.tablero)<<flush;

    if(tablero_puma_acorralado(m.tablero)){
        m.ganador=Ganador::Perros;
        m.motivo_fin=MotivoFin::Victoria;
        m.detener=true;
        cout<<"[mesa "<<m.id<<"] fin: ganan los perros (cliente "<<m.jugador_perros.id<<")\n"<<flush;
    }

    int vistos=0;
    
    while(!m.detener){
        pthread_mutex_lock(&m.candado_eventos);
        while(!m.detener && !m.abandono_puma && !m.abandono_perros && m.cantidad_eventos==vistos){
            pthread_cond_wait(&m.hay_evento,&m.candado_eventos);
        }
        int actual=m.cantidad_eventos;
        string ev=m.ultimo_evento;
        pthread_mutex_unlock(&m.candado_eventos);

        if(m.abandono_puma){
            m.ganador=Ganador::Perros;
            m.motivo_fin=MotivoFin::Abandono;
            m.detener=true;
            cout<<"\n[mesa "<<m.id<<"] abandono del puma. ganan los perros\n"<<flush;
            break;
        }

        if(m.abandono_perros){
            m.ganador=Ganador::Puma;
            m.motivo_fin=MotivoFin::Abandono;
            m.detener=true;
            cout<<"\n[mesa "<<m.id<<"] abandono de los perros. gana el puma\n"<<flush;
            break;
        }

        if(actual>vistos){
            vistos=actual;
            pthread_mutex_lock(&m.tablero.candado);
            int cap=m.tablero.perros_capturados;
            pthread_mutex_unlock(&m.tablero.candado);

            string salida="\n[mesa "+to_string(m.id)+"] "+ev+" (capturados: "+to_string(cap)+"/"+to_string(CAPTURAS_PARA_GANAR)+")\n"+tablero_a_texto(m.tablero);
            cout<<salida<<flush;

            if(cap>=CAPTURAS_PARA_GANAR){
                m.ganador=Ganador::Puma;
                m.motivo_fin=MotivoFin::Victoria;
                m.detener=true;
                cout<<"[mesa "<<m.id<<"] fin: gana el puma (cliente "<<m.jugador_puma.id<<")\n"<<flush;
                break;
            }

            if(tablero_puma_acorralado(m.tablero)){
                m.ganador=Ganador::Perros;
                m.motivo_fin=MotivoFin::Victoria;
                m.detener=true;
                cout<<"[mesa "<<m.id<<"] fin: ganan los perros (cliente "<<m.jugador_perros.id<<")\n"<<flush;
                break;
            }
        }
    }

    m.detener=true;
    if(puma_creado){
        pthread_join(m.hilo_puma,nullptr);
    }
    for(int i=0;i<perros_creados;i++){
        pthread_join(m.hilos_perros[i],nullptr);
    }

    if(m.al_terminar!=nullptr){
        m.al_terminar(m.contexto_servidor,&m);
    }
    return nullptr;
}
