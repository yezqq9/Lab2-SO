#include <iostream>
#include "servidor.hpp"
using namespace std;

// pasa el texto a numero, solo sirve si es un entero mayor que 0
static bool leer_positivo(const char *texto,int &valor){
    try{
        valor=stoi(texto);
    }catch(...){
        return false;
    }
    return valor>0;
}

int main(int argc,char **argv){
    // se usa asi: ./torneo N M [config.txt]
    if(argc<3 || argc>4){
        cerr<<"uso: "<<argv[0]<<" N M [config.txt]"<<endl;
        return 1;
    }
    // N es el total de clientes y M el maximo de mesas a la vez
    int total=0;
    int mesas=0;
    if(!leer_positivo(argv[1],total) || !leer_positivo(argv[2],mesas)){
        cerr<<"N y M deben ser enteros positivos"<<endl;
        return 1;
    }

    // si viene config.txt lo cargo, si tiene errores no se parte
    Configuracion config;
    if(argc==4 && !configuracion_cargar(argv[3],config)) return 1;

    // se prepara el servidor, se instala el ctrl+c y se juega todo
    Servidor servidor;
    servidor_iniciar(servidor,total,mesas,config);
    instalar_manejador_sigint(&servidor);
    servidor_ejecutar(servidor);
    // espera a que terminen las mesas y libera todo
    servidor_destruir(servidor);
    return 0;
}
