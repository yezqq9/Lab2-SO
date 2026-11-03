#include "tablero.hpp"
using namespace std;

bool es_casilla_valida(int fila,int columna){
    if(fila<0 || fila>=FILAS || columna<0 || columna>=COLUMNAS){
        return false;
    }
    if(fila==0 && columna!=2){
        return false;
    }
    if(fila==1 && (columna<1 || columna>3)){
        return false;
    }
    return true;
}

bool son_adyacentes(Posicion a,Posicion b){
    if(!es_casilla_valida(a.fila,a.columna) || !es_casilla_valida(b.fila,b.columna)){
        return false;
    }
    if(a.fila==b.fila && (b.columna==a.columna+1 || b.columna==a.columna-1)){
        return true;
    }
    if(a.columna==b.columna && (b.fila==a.fila+1 || b.fila==a.fila-1)){
        return true;
    }
    return false;
}

void tablero_iniciar(Tablero& tablero,const Configuracion& config){
    pthread_mutex_init(&tablero.candado,nullptr);
    tablero.perros_capturados=0;

    for(int f=0;f<FILAS;f++){
        for(int c=0;c<COLUMNAS;c++){
            if(es_casilla_valida(f,c)){
                tablero.celdas[f][c]=Casilla::Vacia;
            }else{
                tablero.celdas[f][c]=Casilla::Invalida;
            }
        }
    }

    if(config.usar_posiciones_iniciales && (int)config.perros_iniciales.size()==TOTAL_PERROS){
                tablero.posicion_puma=config.puma_inicial;
        tablero.celdas[tablero.posicion_puma.fila][tablero.posicion_puma.columna]=Casilla::Puma;

        for(int i=0;i<TOTAL_PERROS;i++){
            tablero.posicion_perros[i]=config.perros_iniciales[i];
            tablero.perro_vivo[i]=true;
            int pf=tablero.posicion_perros[i].fila;
            int pc=tablero.posicion_perros[i].columna;
            tablero.celdas[pf][pc]=Casilla::Perro;
        }
    }else{
        // formacion por defecto: puma en el trono y 12 perros abajo
        tablero.posicion_puma={0,2};
        tablero.celdas[0][2]=Casilla::Puma;

        int pos[TOTAL_PERROS][2]={
            {6,0},{6,1},{6,2},{6,3},{6,4},
            {5,0},{5,1},{5,2},{5,3},{5,4},
            {4,0},{4,4}
        };
        for(int i=0;i<TOTAL_PERROS;i++){
            int pf=pos[i][0];
            int pc=pos[i][1];
            tablero.posicion_perros[i]={pf,pc};
            tablero.perro_vivo[i]=true;
            tablero.celdas[pf][pc]=Casilla::Perro;
        }
    }
}

void tablero_destruir(Tablero& tablero){
    pthread_mutex_destroy(&tablero.candado);
}

bool tablero_mover_perro(Tablero& tablero,int indice,Posicion destino){
    if(indice<0 || indice>=TOTAL_PERROS){
        return false;
    }
    pthread_mutex_lock(&tablero.candado);

    if(!tablero.perro_vivo[indice]){
        pthread_mutex_unlock(&tablero.candado);
        return false;
    }

    Posicion actual=tablero.posicion_perros[indice];
    if(!son_adyacentes(actual,destino) || tablero.celdas[destino.fila][destino.columna]!=Casilla::Vacia){
        pthread_mutex_unlock(&tablero.candado);
        return false;
    }

    tablero.celdas[actual.fila][actual.columna]=Casilla::Vacia;
    tablero.celdas[destino.fila][destino.columna]=Casilla::Perro;
    tablero.posicion_perros[indice]=destino;

    pthread_mutex_unlock(&tablero.candado);
    return true;
}

