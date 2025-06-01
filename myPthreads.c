#include "myPthreads.h"
#include "scheduler.h"
#include <ucontext.h>

// Variables globales (hilo_actual, contexto del hilo principal, tid para asignarle a los hilos, contador de hilos activos)

cola_hilos cola;
my_pthread *hilo_actual = NULL;
int tid_contador = 1;
int hilos_activos = 0;
ucontext_t contexto_principal;

/*
Función: Inicializa una cola vacía de hilos, dejando sus punteros de cabeza y cola en NULL y su tamaño en cero.

Entrada:
- cola: puntero a la estructura de tipo cola_hilos a inicializar.

Salida: No tiene valor de retorno, pero modifica la estructura de la cola inicializándola.
*/

void cola_init(cola_hilos *cola) {

    cola->head = NULL;
    cola->tail = NULL;
    cola->size = 0;
}

/*
Función: Agrega un hilo al final de la cola de hilos, ajustando los punteros de la cola y aumentando el tamaño.

Entrada:
- cola: puntero a la estructura de tipo cola_hilos donde se insertará el hilo.
- hilo: puntero al hilo (my_pthread *) que se desea agregar.

Salida: No tiene valor de retorno, pero modifica la estructura de la cola insertando el hilo al final.
*/

void encolar(cola_hilos *cola, my_pthread *hilo) {

    nodo *nuevo_nodo = (nodo *)malloc(sizeof(nodo));
    nuevo_nodo->hilo = hilo;
    nuevo_nodo->siguiente = NULL;

    if (cola->head == NULL) {
        cola->head = nuevo_nodo;
        cola->tail = nuevo_nodo;
    } else {
        cola->tail->siguiente = nuevo_nodo;
        cola->tail = nuevo_nodo;
    }
    cola->size++;
}

/*
Función: Extrae el primer hilo de la cola de hilos y devuelve un puntero a él. Ajusta los punteros de la cola 
y decrementa el tamaño.

Entrada:
- cola: puntero a la estructura de tipo cola_hilos de donde se extraerá el hilo.

Salida: Devuelve un puntero al hilo (my_pthread *) que se encontraba en la cabeza de la cola, o 
NULL si la cola está vacía.
*/

my_pthread *desencolar(cola_hilos *cola) {

    if (cola->head == NULL) {
        return NULL;
    }

    nodo *nodo_a_eliminar = cola->head;
    my_pthread *hilo = nodo_a_eliminar->hilo;

    cola->head = nodo_a_eliminar->siguiente;
    if (cola->head == NULL) {
        cola->tail = NULL;
    }
    free(nodo_a_eliminar);
    cola->size--;
    return hilo;
}

/*
Función: Crea un hilo, le asigna un scheduler, la función que hace, los argumentos y un deadline

Entrada:
- hilo: puntero al hilo (my_pthread *) que se desea crear.
- tipo: tipo de scheduler al que pertenece el hilo
- start_rountine: función que hace
- arg: argumentos
- deadline: Se maneja como un tiempo de inicio pero posteriormente sirve para decidir que hilo ejecutar dandole prioridad
al más cercano

Salida: 0 si todo salio bien, 1 si algo falló
*/

int my_pthread_create(my_pthread **hilo, tipo_scheduler tipo, void (*start_routine)(void *), void *arg, int deadline) {

    my_pthread *nuevo_hilo = (my_pthread *)malloc(sizeof(my_pthread));

    if (nuevo_hilo == NULL) return -1;

    getcontext(&nuevo_hilo->contexto);
    nuevo_hilo->contexto.uc_stack.ss_sp = malloc(STACK_SIZE);
    nuevo_hilo->contexto.uc_stack.ss_size = STACK_SIZE;
    nuevo_hilo->contexto.uc_link = NULL;
    nuevo_hilo->tid = tid_contador++;
    nuevo_hilo->estado = LISTO;
    nuevo_hilo->retval = NULL;
    nuevo_hilo->thread_esperando = NULL;
    nuevo_hilo->vinculado = 0;
    nuevo_hilo->scheduler = tipo;
    nuevo_hilo->deadlineSeconds = deadline;
    makecontext(&nuevo_hilo->contexto, (void (*)(void))start_routine, 1, arg);
    
    meter(tipo,nuevo_hilo);

    if (hilo) *hilo = nuevo_hilo;

    hilos_activos++;

    return 0;
}

/*
Función: Cede voluntariamente el control de la CPU para permitir que otros hilos se ejecuten.

Entrada: Ninguna

Salida: Ninguna
*/

void my_pthread_yield() {

    if (!hilo_actual) return;
    
    if (hilo_actual->estado != TERMINADO) {

        hilo_actual->estado = LISTO;
        
        if (hilo_actual->scheduler == RR) {
            meter(hilo_actual->scheduler, hilo_actual);
        }
    }
    scheduler();
}

/*
Función: Termina la ejecución del hilo actual y libera sus recursos.

Entrada:
- retval: Valor de retorno que se pasará a cualquier hilo que esté haciendo join.

Salida: Ninguna
*/

void my_pthread_end(void *retval) {

    
    hilo_actual->estado = TERMINADO;
    hilo_actual->retval = retval;
    tipo_scheduler tipo = hilo_actual->scheduler;
    sacar(hilo_actual,tipo);
    hilos_activos--;
    
    if (hilos_activos == 0) {
        setcontext(&contexto_principal);
    }
    
    if (hilo_actual->thread_esperando != NULL) {
        meter(tipo,hilo_actual->thread_esperando);
    }
    
    if (hilo_actual->vinculado) {
        free(hilo_actual->contexto.uc_stack.ss_sp);
        free(hilo_actual);
    }

    scheduler();
}

