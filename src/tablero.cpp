#include "tablero.hpp"
#include <iostream>
#include <cstdlib>

using namespace std;

bool casilla_valida(int f,int c){
    if(f<0 || f>=7 || c<0 || c>=5){
        return false;
    }
    if(f==0 && c!=2){
        return false;
    }
    if(f==1 && (c<1 || c>3)){
        return false;
    }
    return true;
}

void tablero_iniciar(Tablero* t,const Configuracion* cfg){
    t->perros_capturados=0;
    pthread_mutex_init(&t->candado,NULL);

    for(int f=0;f<7;f++){
        for(int c=0;c<5;c++){
            if(casilla_valida(f,c)){
                t->casillas[f][c]=VACIA;
            }else{
                t->casillas[f][c]=INVALIDA;
            }
        }
    }

    if(cfg!=NULL && cfg->usar_posiciones_iniciales){
        int pf=cfg->puma_inicial.fila;
        int pc=cfg->puma_inicial.col;
        if(casilla_valida(pf,pc)){
            t->casillas[pf][pc]=PUMA;
        }
        for(int i=0;i<12;i++){
            int df=cfg->perros_iniciales[i].fila;
            int dc=cfg->perros_iniciales[i].col;
            if(casilla_valida(df,dc)){
                t->casillas[df][dc]=PERRO;
            }
        }
    }else{
        t->casillas[0][2]=PUMA;
        int pos[12][2]={
            {6,0},{6,1},{6,2},{6,3},{6,4},
            {5,0},{5,1},{5,2},{5,3},{5,4},
            {4,0},{4,4}
        };
        for(int i=0;i<12;i++){
            int pf=pos[i][0];
            int pc=pos[i][1];
            t->casillas[pf][pc]=PERRO;
        }
    }
}

void tablero_destruir(Tablero* t){
    pthread_mutex_destroy(&t->candado);
}

bool son_adyacentes(int f1,int c1,int f2,int c2){
    if(!casilla_valida(f1,c1) || !casilla_valida(f2,c2)){
        return false;
    }
    if(f1==f2 && (c2==c1+1 || c2==c1-1)){
        return true;
    }
    if(c1==c2 && (f2==f1+1 || f2==f1-1)){
        return true;
    }
    return false;
}

bool es_salto_valido(const Tablero* t,int f1,int c1,int fm,int cm,int f2,int c2){
    if(!casilla_valida(f1,c1) || !casilla_valida(fm,cm) || !casilla_valida(f2,c2)){
        return false;
    }
    if(t->casillas[fm][cm]!=PERRO || t->casillas[f2][c2]!=VACIA){
        return false;
    }
    if(!son_adyacentes(f1,c1,fm,cm) || !son_adyacentes(fm,cm,f2,c2)){
        return false;
    }
    // tiene que seguir en la misma direccion
    int paso1_f=fm-f1;
    int paso1_c=cm-c1;
    int paso2_f=f2-fm;
    int paso2_c=c2-cm;

    if(paso1_f==paso2_f && paso1_c==paso2_c){
        return true;
    }
    return false;
}

bool puma_acorralado(const Tablero* t,int pf,int pc){
    for(int f=0;f<7;f++){
        for(int c=0;c<5;c++){
            if(son_adyacentes(pf,pc,f,c) && t->casillas[f][c]==VACIA){
                return false;
            }
        }
    }

    for(int fm=0;fm<7;fm++){
        for(int cm=0;cm<5;cm++){
            if(son_adyacentes(pf,pc,fm,cm) && t->casillas[fm][cm]==PERRO){
                int f2=fm+(fm-pf);
                int c2=cm+(cm-pc);
                if(es_salto_valido(t,pf,pc,fm,cm,f2,c2)){
                    return false;
                }
            }
        }
    }
    return true;
}

void tablero_imprimir(const Tablero* t,int id_mesa,int j_puma,int j_perros,const char* evento){
    cout<<"\n[Mesa "<<id_mesa<<"] "<<evento<<"\n";
    cout<<"Puma(J"<<j_puma<<") vs Perros(J"<<j_perros<<") - Comidos: "<<t->perros_capturados<<"/6\n";
    for(int f=0;f<7;f++){
        cout<<"  ";
        for(int c=0;c<5;c++){
            if(t->casillas[f][c]==INVALIDA){
                cout<<"  ";
            }else{
                cout<<t->casillas[f][c]<<" ";
            }
        }
        cout<<"\n";
    }
}