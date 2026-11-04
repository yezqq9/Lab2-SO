#include "servidor.hpp"
#include "mesa.hpp"
#include <fstream>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <unistd.h>
using namespace std;

static Servidor *servidor_global=nullptr;
static vector<Mesa*> terminadas;

static string limpiar(string s){
    size_t ini=s.find_first_not_of(" \t\r\n");
    if(ini==string::npos) return "";
    size_t fin=s.find_last_not_of(" \t\r\n");
    return s.substr(ini,fin-ini+1);
}

static bool leer_numero(string texto,int &n){
    // stoi tira excepcion si no es numero
    try{
        n=stoi(texto);
    }catch(...){
        return false;
    }
    return n>=0;
}

static bool leer_posicion(string texto,Posicion &pos){
    size_t coma=texto.find(',');
    if(coma==string::npos) return false;
    if(!leer_numero(limpiar(texto.substr(0,coma)),pos.fila)) return false;
    if(!leer_numero(limpiar(texto.substr(coma+1)),pos.columna)) return false;
    // aca solo reviso el rango, si la casilla sirve lo ve el tablero
    return pos.fila<FILAS && pos.columna<COLUMNAS;
}

static bool son_iguales(Posicion a,Posicion b){
    return a.fila==b.fila && a.columna==b.columna;
}

bool configuracion_cargar(const string &ruta,Configuracion &config){
    ifstream archivo(ruta);
    if(!archivo){
        cerr<<"no se pudo abrir "<<ruta<<endl;
        return false;
    }

    // copia para no dejar config a medias si hay error
    Configuracion nueva=config;
    vector<Posicion> perros;
    bool hay_puma=false;
    string linea;
    int num=0;

    while(getline(archivo,linea)){
        num++;
        size_t com=linea.find('#');
        if(com!=string::npos) linea.erase(com);
        linea=limpiar(linea);
        if(linea.empty()) continue;

        size_t igual=linea.find('=');
        if(igual==string::npos){
            cerr<<"config linea "<<num<<": falta el ="<<endl;
            return false;
        }
        string clave=limpiar(linea.substr(0,igual));
        string valor=limpiar(linea.substr(igual+1));
        int n=0;
        Posicion pos={0,0};

        if(clave=="retardo_min_ms" || clave=="retardo_max_ms" || clave=="semilla"){
            if(!leer_numero(valor,n)){
                cerr<<"config linea "<<num<<": numero invalido"<<endl;
                return false;
            }
            if(clave=="retardo_min_ms") nueva.retardo_min_ms=n;
            else if(clave=="retardo_max_ms") nueva.retardo_max_ms=n;
            else nueva.semilla=n;
        }
        else if(clave=="puma" || clave=="perro"){
            if(!leer_posicion(valor,pos)){
                cerr<<"config linea "<<num<<": posicion invalida, usar fila,columna"<<endl;
                return false;
            }
            if(clave=="puma"){
                if(hay_puma){
                    cerr<<"config linea "<<num<<": el puma esta repetido"<<endl;
                    return false;
                }
                nueva.puma_inicial=pos;
                hay_puma=true;
            }
            else perros.push_back(pos);
        }
        else if(clave=="abandono"){
            if(valor=="puma") nueva.abandono=1;
            else if(valor=="perros") nueva.abandono=2;
            else{
                cerr<<"config linea "<<num<<": abandono debe ser puma o perros"<<endl;
                return false;
            }
        }
        else{
            cerr<<"config linea "<<num<<": clave desconocida "<<clave<<endl;
            return false;
        }
    }

    if(nueva.retardo_min_ms>nueva.retardo_max_ms){
        cerr<<"config: el minimo no puede ser mayor al maximo"<<endl;
        return false;
    }

    // o van todas las posiciones o ninguna
    if(hay_puma || !perros.empty()){
        if(!hay_puma || (int)perros.size()!=TOTAL_PERROS){
            cerr<<"config: hay que definir 1 puma y "<<TOTAL_PERROS<<" perros"<<endl;
            return false;
        }
        for(size_t i=0;i<perros.size();i++){
            if(son_iguales(perros[i],nueva.puma_inicial)){
                cerr<<"config: un perro esta sobre el puma"<<endl;
                return false;
            }
            for(size_t j=i+1;j<perros.size();j++){
                if(son_iguales(perros[i],perros[j])){
                    cerr<<"config: dos perros en la misma casilla"<<endl;
                    return false;
                }
            }
        }
        nueva.usar_posiciones_iniciales=true;
        nueva.perros_iniciales=perros;
    }

    config=nueva;
    return true;
}

// en el manejador solo se cambia la variable, lo demas lo hace el servidor
static void manejar_sigint(int){
    if(servidor_global) servidor_global->aceptando_clientes=false;
}

