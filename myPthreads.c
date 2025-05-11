#include "myPthreads.h"

cola_hilos cola;
my_pthread *hilo_actual = NULL;
int tid_contador = 1;

void cola_init(cola_hilos *cola) {
    cola->head = NULL;
    cola->tail = NULL;
    cola->size = 0;
}

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

int my_pthread_create(my_pthread **hilo, void (*start_routine)(void *), void *arg) {
    my_pthread *nuevo_hilo = (my_pthread *)malloc(sizeof(my_pthread));
    if (nuevo_hilo == NULL) {
        return -1; // Error al asignar memoria
    }
    getcontext(&nuevo_hilo->contexto);
    nuevo_hilo->contexto.uc_stack.ss_sp = malloc(STACK_SIZE); // Asignar memoria para la pila
    nuevo_hilo->contexto.uc_stack.ss_size = STACK_SIZE; // Tamaño de la pila
    nuevo_hilo->contexto.uc_link = NULL; // No hay contexto de retorno

    nuevo_hilo->tid = tid_contador++;
    nuevo_hilo->estado = LISTO;
    nuevo_hilo->retval = NULL;
    nuevo_hilo->thread_esperando = NULL;
    nuevo_hilo->vinculado = 0;
    makecontext(&nuevo_hilo->contexto, (void (*)(void))start_routine, 1, arg); // Crear el contexto para la función del hilo
    encolar(&cola, nuevo_hilo); // Encolar el nuevo hilo
    if (hilo) {
        *hilo = nuevo_hilo; // Devolver el hilo creado
    }
    return 0; // Éxito
}

void my_pthread_yield() {
    if (hilo_actual->estado != TERMINADO) {
        hilo_actual->estado = LISTO; // Cambiar el estado del hilo actual a LISTO
        encolar(&cola, hilo_actual); // Reencolar el hilo actual
    }
    scheduler(); // Llamar al scheduler para cambiar de contexto
}

void my_pthread_end(void *retval) {
    hilo_actual->estado = TERMINADO; // Cambiar el estado del hilo actual a TERMINADO
    hilo_actual->retval = retval; // Guardar el valor de retorno

    if(hilo_actual->thread_esperando != NULL) {
        encolar(&cola, hilo_actual->thread_esperando); // Reencolar el hilo que estaba esperando
    }

    if(hilo_actual->vinculado) {
        free(hilo_actual->contexto.uc_stack.ss_sp); // Liberar la pila del hilo
        free(hilo_actual); // Liberar el hilo
    }
    scheduler(); // Llamar al scheduler para cambiar de contexto
}

int my_pthread_join(my_pthread *hilo, void **retval) {
    if (hilo->estado == TERMINADO) {
        hilo->thread_esperando = hilo_actual; // Establecer el hilo actual como el que está esperando
        hilo_actual->estado = BLOQUEADO; // Cambiar el estado del hilo actual a CORRIENDO
        scheduler(); // Llamar al scheduler para cambiar de contexto
    }

    if (retval) {
        *retval = hilo->retval; // Devolver el valor de retorno del hilo
    }
    if(!hilo->vinculado) {
        free(hilo->contexto.uc_stack.ss_sp); // Liberar la pila del hilo
        free(hilo); // Liberar el hilo
    }
    return 0; // Éxito
}

int my_pthread_detach(my_pthread *hilo) {
    if (hilo->estado == TERMINADO) {
        free(hilo->contexto.uc_stack.ss_sp); // Liberar la pila del hilo
        free(hilo); // Liberar el hilo
    } else {
        hilo->vinculado = 1; // Marcar el hilo como vinculado
    }
    return 0; // Éxito
}

void scheduler() {
    my_pthread *prev = hilo_actual;
    hilo_actual = desencolar(&cola); // Desencolar el siguiente hilo
    if (hilo_actual == NULL) {

        exit(0); // No hay más hilos, salir
    }

    hilo_actual->estado = CORRIENDO; // Cambiar el estado del nuevo hilo a CORRIENDO

    if(prev && prev->estado != TERMINADO) {
        swapcontext(&prev->contexto, &hilo_actual->contexto); // Cambiar de contexto
    }
    else {
        setcontext(&hilo_actual->contexto); // Establecer el contexto del nuevo hilo
    }
}