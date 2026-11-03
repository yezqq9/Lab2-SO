#include "mesa.hpp"
#include "tablero.hpp"
#include <iostream>
#include <unistd.h>
#include <cstdlib>

using namespace std;

void esperar_delay(const Configuracion* cfg){
    int min_ms=50;
    int max_ms=300;
    if(cfg!=NULL){
        min_ms=cfg->retardo_min_ms;
        max_ms=cfg->retardo_max_ms;
    }
    if(max_ms<min_ms){
        max_ms=min_ms;
    }
    int t_espera=min_ms+(rand()%(max_ms-min_ms+1));
    usleep(t_espera*1000);
}

void* fn_perro(void* arg){
    Ficha* perro=(Ficha*)arg;
    Mesa* m=perro->mesa;
    Tablero* t=&m->tablero;

    while(true){
        esperar_delay(m->cfg);

        pthread_mutex_lock(&t->candado);
        if(!m->activa || m->parada_pedida || !perro->activa){
            pthread_mutex_unlock(&t->candado);
            break;
        }

        int op_f[4];
        int op_c[4];
        int cant=0;

        for(int f=0;f<7;f++){
            for(int c=0;c<5;c++){
                if(son_adyacentes(perro->fila,perro->col,f,c) && t->casillas[f][c]==VACIA){
                    op_f[cant]=f;
                    op_c[cant]=c;
                    cant++;
                }
            }
        }

        if(cant>0){
            int idx=rand()%cant;
            int nf=op_f[idx];
            int nc=op_c[idx];

            t->casillas[perro->fila][perro->col]=VACIA;
            t->casillas[nf][nc]=PERRO;
            perro->fila=nf;
            perro->col=nc;
            pthread_cond_signal(&m->cond_juez);
        }

        pthread_mutex_unlock(&t->candado);
    }
    return NULL;
}

void* fn_puma(void* arg){
    Ficha* puma=(Ficha*)arg;
    Mesa* m=puma->mesa;
    Tablero* t=&m->tablero;

    while(true){
        esperar_delay(m->cfg);

        pthread_mutex_lock(&t->candado);
        if(!m->activa || m->parada_pedida){
            pthread_mutex_unlock(&t->candado);
            break;
        }

        int pf=puma->fila;
        int pc=puma->col;
        bool salto=false;

        // revisar primero si puede saltar a un perro
        for(int fm=0;fm<7;fm++){
            for(int cm=0;cm<5;cm++){
                if(son_adyacentes(pf,pc,fm,cm) && t->casillas[fm][cm]==PERRO){
                    int f2=fm+(fm-pf);
                    int c2=cm+(cm-pc);

                    if(es_salto_valido(t,pf,pc,fm,cm,f2,c2)){
                        t->casillas[pf][pc]=VACIA;
                        t->casillas[fm][cm]=VACIA;
                        t->casillas[f2][c2]=PUMA;
                        puma->fila=f2;
                        puma->col=c2;
                        t->perros_capturados++;

                        for(int i=0;i<12;i++){
                            if(m->perros[i].activa && m->perros[i].fila==fm && m->perros[i].col==cm){
                                m->perros[i].activa=false;
                                break;
                            }
                        }
                        salto=true;
                        pthread_cond_signal(&m->cond_juez);
                        break;
                    }
                }
            }
            if(salto) break;
        }

        if(!salto){
            int op_f[4];
            int op_c[4];
            int cant=0;

            for(int f=0;f<7;f++){
                for(int c=0;c<5;c++){
                    if(son_adyacentes(pf,pc,f,c) && t->casillas[f][c]==VACIA){
                        op_f[cant]=f;
                        op_c[cant]=c;
                        cant++;
                    }
                }
            }

            if(cant>0){
                int idx=rand()%cant;
                int nf=op_f[idx];
                int nc=op_c[idx];

                t->casillas[pf][pc]=VACIA;
                t->casillas[nf][nc]=PUMA;
                puma->fila=nf;
                puma->col=nc;
                pthread_cond_signal(&m->cond_juez);
            }
        }

        pthread_mutex_unlock(&t->candado);
    }
    return NULL;
}

