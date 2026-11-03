#include <iostream>
#include "servidor.hpp"
using namespace std;

int main(int argc,char **argv){
    Configuracion config;
    string ruta="config.txt";
    if(argc>1) ruta=argv[1];
    if(!configuracion_cargar(ruta,config)) return 1;
    cout<<"min="<<config.retardo_min_ms<<" max="<<config.retardo_max_ms
        <<" semilla="<<config.semilla<<endl;
    return 0;
}