void instalar_manejador_sigint(Servidor *servidor){
    servidor_global=servidor;
    struct sigaction accion={};
    accion.sa_handler=manejar_sigint;
    sigemptyset(&accion.sa_mask);
    // sin SA_RESTART para que el sem_wait se corte con el ctrl+c
    accion.sa_flags=0;
    sigaction(SIGINT,&accion,nullptr);
}

void servidor_iniciar(Servidor &servidor,int total_clientes,int max_mesas,const Configuracion &config){
    servidor.total_clientes=total_clientes;
    servidor.max_mesas=max_mesas;
    servidor.config=config;
    servidor.aceptando_clientes=true;
    servidor.partidas_terminadas=0;
    if(sem_init(&servidor.cupos_mesas,0,max_mesas)!=0){
        perror("sem_init");
        exit(1);
    }
    pthread_mutex_init(&servidor.candado_mesas,nullptr);
}

void servidor_detener(Servidor &servidor){
    servidor.aceptando_clientes=false;
    pthread_mutex_lock(&servidor.candado_mesas);
    for(size_t i=0;i<servidor.mesas_activas.size();i++){
        mesa_pedir_parada(servidor.mesas_activas[i]);
    }
    pthread_mutex_unlock(&servidor.candado_mesas);
}

// el juez no puede hacerse join a si mismo, por eso el servidor las une despues
static void limpiar_terminadas(Servidor &servidor){
    pthread_mutex_lock(&servidor.candado_mesas);
    vector<Mesa*> lista=terminadas;
    terminadas.clear();
    pthread_mutex_unlock(&servidor.candado_mesas);

    for(size_t i=0;i<lista.size();i++){
        mesa_esperar_fin(lista[i]);
        mesa_destruir(lista[i]);
    }
}

void servidor_mesa_terminada(void *contexto,Mesa *mesa){
    Servidor *servidor=(Servidor*)contexto;
    pthread_mutex_lock(&servidor->candado_mesas);
    for(size_t i=0;i<servidor->mesas_activas.size();i++){
        if(servidor->mesas_activas[i]==mesa){
            servidor->mesas_activas.erase(servidor->mesas_activas.begin()+i);
            break;
        }
    }
    terminadas.push_back(mesa);
    servidor->partidas_terminadas++;
    pthread_mutex_unlock(&servidor->candado_mesas);
    sem_post(&servidor->cupos_mesas);
}

void servidor_ejecutar(Servidor &servidor){
    srand(servidor.config.semilla);
    int pares=servidor.total_clientes/2;
    int hechos=0;
    cout<<"llegan "<<servidor.total_clientes<<" clientes"<<endl;

    for(int i=0;i<pares && servidor.aceptando_clientes;i++){
        Jugador puma={2*i+1,Rol::Puma};
        Jugador perros={2*i+2,Rol::Perros};

        // los clientes llegan con un tiempo distinto cada vez
        usleep(rand()%200*1000);
        if(!servidor.aceptando_clientes) break;

        // si las M mesas estan ocupadas se espera aca sin gastar cpu
        int resultado=sem_wait(&servidor.cupos_mesas);
        if(!servidor.aceptando_clientes){
            // devuelvo el cupo para que el conteo final de destruir cuadre
            if(resultado==0) sem_post(&servidor.cupos_mesas);
            break;
        }

        limpiar_terminadas(servidor);
        Mesa *mesa=mesa_crear(i+1,servidor.config,puma,perros,servidor_mesa_terminada,&servidor);
        if(mesa==nullptr){
            cerr<<"no se pudo crear la mesa "<<i+1<<endl;
            sem_post(&servidor.cupos_mesas);
            break;
        }

        // se agrega antes de iniciar porque la mesa puede terminar muy rapido
        pthread_mutex_lock(&servidor.candado_mesas);
        servidor.mesas_activas.push_back(mesa);
        pthread_mutex_unlock(&servidor.candado_mesas);

        cout<<"mesa "<<mesa->id<<": cliente "<<puma.id<<" (puma) contra cliente "<<perros.id<<" (perros)"<<endl;
        mesa_iniciar(mesa);
        hechos++;
    }

    if(!servidor.aceptando_clientes){
        cout<<"ctrl+c: no se aceptan mas clientes, se detienen las mesas"<<endl;
        servidor_detener(servidor);
    }
    cout<<"partidas iniciadas: "<<hechos<<", clientes sin jugar: "<<servidor.total_clientes-2*hechos<<endl;
}

void servidor_destruir(Servidor &servidor){
    // si vuelven los M cupos es porque todas las mesas terminaron
    for(int i=0;i<servidor.max_mesas;i++){
        while(sem_wait(&servidor.cupos_mesas)!=0){
            // un ctrl+c mientras se espera tambien avisa a las mesas
            if(!servidor.aceptando_clientes) servidor_detener(servidor);
        }
    }
    limpiar_terminadas(servidor);
    sem_destroy(&servidor.cupos_mesas);
    pthread_mutex_destroy(&servidor.candado_mesas);
    servidor_global=nullptr;
}
