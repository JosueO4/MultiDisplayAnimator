
#include "myPthreads.h"

// metodos

void scheduler();
void iniciar_timer_scheduler();
void meter(tipo_scheduler tipo, my_pthread* nuevo_hilo);
void sacar(my_pthread* hilo, tipo_scheduler tipo);
