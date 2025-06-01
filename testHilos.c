#include "myPthreads.h"
#include "scheduler.h"
#include <sys/time.h>
extern cola_hilos cola;
extern cola_hilos cola_RR;
extern cola_hilos cola_lottery;
extern cola_hilos lista_real_time;
extern my_pthread *hilo_actual;
int contador_global = 0;
my_mutex mutex_global;
extern ucontext_t contexto_principal;

/* Función: Ejemplo para mostrar escritura en un valor de forma ordenada y segura
*/

void contador_seguro(void *arg) {
    char *nombre = (char *)arg;
    for(int i = 0; i < 10; i++) {
        my_mutex_lock(&mutex_global);

        int valor = contador_global;
        valor++;
        contador_global = valor;

        my_mutex_unlock(&mutex_global);
        printf("Valor: %d\n",contador_global);
        
    }

    printf("Hilo [%s] terminado\n", nombre);
    my_pthread_end(NULL);
}


int main() {

    printf("Programa para mostrar funcionamiento de change schduler: (y de paso mutex)\n");

    tipo_scheduler tipo = RR;

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
    my_mutex_init(&mutex_global); 
    
    my_pthread *h1, *h2, *h3, *h4;

    my_pthread_create(&h1, tipo, contador_seguro, "A", 1000);
    my_pthread_create(&h2, tipo, contador_seguro, "B", 3000);
    my_pthread_create(&h3, tipo, contador_seguro, "C", 5000);
    my_pthread_create(&h4, tipo, contador_seguro, "D", 2000);
    
    printf("Se han creado 4 hilos de RR\n");

    imprimir_cola_rr(&cola_RR);

    printf("Ahora cambiamos de RR a TIEMPOREAL\n");

    hiloPrincipal.scheduler = TIEMPOREAL;
    my_pthread_chsched(h1,TIEMPOREAL);
    my_pthread_chsched(h2,TIEMPOREAL);
    my_pthread_chsched(h3,TIEMPOREAL);
    my_pthread_chsched(h4,TIEMPOREAL);

    printf("Cola de RR:\n");
    imprimir_cola_rr(&cola_RR);
    printf("Cola de TIEMPOREAL:\n");
    imprimir_lista_real_time(&lista_real_time);

    printf("Se ejecuta para probar que todo funciona:\n");

    getcontext(&contexto_principal);
    iniciar_timer_scheduler();
    scheduler();

    printf("Valor final del contador: %d\n", contador_global);  
    return 0;
}
