#pragma once
#include <pthread.h>
#include <semaphore.h>
#include <atomic>
#include <string>
#include <vector>

constexpr int FILAS=7;
constexpr int COLUMNAS=5;
constexpr int TOTAL_PERROS=12;
constexpr int CAPTURAS_PARA_GANAR=6;

enum class Casilla : char {
    Invalida='X',
    Vacia='.',
    Puma='P',
    Perro='D'
};

enum class Rol { Puma, Perros };
enum class Ganador { Ninguno, Puma, Perros };
enum class MotivoFin { Victoria, Abandono, Interrupcion };

struct Posicion {
    int fila;
    int columna;
};

struct Configuracion {
    int retardo_min_ms=50;
    int retardo_max_ms=300;
    unsigned int semilla=0;
    bool usar_posiciones_iniciales=false;
    Posicion puma_inicial{3,2};
    std::vector<Posicion> perros_iniciales;
};

struct Tablero {
    Casilla celdas[FILAS][COLUMNAS];
    Posicion posicion_puma;
    Posicion posicion_perros[TOTAL_PERROS];
    bool perro_vivo[TOTAL_PERROS];
    int perros_capturados;
    pthread_mutex_t candado;            // protege todo el tablero
};

struct Jugador {
    int id;
    Rol rol;
};

struct Mesa;

// el servidor se entera de que una mesa termino con este callback
using AlTerminarMesa=void (*)(void* contexto,Mesa* mesa);

struct Mesa {
    int id;
    Configuracion config;
    Tablero tablero;
    Jugador jugador_puma;
    Jugador jugador_perros;

    pthread_t hilo_juez;
    pthread_t hilo_puma;
    pthread_t hilos_perros[TOTAL_PERROS];

    std::atomic<bool> detener;          // pide a todas las fichas que paren
    std::atomic<bool> abandono_puma;
    std::atomic<bool> abandono_perros;

    pthread_mutex_t candado_eventos;    // protege el contador y el ultimo evento
    pthread_cond_t hay_evento;          // despierta al juez para imprimir
    int cantidad_eventos;
    std::string ultimo_evento;

    Ganador ganador;
    MotivoFin motivo_fin;

    AlTerminarMesa al_terminar;
    void* contexto_servidor;
};

struct ArgumentosFicha {
    Mesa* mesa;
    int indice;                         // 0..11 para perros, ignorado en puma
    unsigned int semilla;               // semilla de cada ficha
};

struct Servidor {
    int total_clientes;
    int max_mesas;
    Configuracion config;

    std::atomic<bool> aceptando_clientes;
    sem_t cupos_mesas;                  // mesas libres, inicia en M

    pthread_mutex_t candado_mesas;      // protege la lista de mesas activas
    std::vector<Mesa*> mesas_activas;
    int partidas_terminadas;
};
