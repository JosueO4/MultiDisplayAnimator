#include "myPthreads.h"
#include "scheduler.h"
#include <sys/time.h>
#include <signal.h>

// Variables globales (cola/lista para schedulers, hilo_actual)

cola_hilos cola_RR;
cola_hilos cola_lottery;
cola_hilos lista_real_time;
extern my_pthread *hilo_actual;


// === FUNCIONES PARA DEBUG O PARA MOSTRAR FUNCIONAMIENTO === //

void imprimir_cola_rr(cola_hilos *cola) {

    printf("Cola RR (tamaño: %d):\n", cola->size);

    if (!cola->head) {
        printf("  (vacía)\n");
        return;
    }

    nodo *actual = cola->head;

    while (actual) {
        my_pthread *h = actual->hilo;
        printf("  [TID: %d]\n", h->tid);
        actual = actual->siguiente;
    }
}

void imprimir_cola_sorteo(cola_hilos *cola) {

    printf("Cola Sorteo (tamaño: %d):\n", cola->size);

    if (!cola->head) {
        printf("  (vacía)\n");
        return;
    }

    nodo *actual = cola->head;

    while (actual) {
        my_pthread *h = actual->hilo;
        printf("  [TID: %d, Tickets: %d]\n", h->tid, h->tickets);
        actual = actual->siguiente;
    }
}

void imprimir_lista_real_time(cola_hilos *lista) {

    printf("Lista Tiempo Real (tamaño: %d):\n", lista->size);

    if (!lista->head) {
        printf("  (vacía)\n");
        return;
    }

    nodo *actual = lista->head;
    
    while (actual) {
        my_pthread *h = actual->hilo;
        printf("  [TID: %d, Deadline: %d segundos]\n", h->tid, h->deadlineSeconds);
        actual = actual->siguiente;
    }
}

/*
Función: Verifica si un hilo específico está presente en una cola de hilos.

Entrada:
- cola: Puntero a la cola de hilos donde se buscará.
- hilo: Puntero al hilo que se desea buscar.

Salida: 1 si el hilo está en la cola, 0 si no está.
*/

int esta_en_cola(cola_hilos *cola, my_pthread *hilo) {
    nodo *actual = cola->head;
    while (actual) {
        if (actual->hilo == hilo) return 1;
        actual = actual->siguiente;
    }
    return 0;
}

/*
Función: Manejador de interrupción del temporizador que implementa el time-slicing.

Entrada:
- signum: Número de señal (siempre será SIGALRM en este caso).

Salida: Ninguna
*/

void manejador_timer(int signum) {
    if (hilo_actual && hilo_actual->estado == CORRIENDO) {
        if (hilo_actual->scheduler == RR) {
            hilo_actual->estado = LISTO;
            encolar(&cola_RR, hilo_actual);
        }

        if(hilo_actual->scheduler == TIEMPOREAL){
            hilo_actual->estado = LISTO;
            hilo_actual->deadlineSeconds = hilo_actual->deadlineSeconds + 100;
        }

        hilo_actual->estado = LISTO;
        
    }

    scheduler();
}

/*
Función: Configura e inicia el temporizador para el scheduler.

Entrada: Ninguna

Salida: Ninguna
*/

void iniciar_timer_scheduler() {
    struct sigaction sa;
    struct itimerval timer;

    // Configurar manejador
    sa.sa_handler = manejador_timer;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);

    // Temporizador de 100ms
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 100000;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 100000;

    setitimer(ITIMER_REAL, &timer, NULL);
}

/*
Función: Encuentra el hilo de tiempo real con el deadline más cercano.

Entrada: Ninguna

Salida: Puntero al hilo con deadline más cercano, o NULL si no hay hilos.
*/

my_pthread* encontrar_mas_cercano() {
    if (lista_real_time.head == NULL) return NULL;

    nodo* nodo_actual = lista_real_time.head;
    my_pthread* mas_cercano = nodo_actual->hilo;

    while (nodo_actual != NULL) {
        if (nodo_actual->hilo->deadlineSeconds < mas_cercano->deadlineSeconds) {
            mas_cercano = nodo_actual->hilo;
        }
        nodo_actual = nodo_actual->siguiente;
    }

    return mas_cercano;
}

/*
Función: Implementa el algoritmo de scheduling Round Robin.

Entrada: Ninguna

Salida: Puntero al siguiente hilo a ejecutar, o NULL si la cola está vacía.
*/

my_pthread *scheduler_rr() {
    return desencolar(&cola_RR);
}

/*
Función: Implementa el algoritmo de scheduling para tiempo real (EDF).

Entrada: Ninguna

Salida: Puntero al hilo de tiempo real con deadline más cercano, o NULL si no hay hilos.
*/

my_pthread *scheduler_rt() {

    if (!lista_real_time.head || lista_real_time.size == 0) {
        return NULL;
    }

    my_pthread* actual = encontrar_mas_cercano();
    
    return actual;
}

/*
Función: Implementa el algoritmo de scheduling por lotería.

Entrada: Ninguna

Salida: Puntero al hilo ganador de la lotería, o NULL si no hay hilos disponibles.
*/