void* fn_juez(void* arg){
    Mesa* m=(Mesa*)arg;
    Tablero* t=&m->tablero;

    pthread_mutex_lock(&t->candado);
    tablero_imprimir(t,m->id_mesa,m->id_jugador_puma,m->id_jugador_perros,"Empieza la partida");
    pthread_mutex_unlock(&t->candado);

    if(pthread_create(&m->puma.hilo,NULL,fn_puma,&m->puma)!=0){
        pthread_mutex_lock(&t->candado);
        m->fallo_ficha=true;
        m->id_jugador_fallido=m->id_jugador_puma;
        m->activa=false;
        pthread_mutex_unlock(&t->candado);
    }

    int creados=0;
    for(int i=0;i<12;i++){
        if(pthread_create(&m->perros[i].hilo,NULL,fn_perro,&m->perros[i])!=0){
            pthread_mutex_lock(&t->candado);
            m->fallo_ficha=true;
            m->id_jugador_fallido=m->id_jugador_perros;
            m->activa=false;
            pthread_mutex_unlock(&t->candado);
            break;
        }
        creados++;
    }

    pthread_mutex_lock(&t->candado);
    while(m->activa && !m->parada_pedida){
        pthread_cond_wait(&m->cond_juez,&t->candado);

        if(m->parada_pedida){
            tablero_imprimir(t,m->id_mesa,m->id_jugador_puma,m->id_jugador_perros,"Partida cortada por servidor");
            m->activa=false;
            break;
        }

        if(m->fallo_ficha){
            if(m->id_jugador_fallido==m->id_jugador_puma){
                tablero_imprimir(t,m->id_mesa,m->id_jugador_puma,m->id_jugador_perros,"Abandono Puma. Ganan Perros");
            }else{
                tablero_imprimir(t,m->id_mesa,m->id_jugador_puma,m->id_jugador_perros,"Abandono Perros. Gana Puma");
            }
            m->activa=false;
            break;
        }

        tablero_imprimir(t,m->id_mesa,m->id_jugador_puma,m->id_jugador_perros,"Jugada");

        if(t->perros_capturados>=6){
            tablero_imprimir(t,m->id_mesa,m->id_jugador_puma,m->id_jugador_perros,"Fin: Gana el Puma");
            m->activa=false;
            break;
        }

        if(puma_acorralado(t,m->puma.fila,m->puma.col)){
            tablero_imprimir(t,m->id_mesa,m->id_jugador_puma,m->id_jugador_perros,"Fin: Ganan los Perros");
            m->activa=false;
            break;
        }
    }
    m->activa=false;
    pthread_mutex_unlock(&t->candado);

    // esperar que salgan las fichas antes de avisar que termino la mesa
    if(!m->fallo_ficha || m->id_jugador_fallido!=m->id_jugador_puma){
        pthread_join(m->puma.hilo,NULL);
    }
    for(int i=0;i<creados;i++){
        pthread_join(m->perros[i].hilo,NULL);
    }

    if(m->al_terminar!=NULL){
        m->al_terminar(m->id_mesa,m->ctx_servidor);
    }
    return NULL;
}

Mesa* mesa_crear(int id_mesa,int j_puma,int j_perros,const Configuracion* cfg,CallbackFinMesa al_terminar,void* ctx){
    Mesa* m=new Mesa;

    m->id_mesa=id_mesa;
    m->id_jugador_puma=j_puma;
    m->id_jugador_perros=j_perros;
    m->cfg=cfg;
    m->activa=true;
    m->parada_pedida=false;
    m->fallo_ficha=false;
    m->id_jugador_fallido=-1;
    m->al_terminar=al_terminar;
    m->ctx_servidor=ctx;

    pthread_cond_init(&m->cond_juez,NULL);
    tablero_iniciar(&m->tablero,cfg);

    int cont=0;
    for(int f=0;f<7;f++){
        for(int c=0;c<5;c++){
            if(m->tablero.casillas[f][c]==PUMA){
                m->puma.id=0;
                m->puma.es_puma=true;
                m->puma.activa=true;
                m->puma.fila=f;
                m->puma.col=c;
                m->puma.mesa=m;
            }else if(m->tablero.casillas[f][c]==PERRO && cont<12){
                m->perros[cont].id=cont+1;
                m->perros[cont].es_puma=false;
                m->perros[cont].activa=true;
                m->perros[cont].fila=f;
                m->perros[cont].col=c;
                m->perros[cont].mesa=m;
                cont++;
            }
        }
    }

    if(pthread_create(&m->hilo_juez,NULL,fn_juez,m)!=0){
        tablero_destruir(&m->tablero);
        pthread_cond_destroy(&m->cond_juez);
        delete m;
        return nullptr;
    }

    return m;
}

void mesa_pedir_parada(Mesa* m){
    if(m==NULL) return;
    pthread_mutex_lock(&m->tablero.candado);
    m->parada_pedida=true;
    m->activa=false;
    pthread_cond_signal(&m->cond_juez);
    pthread_mutex_unlock(&m->tablero.candado);
}

void mesa_esperar_fin(Mesa* m){
    if(m==NULL) return;
    pthread_join(m->hilo_juez,NULL);
}

void mesa_destruir(Mesa* m){
    if(m==NULL) return;
    tablero_destruir(&m->tablero);
    pthread_cond_destroy(&m->cond_juez);
    delete m;
}
