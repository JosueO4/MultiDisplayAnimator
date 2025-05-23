
#include "myPthreads.h"
#include "scheduler.h"
#include <sys/time.h>
#include <signal.h>

cola_hilos cola_RR;
cola_hilos cola_lottery;
cola_hilos lista_real_time;

extern my_pthread *hilo_actual;

void manejador_timer(int signum) {
    // Reencolar el hilo actual si es RR y no ha terminado
    if (hilo_actual && hilo_actual->scheduler == RR && hilo_actual->estado != TERMINADO) {
        hilo_actual->estado = LISTO;
        encolar(&cola_RR, hilo_actual);
    }

    // Invocar al scheduler
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
    return desencolar(&lista_real_time);
}

my_pthread *scheduler_lottery() {

    if (cola_lottery.head == NULL) return NULL;

    int total_tickets = 0;
    nodo *n = cola_lottery.head;
    while (n) {
        total_tickets += n->hilo->tickets;
        n = n->siguiente;
    }

    if (total_tickets == 0) return desencolar(&cola_lottery); // fallback

    int ganador = rand() % total_tickets;
    n = cola_lottery.head;
    nodo *anterior = NULL;

    while (n) {

        if (ganador < n->hilo->tickets) {
            // Sacar de la cola
            if (anterior) anterior->siguiente = n->siguiente;
            else cola_lottery.head = n->siguiente;
            if (n == cola_lottery.tail) cola_lottery.tail = anterior;

            my_pthread *ganador_hilo = n->hilo;
            free(n);
            return ganador_hilo;
        }
        ganador -= n->hilo->tickets;
        anterior = n;
        n = n->siguiente;
    }
    return NULL;
}

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
      printf("Borrado de la cola RT\n");
      quitar_hilo_de_cola(&lista_real_time, hilo);
  }
}




void scheduler() {

    my_pthread *prev = hilo_actual;

    tipo_scheduler tipo = hilo_actual->scheduler;
    
    if(tipo == RR){
        printf("RR2\n");
        hilo_actual = scheduler_rr();

    } else if(tipo == TIEMPOREAL){
        printf("RT2\n");
        hilo_actual = scheduler_rt();
    }else{
        printf("LT2\n");
        hilo_actual = scheduler_lottery();
    }

    if (hilo_actual == NULL) exit(0);

    hilo_actual->estado = CORRIENDO;

     if (hilo_actual->scheduler == RR && hilo_actual->quantum <= 0) {
        hilo_actual->quantum = 2;
    }
    
    if(prev && prev->estado != TERMINADO) {
        swapcontext(&prev->contexto, &hilo_actual->contexto);
    } else {
        setcontext(&hilo_actual->contexto);
    }
}

void insertar_ordenado_por_prioridad(cola_hilos *lista, my_pthread *hilo) {
    nodo *nuevo = (nodo *)malloc(sizeof(nodo));
    nuevo->hilo = hilo;
    nuevo->siguiente = NULL;

    // Si la lista está vacía o el nuevo tiene menor deadline
    if (lista->head == NULL || hilo->deadlineSeconds < lista->head->hilo->deadlineSeconds) {
        nuevo->siguiente = lista->head;
        lista->head = nuevo;
        if (lista->tail == NULL) {
            lista->tail = nuevo;
        }
    } else {
        nodo *actual = lista->head;
        while (actual->siguiente != NULL &&
               actual->siguiente->hilo->deadlineSeconds <= hilo->deadlineSeconds) {
            actual = actual->siguiente;
        }
        nuevo->siguiente = actual->siguiente;
        actual->siguiente = nuevo;
        if (nuevo->siguiente == NULL) {
            lista->tail = nuevo;
        }
    }

    lista->size++;
}

void meter(tipo_scheduler tipo, my_pthread* nuevo_hilo){

    if (tipo == TIEMPOREAL) {
        insertar_ordenado_por_prioridad(&lista_real_time,nuevo_hilo);
         printf("RT\n");
    }else if(tipo == RR){
        printf("RR\n");
        encolar(&cola_RR, nuevo_hilo);   
    }
    else {
        printf("LT\n");
        encolar(&cola_lottery, nuevo_hilo);
    }
}
