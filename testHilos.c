#include "myPthreads.h"
#include <sys/time.h>
extern cola_hilos cola;
extern cola_hilos cola_RR;
extern cola_hilos cola_lottery;
extern cola_hilos lista_real_time;
extern my_pthread *hilo_actual;
int contador_global = 0;
my_mutex mutex_global;


void contador_seguro(void *arg) {
    char *nombre = (char *)arg;
    for(int i = 0; i < 1000; i++) {
        my_mutex_lock(&mutex_global);    // Entrar a la sección crítica

        int valor = contador_global;
        valor++;
        contador_global = valor;

        my_mutex_unlock(&mutex_global);  // Salir de la sección crítica
        
        printf("VAlor: %d\n",contador_global);
        // Sin yield aquí para probar contención
    }

    printf("Hilo [%s] terminado\n", nombre);
    my_pthread_end(NULL);
}

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
    tipo_scheduler tipo = RR;  // Podés probar también con Lottery o Tiempo Real

    cola_init(&cola);
    cola_init(&cola_RR);
    cola_init(&cola_lottery);
    cola_init(&lista_real_time);

    my_pthread hiloPrincipal;
    hiloPrincipal.tid = 0;
    hiloPrincipal.estado = CORRIENDO;
    hiloPrincipal.scheduler = tipo;
    hilo_actual = &hiloPrincipal;
    
    printf("Hilo principal creado\n");
    mutex_global.cola_esperando = &cola;
    my_mutex_init(&mutex_global); // Inicializar el mutex
    
    printf("HICE el mutex BIEN\n");
    
    my_pthread *h1, *h2, *h3, *h4;

    my_pthread_create(&h1, tipo, contador_seguro, "A", 0);
    my_pthread_create(&h2, tipo, contador_seguro, "B", 0);
    my_pthread_create(&h3, tipo, contador_seguro, "C", 0);
    my_pthread_create(&h4, tipo, contador_seguro, "D", 0);
    
    printf("HICE LOS HILOS BIEN\n");
    
    iniciar_timer_scheduler();
    scheduler();

    printf("Valor final del contador: %d\n", contador_global);

    return 0;
}
