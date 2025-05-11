#include "myPthreads.h"

void hello(void *arg) {
    char *str = (char *)arg;
    for(int i = 0; i < 5; i++) {
        printf("Hilo [%s] iteracion %d\n", str, i);
        my_pthread_yield();
    }
    my_pthread_end(NULL);
}

int main() {
    extern cola_hilos cola;
    extern my_pthread *hilo_actual;

    cola_init(&cola);

    my_pthread hiloPrincipal;
    hiloPrincipal.tid = 0;
    hiloPrincipal.estado = CORRIENDO;
    hiloPrincipal.retval = NULL;
    hiloPrincipal.thread_esperando = NULL;
    hiloPrincipal.vinculado = 0;

    hilo_actual = &hiloPrincipal;

    // Ahora que hilo_actual ya está definido, se pueden crear los hilos
    my_pthread_create(NULL, hello, "Hilo 1");
    my_pthread_create(NULL, hello, "Hilo 2");

    scheduler(); // Iniciar el scheduler

    return 0;
}
