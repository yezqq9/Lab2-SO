#include "servidor.hpp"
#include <fstream>
#include <iostream>
using namespace std;

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
