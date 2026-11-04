# Komikan Concurrente

Tarea 2 de Sistemas Operativos, Universidad Diego Portales.

Integrantes: Bastián Ampuero y Guliano Bardi.

## Qué es

Es una simulación de un torneo de Komikan donde las fichas juegan en tiempo real, sin turnos. Un servidor recibe a N clientes, los empareja de a dos (uno juega al puma y el otro a los perros) y les abre una mesa. Nunca hay más de M mesas jugando al mismo tiempo. Cada mesa tiene 14 hilos: el puma, los 12 perros y un juez que imprime la partida y decide quién gana.

## Cómo compilar

El proyecto está en C++17 y usa solo primitivas POSIX (pthread_mutex, pthread_cond y sem_t). Se compila con:

    make

que ejecuta g++ con las banderas -Wall -Wextra -std=c++17 -pthread, sin warnings. El ejecutable queda en la carpeta del proyecto con el nombre torneo. Para borrar lo compilado se usa make clean. El Makefile genera las dependencias de los headers, así que si se cambia un .hpp basta con volver a correr make.

## Cómo ejecutar

    ./torneo N M [config.txt]

N es el total de clientes que llegan al servidor y M es el máximo de mesas simultáneas. Ambos deben ser enteros positivos. El archivo de configuración es opcional. Si N es impar, el último cliente se queda sin pareja y no juega. Algunos ejemplos:

    ./torneo 4 2
    ./torneo 10 3 config.txt
    ./torneo 4 2 config_rapido.txt

Si los argumentos son incorrectos, o el archivo no existe o tiene errores, el programa muestra un mensaje y termina sin iniciar el torneo.

## Archivo de configuración

Es un archivo de texto con una línea clave=valor. Se pueden dejar líneas en blanco y comentarios que empiezan con #. Las claves que acepta son:

- retardo_min_ms y retardo_max_ms: tiempo mínimo y máximo que duerme cada ficha antes de intentar moverse. Por defecto son 50 y 300, y el mínimo no puede ser mayor al máximo.
- semilla: semilla del generador aleatorio, para repetir pruebas. Cada ficha usa además su propia semilla derivada de esta, así no comparten el generador. Como el planificador del sistema operativo no es determinista, la semilla ayuda a repetir las pruebas pero no garantiza partidas idénticas.
- puma=fila,columna y perro=fila,columna: posición inicial de las fichas. Si se usa alguna de estas claves hay que definir exactamente un puma y 12 perros, sin casillas repetidas. Si no se define ninguna, se usa la formación por defecto: el puma en el trono (0,2) y los perros en las filas 6 y 5 completas más las casillas (4,0) y (4,4).
- abandono=puma o abandono=perros: sirve para probar el manejo de fallos. Hace que esa ficha abandone la partida en su sexta jugada (en el caso de los perros, solo el perro 1).

Un ejemplo mínimo:

    retardo_min_ms=5
    retardo_max_ms=20
    semilla=42

En el repositorio hay varios ejemplos: config.txt, config_rapido.txt, config_posiciones.txt y config_abandono.txt.

## El tablero y las reglas

El tablero es una matriz de 7 filas por 5 columnas. La fila 0 solo tiene válida la columna 2 (el trono), la fila 1 solo las columnas 1 a 3, y las filas 2 a 6 tienen las 5 columnas. Las demás casillas son inválidas y se muestran en blanco. Tomamos "adyacente" como movimiento ortogonal (arriba, abajo, izquierda y derecha), sin diagonales, y el salto del puma también es en línea recta sobre un perro hacia la casilla vacía de atrás. El puma gana al capturar 6 perros y los perros ganan si el puma queda sin ningún movimiento ni salto posible.

En consola se imprime el tablero después de cada evento junto con la cuenta de capturas, y al final de cada mesa se indica quién ganó.

## Decisiones de sincronización

**Tablero.** Cada tablero tiene un solo mutex. Todas las operaciones sobre él (mover un perro, mover el puma, saltar, revisar si el puma está acorralado e imprimir) toman el candado al entrar y lo sueltan al salir. Con esto, si dos fichas quieren la misma casilla, la que entra primero al mutex se la queda y la otra ve la casilla ocupada y prueba otra dirección. Elegimos un solo mutex y no uno por casilla porque el salto del puma toca tres casillas a la vez, y tomar varios candados habría abierto la posibilidad de deadlock. Dentro de la sección crítica nunca se toma otro candado, así que no hay orden de bloqueo que cuidar.

**Registro de eventos.** Las fichas avisan al juez con registrar_evento, que guarda el texto en una lista protegida por otro mutex (candado_eventos) y despierta al juez con una variable de condición. El juez duerme en pthread_cond_wait, sin gastar CPU, y cada vez que despierta se lleva toda la lista de una vez y la imprime. Antes guardábamos solo el último evento y el registro salía desfasado, por eso ahora es una lista.

**Límite de mesas.** El servidor usa un semáforo POSIX (cupos_mesas) que parte en M. Antes de abrir una mesa hace sem_wait, y si las M están ocupadas se queda bloqueado ahí sin consumir CPU. Cuando una partida termina, el juez llama a un callback del servidor que quita la mesa de la lista de activas y hace sem_post, liberando el cupo. La lista de mesas activas y la de terminadas se protegen con otro mutex (candado_mesas).

**Fin de las mesas.** Un juez no puede hacerse pthread_join a sí mismo. Por eso, cuando termina, deja su mesa en una lista de terminadas y es el servidor el que después hace el join y destruye la mesa. Cada juez hace join de sus 13 fichas antes de avisar al servidor, así que no quedan hilos sueltos.

**Fallos y abandono.** Si falla un pthread_create del puma o de algún perro, el juez lo toma como abandono de ese jugador, declara ganador al oponente por abandono y cierra la mesa de forma ordenada. Para poder probarlo se agregó la clave abandono del config.

**Ctrl+C.** El manejador de SIGINT solo cambia una variable atómica (aceptando_clientes), porque dentro de un manejador no es seguro tomar mutexes. Se instala sin SA_RESTART para que el sem_wait del servidor se interrumpa. Al detectarlo, el servidor deja de aceptar clientes y llama a mesa_pedir_parada en cada mesa activa. Las fichas, el juez y el servidor revisan una bandera atómica (detener) para terminar. Al final, servidor_destruir espera a recuperar los M cupos del semáforo, lo que indica que todas las mesas terminaron, hace los joins pendientes y destruye mutexes y semáforos.

## Pruebas realizadas

Se probó con Valgrind (valgrind --leak-check=full) una partida normal, el caso de abandono y una ejecución interrumpida con Ctrl+C. En los tres casos todos los bloques de memoria quedaron liberados y sin errores. Después de Ctrl+C el programa termina solo con código de salida 0. También se probaron argumentos incorrectos y un archivo de configuración inexistente.

## Estructura del repositorio

- include/: headers. tipos.hpp tiene las estructuras compartidas, tablero.hpp y mesa.hpp son del juego, y servidor.hpp es del servidor.
- src/tablero.cpp y src/mesa.cpp: reglas del tablero, hilos de las fichas y juez.
- src/servidor.cpp y src/main.cpp: lectura del config, emparejamiento, límite de mesas y manejo de SIGINT.
- Makefile y archivos config*.txt de ejemplo.
- El informe de diseño en PDF.
