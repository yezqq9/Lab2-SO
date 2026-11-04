#include "mesa.hpp"
#include "tablero.hpp"
#include <iostream>
#include <unistd.h>
#include <cstdlib>

using namespace std;

// cada ficha duerme un tiempo al azar entre el minimo y el maximo del config
static void dormir_ficha(const Configuracion& cfg,unsigned int& semilla){
    int min_ms=cfg.retardo_min_ms;
    int max_ms=cfg.retardo_max_ms;
    // por si el maximo viene menor al minimo
    if(max_ms<min_ms){
        max_ms=min_ms;
    }
    // rand_r usa la semilla de cada ficha, asi no se comparte entre hilos
    int espera=min_ms+(rand_r(&semilla)%(max_ms-min_ms+1));
    usleep(espera*1000);
}

// crea la mesa y deja todo listo, pero todavia no lanza ningun hilo
Mesa* mesa_crear(int id,const Configuracion& config,Jugador puma,Jugador perros,AlTerminarMesa al_terminar,void* contexto){
    // si el config trae posiciones iniciales, reviso que sean casillas validas
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

    // datos basicos de la mesa
    m.id=id;
    m.config=config;
    m.jugador_puma=puma;
    m.jugador_perros=perros;
    // todo parte en falso, nadie ha abandonado ni hay que parar
    m.detener=false;
    m.abandono_puma=false;
    m.abandono_perros=false;
    m.cantidad_eventos=0;
    m.ganador=Ganador::Ninguno;
    m.motivo_fin=MotivoFin::Interrupcion;
    // callback para avisarle al servidor cuando termine la partida
    m.al_terminar=al_terminar;
    m.contexto_servidor=contexto;

    pthread_mutex_init(&m.candado_eventos,nullptr);
    pthread_cond_init(&m.hay_evento,nullptr);
    tablero_iniciar(m.tablero,config);

    return mesa;
}

// las fichas llaman a esto cada vez que hacen algo, y el juez lo imprime
void registrar_evento(Mesa* mesa,const string& texto){
    Mesa& m=*mesa;
    pthread_mutex_lock(&m.candado_eventos);
    m.cantidad_eventos++;
    // guardo el evento en la lista para que no se pierda ninguno
    m.eventos.push_back(texto);
    // despierto al juez que esta esperando
    pthread_cond_signal(&m.hay_evento);
    pthread_mutex_unlock(&m.candado_eventos);
}

// lanza solo el juez, y el juez despues lanza las fichas
void mesa_iniciar(Mesa* mesa){
    pthread_create(&mesa->hilo_juez,nullptr,hilo_juez,mesa);
}

// se usa con el ctrl+c, avisa que hay que terminar la partida
void mesa_pedir_parada(Mesa* mesa){
    Mesa& m=*mesa;
    m.detener=true;
    // despierto al juez por si estaba dormido esperando eventos
    pthread_mutex_lock(&m.candado_eventos);
    pthread_cond_signal(&m.hay_evento);
    pthread_mutex_unlock(&m.candado_eventos);
}

// marca que un jugador se cayo y despierta al juez para que lo vea
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

// espera a que el hilo del juez termine
void mesa_esperar_fin(Mesa* mesa){
    pthread_join(mesa->hilo_juez,nullptr);
}

// libera todo lo de la mesa, se llama despues del join del juez
void mesa_destruir(Mesa* mesa){
    Mesa& m=*mesa;
    tablero_destruir(m.tablero);
    pthread_mutex_destroy(&m.candado_eventos);
    pthread_cond_destroy(&m.hay_evento);
    delete mesa;
}

