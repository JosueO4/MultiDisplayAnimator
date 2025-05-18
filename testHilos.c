#include "myPthreads.h"

extern cola_hilos cola;
extern cola_hilos cola_RR;
extern cola_hilos cola_lottery;
extern cola_hilos lista_real_time;
extern my_pthread *hilo_actual;




void hello(void *arg) {
    char *str = (char *)arg;
    for(int i = 0; i < 5; i++) {
        printf("Hilo [%s] iteracion %d\n", str, i);
        usleep((rand() % 500 + 100) * 1000); // duerme entre 100ms y 600ms
        my_pthread_yield();
    }
    my_pthread_end(NULL);
}

void hello_fast(void *arg){
    char *str = (char *)arg;
    for(int i = 0; i < 5; i++) {
        printf("Hilo [%s] iteracion %d\n", str, i);
        my_pthread_yield();
    }
    my_pthread_end(NULL);
}

int main() {
    
    // AQUI SE PUEDE CAMBIAR EL SCHEDULER DE TODOS (UTIL PARA HACER PRUEBAS XD)
    
    tipo_scheduler tipo = TIEMPOREAL;
    
    
    cola_init(&cola);
    cola_init(&cola_RR);
    cola_init(&cola_lottery);
    cola_init(&lista_real_time);

    my_pthread hiloPrincipal;
    hiloPrincipal.tid = 0;
    hiloPrincipal.estado = CORRIENDO;
    hiloPrincipal.scheduler = SORTEO;
    hilo_actual = &hiloPrincipal;

    my_pthread *h1, *h2, *h3, *h4;
    my_pthread_create(&h1, tipo,hello_fast, "Hilo 1");
    my_pthread_create(&h2, tipo,hello_fast, "Hilo 2");
    my_pthread_create(&h3, tipo,hello_fast, "Hilo 3");
    my_pthread_create(&h4, tipo,hello_fast, "Hilo 4");
    
    
    my_pthread_chsched(h1,SORTEO);
    my_pthread_chsched(h2,SORTEO);
    my_pthread_chsched(h3,SORTEO);
    my_pthread_chsched(h4,SORTEO);

    // Configurar atributos según el tipo de scheduler
    h1->tickets = 10;
    h2->tickets = 15;
    h3->tickets = 5;
    h4->tickets = 70;

    h1->deadlineSeconds = 1;
    h2->deadlineSeconds = 3;
    h3->deadlineSeconds = 15;
    h4->deadlineSeconds = 2;

    h1->quantum = 2;
    h2->quantum = 2;
    h3->quantum = 2;
    h4->quantum = 2;

    scheduler();

    return 0;
}
