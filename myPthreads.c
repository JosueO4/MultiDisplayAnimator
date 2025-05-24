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
        //printf("NO HABÍA NADA\n");
        return NULL;
    }

    nodo *nodo_a_eliminar = cola->head;
    my_pthread *hilo = nodo_a_eliminar->hilo;

    cola->head = nodo_a_eliminar->siguiente;
    if (cola->head == NULL) {
        //printf("NO HABÍA NADA2\n");
        cola->tail = NULL;
    }
    free(nodo_a_eliminar);
    cola->size--;
    return hilo;
}


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
    
    printf("METIENDO AL CREAR\n");
    
    meter(tipo,nuevo_hilo);

    if (hilo) *hilo = nuevo_hilo;

    return 0;
}

void my_pthread_yield() {
    if (!hilo_actual) return;
    
    if (hilo_actual->estado != TERMINADO) {
        hilo_actual->estado = LISTO;
        // Para SORTEO no se vuelve a encolar, ya está en la lista
        if (hilo_actual->scheduler == RR) {
            meter(hilo_actual->scheduler, hilo_actual);
        }
    }
    scheduler();
}
void my_pthread_end(void *retval) {

    //printf("END\n");

    hilo_actual->estado = TERMINADO;
    hilo_actual->retval = retval;
    tipo_scheduler tipo = hilo_actual->scheduler;
    sacar(hilo_actual,tipo);
    
    if (hilo_actual->thread_esperando != NULL) {

        meter(tipo,hilo_actual->thread_esperando);
    }
    
    
    
    if (hilo_actual->vinculado) {
        free(hilo_actual->contexto.uc_stack.ss_sp);
        free(hilo_actual);
    }

    scheduler();
}

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

int my_pthread_detach(my_pthread *hilo) {
    if (hilo->estado == TERMINADO) {
        free(hilo->contexto.uc_stack.ss_sp);
        free(hilo);
    } else {
        hilo->vinculado = 1;
    }
    return 0;
}

void my_pthread_chsched(my_pthread *hilo, tipo_scheduler nuevo_scheduler){
  

  if(hilo == hilo_actual){
    
    printf("Papi no se puede cambiar un scheduler del hilo en ejecución");
  
  }
  
  tipo_scheduler tipo = hilo->scheduler;
  
  sacar(hilo, tipo);
  
  hilo->scheduler = nuevo_scheduler;
  
  meter(nuevo_scheduler, hilo);
  
  printf("Cambio realizado correctamente\n");
}

int my_mutex_init(my_mutex *mutex) {
    if(!mutex) {
        printf("Error: mutex no valido\n");
        return -1; // Error: mutex no válido
    }
    mutex->bloqueado = 0;
    mutex->duenio = NULL;
    cola_init(mutex->cola_esperando);
    return 0; // Éxito
}

int my_mutex_lock(my_mutex *mutex) {
    if(!mutex) {
        printf("Error: mutex no valido\n");
        return -1; // Error: mutex no válido
    }
    if(mutex->bloqueado && mutex->duenio == hilo_actual) {
        return -1; // Error: el hilo actual ya es el dueño del mutex
    }
    while(__sync_lock_test_and_set(&mutex->bloqueado, 1)) { // lock seguro sin usar asm, es una operación atómica, evita condiciones de carrera
        encolar(mutex->cola_esperando, hilo_actual); // Encolar el hilo actual
        hilo_actual->estado = BLOQUEADO; // Cambiar el estado del hilo actual a BLOQUEADO
        scheduler(); // Llamar al scheduler para cambiar de contexto
    }
    mutex->duenio = hilo_actual; // Establecer el hilo actual como dueño del mutex
    return 0; // Éxito
}

int my_mutex_unlock(my_mutex *mutex) {
    if(!mutex || mutex->duenio != hilo_actual) {
        printf("Error: mutex no valido o el hilo actual no es el dueño\n");
        return -1; // Error: mutex no válido o el hilo actual no es el dueño
    }

    mutex->duenio = NULL; // Liberar el dueño del mutex
    __sync_lock_release(&mutex->bloqueado); // Liberar el mutex de forma atomica

    my_pthread *hilo_esperando = desencolar(mutex->cola_esperando); // Desencolar el hilo que estaba esperando
    if(hilo_esperando){
        hilo_esperando->estado = LISTO; // Cambiar el estado del hilo esperando a LISTO
        encolar(&cola, hilo_esperando); // Reencolar el hilo esperando
    }
    return 0; // Éxito
}

int my_mutex_destroy(my_mutex *mutex) {
    if(!mutex) {
        printf("Error: mutex no valido\n");
        return -1; // Error: mutex no válido
    }
    if(mutex->bloqueado) {
        printf("Error: mutex bloqueado\n");
        return -1; // Error: mutex bloqueado
    }
    if(mutex->cola_esperando->size > 0) {
        printf("Error: hay hilos esperando en la cola\n");
        return -1; // Error: hay hilos esperando en la cola
    }
    free(mutex->cola_esperando); // Liberar la cola de esperando
    return 0; // Éxito
}

int my_mutex_trylock(my_mutex *mutex) { // intenta bloquear el mutex sin esperar
    if(!mutex) {
        printf("Error: mutex no valido\n");
        return -1; // Error: mutex no válido
    }
    if(mutex->bloqueado) {
        return -1; // Mutex ya bloqueado
    }
    __sync_lock_test_and_set(&mutex->bloqueado, 1); // Bloquear el mutex de forma atomica
    mutex->duenio = hilo_actual; // Establecer el hilo actual como dueño del mutex
    return 0; // Éxito
}


