#ifndef MY_PTHREADS_H
#define MY_PTHREADS_H
#include <ucontext.h>
#include <stdlib.h>
#include <stdio.h>


#define STACK_SIZE (640 * 1024) 

// Estados de un hilo

typedef enum {
    CORRIENDO,
    LISTO,
    BLOQUEADO,
    TERMINADO
} thread_state_t;

// Tipos de scheduler

typedef enum{
    RR,
    SORTEO,
    TIEMPOREAL
} tipo_scheduler;


// Atributos de un hilo

typedef struct {
    int tid;
    ucontext_t contexto;
    thread_state_t estado;
    void *retval;
    int vinculado;
    struct my_pthread *thread_esperando;
    tipo_scheduler scheduler;
    int tickets; 
    int deadlineSeconds; 
    int quantum;
} my_pthread;


// Nodo para una cola

typedef struct nodo {
    my_pthread *hilo;
    struct nodo *siguiente;
} nodo;


// Cola

typedef struct {
    nodo *head;
    nodo *tail;
    int size;
} cola_hilos;


// Atributos del mutex

typedef struct {
    int bloqueado;
    my_pthread *duenio;
    cola_hilos *cola_esperando;
} my_mutex;


// Metodos

int my_pthread_create(my_pthread **hilo, tipo_scheduler tipo, void (*start_routine)(void *), void *arg, int deadline);
void my_pthread_yield();
void my_pthread_end(void *retval);
void scheduler();
int my_pthread_join(my_pthread *hilo, void **retval);
int my_pthread_detach(my_pthread *hilo);

// Métodos de cola

void cola_init(cola_hilos *cola);
void encolar(cola_hilos *cola, my_pthread *hilo);
my_pthread *desencolar(cola_hilos *cola);


// Metodos de los mutexes 

int my_mutex_init(my_mutex *mutex);
int my_mutex_lock(my_mutex *mutex);
int my_mutex_unlock(my_mutex *mutex);
int my_mutex_destroy(my_mutex *mutex);
int my_mutex_trylock(my_mutex *mutex);

// Funcion adicional
 
void my_pthread_chsched(my_pthread *hilo, tipo_scheduler nuevo_scheduler);


#endif