my_pthread *scheduler_lottery() {

    if (!cola_lottery.head || cola_lottery.size == 0) {
        return NULL;
    }

    // Calcular tickets

    int total_tickets = 0;
    nodo *actual = cola_lottery.head;
    
    while (actual != NULL) {
        if (actual->hilo == NULL) {
            actual = actual->siguiente;
            continue;
        }
        
        if (actual->hilo->estado == LISTO) {
            total_tickets += actual->hilo->tickets;
        }
        actual = actual->siguiente;
    }
    
    if (total_tickets == 0) {
        return NULL;
    }

    // Sacar ganador

    int ganador = rand() % total_tickets;

    actual = cola_lottery.head;

    while (actual != NULL) {

        if (actual->hilo == NULL) {
            actual = actual->siguiente;
            continue;
        }
        
        if (actual->hilo->estado == LISTO) {

            if (ganador < actual->hilo->tickets) {
                return actual->hilo;
            }
            ganador -= actual->hilo->tickets;
        }
        actual = actual->siguiente;
    }
    
    return NULL;
}

/*
Función: Elimina un hilo específico de una cola de hilos.

Entrada:
- cola: Puntero a la cola de hilos.
- hilo: Puntero al hilo que se desea remover.

Salida: 1 si se eliminó exitosamente, 0 si el hilo no se encontró.
*/

int quitar_hilo_de_cola(cola_hilos *cola, my_pthread *hilo) {

    nodo *actual = cola->head;

    nodo *anterior = NULL;

    while (actual != NULL) {

        if (actual->hilo == hilo) {

            if (anterior != NULL) {
                anterior->siguiente = actual->siguiente;
            } else {
                cola->head = actual->siguiente;
            }

            if (actual == cola->tail) {
                cola->tail = anterior;
            }

            free(actual);
            cola->size--;
            return 1;
        }
        anterior = actual;
        actual = actual->siguiente;
    }

    return 0;
}

/*
Función: Remueve un hilo de la cola correspondiente a su scheduler.

Entrada:
- hilo: Puntero al hilo que se desea sacar.
- tipo: Tipo de scheduler al que pertenece el hilo.

Salida: Ninguna
*/

void sacar(my_pthread* hilo, tipo_scheduler tipo){

  if (tipo == RR) {

      quitar_hilo_de_cola(&cola_RR, hilo);
  } else if (tipo== SORTEO) {

      quitar_hilo_de_cola(&cola_lottery, hilo);
  } else if (tipo == TIEMPOREAL) {
      
      quitar_hilo_de_cola(&lista_real_time, hilo);
  }
}

/*
Función: Selecciona el siguiente hilo a ejecutar según el scheduler apropiado.

Entrada: Ninguna

Salida: Ninguna
*/

void scheduler() {
    
    my_pthread *prev = hilo_actual;

    tipo_scheduler tipo = hilo_actual->scheduler;
    
     if(tipo == RR){

        hilo_actual = scheduler_rr();

    } else if(tipo == TIEMPOREAL){
        //imprimir_lista_real_time(&lista_real_time);
        hilo_actual = scheduler_rt();
        
    }else{
        
        hilo_actual = scheduler_lottery();
    }
    
    if (hilo_actual == NULL) {
        return;
    }
        
    hilo_actual->estado = CORRIENDO;
    
    if(prev && prev->estado != TERMINADO) {
        swapcontext(&prev->contexto, &hilo_actual->contexto);
        swapcontext(&hilo_actual->contexto, &prev->contexto);
        
    } else {
        
        setcontext(&hilo_actual->contexto);
    }
}

/*
Función: Inserta un hilo en una cola manteniendo orden por prioridad (deadline).

Entrada:
- cola: Puntero a la cola de hilos.
- hilo: Puntero al hilo que se desea insertar.

Salida: Ninguna
*/

void insertar_ordenado_por_prioridad(cola_hilos *cola, my_pthread *hilo) {
    
    if (!cola || !hilo) {
        fprintf(stderr, "Error: Parámetros inválidos\n");
        return;
    }

    
    nodo *nuevo = malloc(sizeof(nodo));

    if (!nuevo) {
        perror("Error al asignar memoria para nodo");
        exit(EXIT_FAILURE);
    }

    nuevo->hilo = hilo;
    nuevo->siguiente = NULL;

    
    if (!cola->head) {
        cola->head = nuevo;
        cola->tail = nuevo;
        cola->size++;
        return;
    }
    
    
    if (hilo->deadlineSeconds < cola->head->hilo->deadlineSeconds) {
        nuevo->siguiente = cola->head;
        cola->head = nuevo;
        cola->size++;
        return;
    }

    
    nodo *actual = cola->head;
    nodo *anterior = NULL;
    int contador = 1;

    while (actual && actual->hilo->deadlineSeconds <= hilo->deadlineSeconds) {
        anterior = actual;
        actual = actual->siguiente;
        contador++;
    }

    
    if (anterior) {
        anterior->siguiente = nuevo;
    }
    nuevo->siguiente = actual;

    
    if (!actual) {
        cola->tail = nuevo;
    }

    cola->size++;
}

/*
Función: Añade un hilo a la cola correspondiente según su tipo de scheduler.

Entrada:
- tipo: Tipo de scheduler del hilo.
- hilo: Puntero al hilo que se desea encolar.

Salida: Ninguna
*/

void meter(tipo_scheduler tipo, my_pthread* hilo) {
    if (tipo == TIEMPOREAL) {

        insertar_ordenado_por_prioridad(&lista_real_time, hilo);

    } else if(tipo == RR) {

        encolar(&cola_RR, hilo);   
    } else {
        
        if (!esta_en_cola(&cola_lottery, hilo)) {
            encolar(&cola_lottery, hilo);
        }
    }
}