bool tablero_mover_puma(Tablero& tablero,Posicion destino){
    pthread_mutex_lock(&tablero.candado);

    Posicion actual=tablero.posicion_puma;
    if(!son_adyacentes(actual,destino) || tablero.celdas[destino.fila][destino.columna]!=Casilla::Vacia){
        pthread_mutex_unlock(&tablero.candado);
        return false;
    }

    tablero.celdas[actual.fila][actual.columna]=Casilla::Vacia;
    tablero.celdas[destino.fila][destino.columna]=Casilla::Puma;
    tablero.posicion_puma=destino;

    pthread_mutex_unlock(&tablero.candado);
    return true;
}

bool tablero_saltar_puma(Tablero& tablero,Posicion destino,int& perro_capturado){
    pthread_mutex_lock(&tablero.candado);

    Posicion origen=tablero.posicion_puma;
    if(!es_casilla_valida(destino.fila,destino.columna) || tablero.celdas[destino.fila][destino.columna]!=Casilla::Vacia){
        pthread_mutex_unlock(&tablero.candado);
        return false;
    }

    int df=destino.fila-origen.fila;
    int dc=destino.columna-origen.columna;

    // el salto ortogonal avanza 2 casillas en fila o 2 en columna
    if(!((df==2 && dc==0) || (df==-2 && dc==0) || (df==0 && dc==2) || (df==0 && dc==-2))){
        pthread_mutex_unlock(&tablero.candado);
        return false;
    }

    Posicion medio={origen.fila+df/2,origen.columna+dc/2};
    if(!son_adyacentes(origen,medio) || !son_adyacentes(medio,destino)){
        pthread_mutex_unlock(&tablero.candado);
        return false;
    }

    if(tablero.celdas[medio.fila][medio.columna]!=Casilla::Perro){
        pthread_mutex_unlock(&tablero.candado);
        return false;
    }

    int idx=-1;
    for(int i=0;i<TOTAL_PERROS;i++){
        if(tablero.perro_vivo[i] && tablero.posicion_perros[i].fila==medio.fila && tablero.posicion_perros[i].columna==medio.columna){
            idx=i;
            break;
        }
    }

    if(idx==-1){
        pthread_mutex_unlock(&tablero.candado);
        return false;
    }

    tablero.perro_vivo[idx]=false;
    tablero.celdas[origen.fila][origen.columna]=Casilla::Vacia;
    tablero.celdas[medio.fila][medio.columna]=Casilla::Vacia;
    tablero.celdas[destino.fila][destino.columna]=Casilla::Puma;
    tablero.posicion_puma=destino;
    tablero.perros_capturados++;
    perro_capturado=idx;

    pthread_mutex_unlock(&tablero.candado);
    return true;
}

bool tablero_puma_acorralado(Tablero& tablero){
    pthread_mutex_lock(&tablero.candado);

    Posicion p=tablero.posicion_puma;
    int df[4]={-1,1,0,0};
    int dc[4]={0,0,-1,1};

    for(int i=0;i<4;i++){
        Posicion vecina={p.fila+df[i],p.columna+dc[i]};
        if(son_adyacentes(p,vecina) && tablero.celdas[vecina.fila][vecina.columna]==Casilla::Vacia){
            pthread_mutex_unlock(&tablero.candado);
            return false;
        }

        Posicion salto={p.fila+2*df[i],p.columna+2*dc[i]};
        if(son_adyacentes(p,vecina) && son_adyacentes(vecina,salto)){
            if(tablero.celdas[vecina.fila][vecina.columna]==Casilla::Perro && tablero.celdas[salto.fila][salto.columna]==Casilla::Vacia){
                pthread_mutex_unlock(&tablero.candado);
                return false;
            }
        }
    }

    pthread_mutex_unlock(&tablero.candado);
    return true;
}

string tablero_a_texto(Tablero& tablero){
    pthread_mutex_lock(&tablero.candado);
    string txt="";
    for(int f=0;f<FILAS;f++){
        txt+="  ";
        for(int c=0;c<COLUMNAS;c++){
            if(tablero.celdas[f][c]==Casilla::Invalida){
                txt+="  ";
            }else{
                txt+=(char)tablero.celdas[f][c];
                txt+=" ";
            }
        }
        txt+="\n";
    }
    pthread_mutex_unlock(&tablero.candado);
    return txt;
}
