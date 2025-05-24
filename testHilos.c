#include "myPthreads.h"
#include <sys/time.h>
extern cola_hilos cola;
extern cola_hilos cola_RR;
extern cola_hilos cola_lottery;
extern cola_hilos lista_real_time;
extern my_pthread *hilo_actual;




void hello(void *arg) {
    char *str = (char *)arg;
    for(int i = 0; i < 5; i++) {
        printf("Hilo [%s] iteracion %d\n", str, i);
        usleep((rand() % 500 + 100) * 1000);
        if (i < 4) {  // Solo hacer yield si no es la última iteración
            my_pthread_yield();
        }
    }
    printf("Hilo [%s] terminando...\n", str);
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
    hiloPrincipal.scheduler = TIEMPOREAL;
    hilo_actual = &hiloPrincipal;

my_pthread *h1, *h2, *h3, *h4, *h5, *h6, *h7, *h8, *h9, *h10;


// Se agrega deadline como parametro al crear porque al meterse en cola debe ser ordenado con base a deadline

my_pthread_create(&h1, tipo, hello, "Hilo 1",5);
my_pthread_create(&h2, tipo, hello, "Hilo 2",1);
my_pthread_create(&h3, tipo, hello, "Hilo 3",6);
my_pthread_create(&h4, tipo, hello, "Hilo 4",7);
my_pthread_create(&h5, tipo, hello, "Hilo 5",3);
my_pthread_create(&h6, tipo, hello, "Hilo 6",2);
my_pthread_create(&h7, tipo, hello, "Hilo 7",4);
my_pthread_create(&h8, tipo, hello, "Hilo 8",8);
my_pthread_create(&h9, tipo, hello, "Hilo 9",9);
my_pthread_create(&h10, tipo, hello, "Hilo 10",10);

h1->tickets = 10;
h2->tickets = 15;
h3->tickets = 5;
h4->tickets = 70;
h5->tickets = 20;
h6->tickets = 25;
h7->tickets = 30;
h8->tickets = 5;
h9->tickets = 50;
h10->tickets = 40;

    iniciar_timer_scheduler();
    scheduler();

    return 0;
}
