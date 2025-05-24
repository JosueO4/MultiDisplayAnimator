#include "myPthreads.h"
#include "scheduler.h"
#include <sys/time.h>
#include <signal.h>

cola_hilos cola_RR;
cola_hilos cola_lottery;
cola_hilos lista_real_time;

extern my_pthread *hilo_actual;


// ===============  DEBUG  =================== //


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

    //printf("Fin de la cola RR.\n");
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
        printf("  [TID: %d, Deadline: %ld segundos]\n", h->tid, h->deadlineSeconds);
        actual = actual->siguiente;
    }
}

// ================================================ //


int esta_en_cola(cola_hilos *cola, my_pthread *hilo) {
    nodo *actual = cola->head;
    while (actual) {
        if (actual->hilo == hilo) return 1;
        actual = actual->siguiente;
    }
    return 0;
}

void manejador_timer(int signum) {
    if (hilo_actual && hilo_actual->estado == CORRIENDO) {
        if (hilo_actual->scheduler == RR) {
            hilo_actual->estado = LISTO;
            encolar(&cola_RR, hilo_actual);
        }
        
        hilo_actual->estado = LISTO;
        // Para SORTEO no hacemos nada, los hilos permanecen en la lista y en real igual
    }
    scheduler();
}

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



my_pthread *scheduler_rr() {
    return desencolar(&cola_RR);
}

my_pthread *scheduler_rt() {

    if (!lista_real_time.head || lista_real_time.size == 0) {
        printf("Real time: Cola vacía\n");
        return NULL;
    }
    nodo *actual = lista_real_time.head;
    return actual->hilo;
}

my_pthread *scheduler_lottery() {
    if (!cola_lottery.head || cola_lottery.size == 0) {
        printf("Lottery: Cola vacía\n");
        return NULL;
    }

    // 1. Calcular total de tickets de hilos LISTOS
    int total_tickets = 0;
    nodo *actual = cola_lottery.head;
    
    printf("Calculando total de tickets...\n");
    while (actual != NULL) {
        if (actual->hilo == NULL) {
            printf("Error: Nodo con hilo NULL encontrado\n");
            actual = actual->siguiente;
            continue;
        }
        
        printf("Hilo %d - Estado: %d - Tickets: %d\n", 
               actual->hilo->tid, actual->hilo->estado, actual->hilo->tickets);
        
        if (actual->hilo->estado == LISTO) {
            total_tickets += actual->hilo->tickets;
        }
        actual = actual->siguiente;
    }
    
    printf("Total tickets: %d\n", total_tickets);
    if (total_tickets == 0) {
        printf("No hay hilos LISTOS disponibles\n");
        return NULL;
    }

    // 2. Seleccionar ganador
    int ganador = rand() % total_tickets;
    printf("Ticket ganador: %d de %d\n", ganador, total_tickets);
    
    actual = cola_lottery.head;
    while (actual != NULL) {
        if (actual->hilo == NULL) {
            printf("Error: Nodo con hilo NULL durante selección\n");
            actual = actual->siguiente;
            continue;
        }
        
        if (actual->hilo->estado == LISTO) {
            printf("Evaluando hilo %d con %d tickets (Ganador restante: %d)\n",
                  actual->hilo->tid, actual->hilo->tickets, ganador);
            
            if (ganador < actual->hilo->tickets) {
                printf("Ganador encontrado: Hilo %d\n", actual->hilo->tid);
                return actual->hilo;
            }
            ganador -= actual->hilo->tickets;
        }
        actual = actual->siguiente;
    }
    
    printf("Error: No se encontró ganador (esto no debería pasar)\n");
    return NULL;
}

int quitar_hilo_de_cola(cola_hilos *cola, my_pthread *hilo) {

    //printf("Estoy intentando sacar un hilo\n");
    
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
            return 1; // éxito
        }
        anterior = actual;
        actual = actual->siguiente;
    }

    return 0; // hilo no encontrado
}

void sacar(my_pthread* hilo, tipo_scheduler tipo){

  if (tipo == RR) {
      printf("Borrado de la cola RR\n");
      quitar_hilo_de_cola(&cola_RR, hilo);
  } else if (tipo== SORTEO) {
      printf("Borrado de la cola LT\n");
      quitar_hilo_de_cola(&cola_lottery, hilo);
  } else if (tipo == TIEMPOREAL) {
      //printf("Borrado de la cola RT\n");
      quitar_hilo_de_cola(&lista_real_time, hilo);
  }
}




void scheduler() {
    
    my_pthread *prev = hilo_actual;
    tipo_scheduler tipo = hilo_actual->scheduler;
    
     if(tipo == RR){
        //printf("RR2\n");
        imprimir_cola_rr(&cola_RR);
        hilo_actual = scheduler_rr();

    } else if(tipo == TIEMPOREAL){
        //printf("RT2\n");
        //imprimir_lista_real_time(&lista_real_time);
        hilo_actual = scheduler_rt();
    }else{
        //printf("LT2\n");
        imprimir_cola_sorteo(&cola_lottery);
        hilo_actual = scheduler_lottery();
    }
    
    if (hilo_actual == NULL) {
        printf("No hay más hilos activos. Saliendo...\n");
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


void insertar_ordenado_por_prioridad(cola_hilos *cola, my_pthread *hilo) {
    // Validar parámetros
    if (!cola || !hilo) {
        fprintf(stderr, "Error: Parámetros inválidos\n");
        return;
    }

    // Crear nuevo nodo
    nodo *nuevo = malloc(sizeof(nodo));
    if (!nuevo) {
        perror("Error al asignar memoria para nodo");
        exit(EXIT_FAILURE);
    }
    nuevo->hilo = hilo;
    nuevo->siguiente = NULL;

    // Caso 1: Lista vacía
    if (!cola->head) {
        cola->head = nuevo;
        cola->tail = nuevo;
        cola->size++;
        return;
    }
    
    // Caso 2: Insertar al inicio (nuevo deadline es menor que el primero)
    if (hilo->deadlineSeconds < cola->head->hilo->deadlineSeconds) {

        nuevo->siguiente = cola->head;
        cola->head = nuevo;
        cola->size++;
        return;
    }

    // Caso 3: Buscar posición adecuada
    nodo *actual = cola->head;
    nodo *anterior = NULL;
    int contador = 1;
    while (actual && actual->hilo->deadlineSeconds <= hilo->deadlineSeconds) {
        anterior = actual;
        actual = actual->siguiente;
        contador++;
    }

    // Insertar el nuevo nodo
    if (anterior) {
        anterior->siguiente = nuevo;
    }
    nuevo->siguiente = actual;

    // Actualizar tail si estamos insertando al final
    if (!actual) {
        cola->tail = nuevo;
    }

    cola->size++;
}


void meter(tipo_scheduler tipo, my_pthread* hilo) {
    if (tipo == TIEMPOREAL) {
        insertar_ordenado_por_prioridad(&lista_real_time, hilo);
    } else if(tipo == RR) {
        encolar(&cola_RR, hilo);   
    } else {
    
        // Para SORTEO, solo añadir si no está ya en la lista
        if (!esta_en_cola(&cola_lottery, hilo)) {
            encolar(&cola_lottery, hilo);
        }
    }
}