/*
Función: Espera a que un hilo específico termine su ejecución.

Entrada:
- hilo: Puntero al hilo que se desea esperar.
- retval: Doble puntero donde se almacenará el valor de retorno del hilo.

Salida: 0 si todo salió bien, -1 si hubo error.
*/

int my_pthread_join(my_pthread *hilo, void **retval) {

    if (hilo->estado != TERMINADO) {
        hilo->thread_esperando = hilo_actual;
        hilo_actual->estado = BLOQUEADO;
        scheduler();
    }

    if (retval) {
        *retval = hilo->retval;
    }

    if (!hilo->vinculado) {
        free(hilo->contexto.uc_stack.ss_sp);
        free(hilo);
    }

    return 0;
}

/*
Función: Marca un hilo como "desvinculado" para que sus recursos sean liberados automáticamente al terminar.

Entrada:
- hilo: Puntero al hilo que se desea marcar como desvinculado.

Salida: 0 si todo salió bien, -1 si hubo error.
*/

int my_pthread_detach(my_pthread *hilo) {

    if (hilo->estado == TERMINADO) {
        free(hilo->contexto.uc_stack.ss_sp);
        free(hilo);
    } else {
        hilo->vinculado = 1;
    }
    return 0;
}

/*
Función: Cambia el scheduler asociado a un hilo.

Entrada:
- hilo: Puntero al hilo cuyo scheduler se desea cambiar.
- nuevo_scheduler: Nuevo tipo de scheduler a asignar.

Salida: Ninguna
*/

void my_pthread_chsched(my_pthread *hilo, tipo_scheduler nuevo_scheduler){
  
  if(hilo == hilo_actual){
    
    printf("Papi no se puede cambiar un scheduler del hilo en ejecución");
  
  }
  
  tipo_scheduler tipo = hilo->scheduler;
  
  sacar(hilo, tipo);
  
  hilo->scheduler = nuevo_scheduler;
  
  meter(nuevo_scheduler, hilo);
  
  printf("Cambio realizado correctamente\n");

  return;
  
}

/*
Función: Inicializa un mutex.

Entrada:
- mutex: Puntero al mutex que se desea inicializar.

Salida: 0 si todo salió bien, -1 si hubo error.
*/

int my_mutex_init(my_mutex *mutex) {

    if(!mutex) {
        printf("Error: mutex no valido\n");
        return -1; 
    }
    mutex->bloqueado = 0;
    mutex->duenio = NULL;
    cola_init(mutex->cola_esperando);
    return 0; 
}

/*
Función: Intenta adquirir un mutex, bloqueando si no está disponible.

Entrada:
- mutex: Puntero al mutex que se desea bloquear.

Salida: 0 si se adquirió el mutex, -1 si hubo error.
*/

int my_mutex_lock(my_mutex *mutex) {

    if(!mutex) {
        printf("Error: mutex no valido\n");
        return -1; 
    }
    if(mutex->bloqueado && mutex->duenio == hilo_actual) {
        return -1; 
    }
    while(__sync_lock_test_and_set(&mutex->bloqueado, 1)) { 
        encolar(mutex->cola_esperando, hilo_actual); 
        hilo_actual->estado = BLOQUEADO; 
        scheduler(); 
    }
    mutex->duenio = hilo_actual; 
    return 0; 
}

/*
Función: Libera un mutex previamente adquirido.

Entrada:
- mutex: Puntero al mutex que se desea liberar.

Salida: 0 si todo salió bien, -1 si hubo error.
*/

int my_mutex_unlock(my_mutex *mutex) {

    if(!mutex || mutex->duenio != hilo_actual) {
        printf("Error: mutex no valido o el hilo actual no es el dueño\n");
        return -1; 
    }

    mutex->duenio = NULL; 
    __sync_lock_release(&mutex->bloqueado); 
    my_pthread *hilo_esperando = desencolar(mutex->cola_esperando); 

    if(hilo_esperando){
        hilo_esperando->estado = LISTO; 
        encolar(&cola, hilo_esperando);
    }

    return 0; 
}

/*
Función: Destruye un mutex y libera sus recursos.

Entrada:
- mutex: Puntero al mutex que se desea destruir.

Salida: 0 si todo salió bien, -1 si hubo error.
*/

int my_mutex_destroy(my_mutex *mutex) {

    if(!mutex) {
        printf("Error: mutex no valido\n");
        return -1;
    }

    if(mutex->bloqueado) {
        printf("Error: mutex bloqueado\n");
        return -1; 
    }

    if(mutex->cola_esperando->size > 0) {
        printf("Error: hay hilos esperando en la cola\n");
        return -1; 
    }

    free(mutex->cola_esperando); 
    return 0; 
}

/*
Función: Intenta adquirir un mutex sin bloquear.

Entrada:
- mutex: Puntero al mutex que se desea bloquear.

Salida: 0 si se adquirió el mutex, -1 si no estaba disponible o hubo error.
*/

int my_mutex_trylock(my_mutex *mutex) { 

    if(!mutex) {
        printf("Error: mutex no valido\n");
        return -1; 
    }

    if(mutex->bloqueado) {
        return -1; 
    }

    __sync_lock_test_and_set(&mutex->bloqueado, 1);
    mutex->duenio = hilo_actual; 
    return 0; 
}


