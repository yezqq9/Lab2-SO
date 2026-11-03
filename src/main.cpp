#include <iostream>
#include "servidor.hpp"
using namespace std;

static bool leer_positivo(const char *texto,int &valor){
    try{
        valor=stoi(texto);
    }catch(...){
        return false;
    }
    return valor>0;
}

int main(int argc,char **argv){
    if(argc<3 || argc>4){
        cerr<<"uso: "<<argv[0]<<" N M [config.txt]"<<endl;
        return 1;
    }
    int total=0;
    int mesas=0;
    if(!leer_positivo(argv[1],total) || !leer_positivo(argv[2],mesas)){
        cerr<<"N y M deben ser enteros positivos"<<endl;
        return 1;
    }

    Configuracion config;
    if(argc==4 && !configuracion_cargar(argv[3],config)) return 1;

    Servidor servidor;
    servidor_iniciar(servidor,total,mesas,config);
    instalar_manejador_sigint(&servidor);
    servidor_ejecutar(servidor);
    servidor_destruir(servidor);
    return 0;
}
