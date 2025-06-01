
#include "myPthreads.h"

// Metodos que pueden ser utilizados fuera de scheduler.c

void scheduler();
void iniciar_timer_scheduler();
void meter(tipo_scheduler tipo, my_pthread* nuevo_hilo);
void sacar(my_pthread* hilo, tipo_scheduler tipo);
void imprimir_cola_rr(cola_hilos *cola);
void imprimir_cola_sorteo(cola_hilos *cola);
void imprimir_lista_real_time(cola_hilos *lista);