// hilo de cada perro, se mueve a una casilla vecina vacia
void* hilo_perro(void* arg){
    ArgumentosFicha& info=*(ArgumentosFicha*)arg;
    Mesa& m=*info.mesa;
    int idx=info.indice;
    // direcciones: arriba, abajo, izquierda y derecha
    int df[4]={-1,1,0,0};
    int dc[4]={0,0,-1,1};
    // cuenta las jugadas, sirve para probar el abandono
    int jugadas=0;

    while(!m.detener){
        dormir_ficha(m.config,info.semilla);
        if(m.detener) break;
        jugadas++;
        // si el config pide que los perros abandonen, solo el perro 0 se cae
        if(m.config.abandono==2 && idx==0 && jugadas>5){
            mesa_reportar_abandono(&m,Rol::Perros);
            break;
        }

        // leo mi posicion con el candado puesto
        pthread_mutex_lock(&m.tablero.candado);
        bool vivo=m.tablero.perro_vivo[idx];
        Posicion actual=m.tablero.posicion_perros[idx];
        pthread_mutex_unlock(&m.tablero.candado);

        // si me capturaron ya no juego mas
        if(!vivo) break;

        // mezclo las 4 direcciones para no ir siempre hacia el mismo lado
        int orden[4]={0,1,2,3};
        for(int i=0;i<4;i++){
            int r=rand_r(&info.semilla)%4;
            int tmp=orden[i];
            orden[i]=orden[r];
            orden[r]=tmp;
        }

        // pruebo las direcciones hasta que una funcione
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

// hilo del puma, intenta saltar un perro y si no puede se mueve normal
void* hilo_puma(void* arg){
    ArgumentosFicha& info=*(ArgumentosFicha*)arg;
    Mesa& m=*info.mesa;
    int df[4]={-1,1,0,0};
    int dc[4]={0,0,-1,1};
    int jugadas=0;

    while(!m.detener){
        dormir_ficha(m.config,info.semilla);
        if(m.detener) break;
        jugadas++;
        // si el config pide que el puma abandone, se cae a la jugada 6
        if(m.config.abandono==1 && jugadas>5){
            mesa_reportar_abandono(&m,Rol::Puma);
            break;
        }

        // leo la posicion del puma con el candado puesto
        pthread_mutex_lock(&m.tablero.candado);
        Posicion actual=m.tablero.posicion_puma;
        pthread_mutex_unlock(&m.tablero.candado);

        // mezclo las direcciones igual que los perros
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
            // el salto cae 2 casillas mas alla
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

// hilo del juez, crea las fichas, imprime la partida y decide quien gana
void* hilo_juez(void* arg){
    Mesa& m=*(Mesa*)arg;

    // cada ficha lleva su propia semilla
    ArgumentosFicha arg_puma={&m,0,m.config.semilla+100+(unsigned int)m.id};
    ArgumentosFicha args_perros[TOTAL_PERROS];

    // llevo la cuenta de los hilos creados para hacerles join al final
    bool puma_creado=false;
    int perros_creados=0;

    // creo el hilo del puma, si falla cuenta como abandono
    if(pthread_create(&m.hilo_puma,nullptr,hilo_puma,&arg_puma)==0){
        puma_creado=true;
    }else{
        mesa_reportar_abandono(&m,Rol::Puma);
    }

    // creo los 12 perros, si uno falla paro de crear y cuenta como abandono
    for(int i=0;i<TOTAL_PERROS;i++){
        args_perros[i]={&m,i,m.config.semilla+200+(unsigned int)(m.id*20+i)};
        if(pthread_create(&m.hilos_perros[i],nullptr,hilo_perro,&args_perros[i])==0){
            perros_creados++;
        }else{
            mesa_reportar_abandono(&m,Rol::Perros);
            break;
        }
    }

    // imprimo el tablero inicial
    cout<<"\n[mesa "<<m.id<<"] inicia partida\n"<<tablero_a_texto(m.tablero)<<flush;

    // puede que el puma ya empiece sin movimientos (por el config)
    if(tablero_puma_acorralado(m.tablero)){
        m.ganador=Ganador::Perros;
        m.motivo_fin=MotivoFin::Victoria;
        m.detener=true;
        cout<<"[mesa "<<m.id<<"] fin: ganan los perros (cliente "<<m.jugador_perros.id<<")\n"<<flush;
    }

    // el juez espera eventos, los imprime y revisa si alguien gano
    while(!m.detener){
        pthread_mutex_lock(&m.candado_eventos);
        // duerme hasta que haya eventos nuevos, abandono o parada
        while(!m.detener && !m.abandono_puma && !m.abandono_perros && m.eventos.empty()){
            pthread_cond_wait(&m.hay_evento,&m.candado_eventos);
        }
        // me llevo todos los eventos de una vez y dejo la lista vacia
        vector<string> lista;
        lista.swap(m.eventos);
        pthread_mutex_unlock(&m.candado_eventos);

        // imprimo cada evento con su tablero
        for(size_t i=0;i<lista.size();i++){
            pthread_mutex_lock(&m.tablero.candado);
            int cap=m.tablero.perros_capturados;
            pthread_mutex_unlock(&m.tablero.candado);
            string salida="\n[mesa "+to_string(m.id)+"] "+lista[i]+" (capturados: "+to_string(cap)+"/"+to_string(CAPTURAS_PARA_GANAR)+")\n"+tablero_a_texto(m.tablero);
            cout<<salida<<flush;
        }

        // si el puma abandono, ganan los perros
        if(m.abandono_puma){
            m.ganador=Ganador::Perros;
            m.motivo_fin=MotivoFin::Abandono;
            m.detener=true;
            cout<<"\n[mesa "<<m.id<<"] abandono del puma. ganan los perros\n"<<flush;
            break;
        }

        // si los perros abandonaron, gana el puma
        if(m.abandono_perros){
            m.ganador=Ganador::Puma;
            m.motivo_fin=MotivoFin::Abandono;
            m.detener=true;
            cout<<"\n[mesa "<<m.id<<"] abandono de los perros. gana el puma\n"<<flush;
            break;
        }

        pthread_mutex_lock(&m.tablero.candado);
        int capturados=m.tablero.perros_capturados;
        pthread_mutex_unlock(&m.tablero.candado);

        // el puma gana con 6 capturas o mas
        if(capturados>=CAPTURAS_PARA_GANAR){
            m.ganador=Ganador::Puma;
            m.motivo_fin=MotivoFin::Victoria;
            m.detener=true;
            cout<<"[mesa "<<m.id<<"] fin: gana el puma (cliente "<<m.jugador_puma.id<<")\n"<<flush;
            break;
        }

        // los perros ganan si el puma no tiene a donde moverse
        if(tablero_puma_acorralado(m.tablero)){
            m.ganador=Ganador::Perros;
            m.motivo_fin=MotivoFin::Victoria;
            m.detener=true;
            cout<<"[mesa "<<m.id<<"] fin: ganan los perros (cliente "<<m.jugador_perros.id<<")\n"<<flush;
            break;
        }
    }

    // aviso a todas las fichas que paren y espero a que terminen
    m.detener=true;
    if(puma_creado){
        pthread_join(m.hilo_puma,nullptr);
    }
    for(int i=0;i<perros_creados;i++){
        pthread_join(m.hilos_perros[i],nullptr);
    }

    // le aviso al servidor que la mesa termino para que libere el cupo
    if(m.al_terminar!=nullptr){
        m.al_terminar(m.contexto_servidor,&m);
    }
    return nullptr;
}
