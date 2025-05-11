#ifndef MY_PTHREADS_H
#define MY_PTHREADS_H

#include <ucontext.h>
#include <stdlib.h>
#include <stdio.h>

#define STACK_SIZE (640 * 1024) 

typedef enum {
    CORRIENDO,
    LISTO,
    BLOQUEADO,
    TERMINADO
} thread_state_t;

// Estructura de hilo
typedef struct {
    int tid;
    ucontext_t contexto;
    thread_state_t estado;
    void *retval;
    int vinculado;
    struct my_pthread *thread_esperando;
} my_pthread;

// nodo para cola
typedef struct nodo {
    my_pthread *hilo;
    struct nodo *siguiente;
} nodo;

// cola de hilos para RR
typedef struct {
    nodo *head;
    nodo *tail;
    int size;
} cola_hilos;

// metodos
int my_pthread_create(my_pthread **hilo, void (*start_routine)(void *), void *arg);
void my_pthread_yield();
void my_pthread_end(void *retval);
void scheduler();
int my_pthread_join(my_pthread *hilo, void **retval);
int my_pthread_detach(my_pthread *hilo);

// metodos de la cola
void cola_init(cola_hilos *cola);
void encolar(cola_hilos *cola, my_pthread *hilo);
my_pthread *desencolar(cola_hilos *cola);


#endif