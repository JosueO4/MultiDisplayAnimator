#include "myPthreads.h"
#include "scheduler.h"
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_ROWS 100
#define MAX_COLS 100
#define MAX_FIGURAS 10
#define PORT 12345
#define FIG_SIZE 5


typedef struct {
    int fd;
    int offset;
} Client;

int MAX_CLIENTS;
Client *clients;
int num_clients = 0;

typedef struct {
    char figura[MAX_ROWS][MAX_COLS];
    int fig_rows, fig_cols;
    int inicio_ms, fin_ms;
    int pos_x_inicio;
    int pos_x_final;
    int angulo;
    int periodo_angulo;
    int pos_y;
    int pos_global_x;
    int terminado;
    int explotado;
} figura_anim;

typedef struct {
    int idx;
    my_mutex *mutex;
} hilo_args;

extern my_pthread *hilo_actual;
extern char figura[MAX_ROWS][MAX_COLS];
extern cola_hilos cola;
extern cola_hilos cola_RR;
extern cola_hilos cola_lottery;
extern cola_hilos lista_real_time;
int fig_rows, fig_cols;
int ventana_rows, ventana_cols;
extern ucontext_t contexto_principal;
int monitores;
my_mutex mutex_pantalla;
int num_figuras = 0;
figura_anim figuras[MAX_FIGURAS];

// Si no se especifica es siempre RR

tipo_scheduler tipo = RR;
int CLIENT_WIDTH; 
int TOTAL_WIDTH = 0; 

/*
    Función: Lee y parsea un archivo YAML que contiene la configuración de animaciones,
    incluyendo parámetros de scheduler, dimensiones de ventana, rotaciones, posiciones 
    y figuras ASCII.
    
    Entrada:
        - filename: Ruta del archivo YAML a leer
    
    Salida: Ninguna pero básicamente configura todo.
*/

void leer_figuras_yaml(const char *filename) {

    FILE *file = fopen(filename, "r");
    if (!file) { perror("No se pudo abrir el archivo YAML"); exit(1); }
    char line[256];
    int leyendo_figura = 0, fig_idx = -1, row = 0;

    while (fgets(line, sizeof(line), file)) {

        if(strstr(line,"scheduler:")){
            
            char* start = strchr(line, '"');

            if (!start) start = strchr(line, '\'');

            if (start) {

                start++;
                char* end = strchr(start, '"');

                if (!end) end = strchr(start, '\'');

                if (end) {

                    *end = '\0';
                    
                    if(strcmp(start, "RR") == 0){
                        tipo = RR;
                    }
                    else if(strcmp(start, "SORTEO") == 0){
                        tipo = SORTEO;
                    }
                    else if(strcmp(start, "TIEMPOREAL") == 0){
                        tipo = TIEMPOREAL;
                    }
                }
            }
        }

        if(strstr(line, "monitores:")) {
            monitores = atoi(strchr(line, ':') + 1);
            MAX_CLIENTS = monitores;
        }
        else if (strstr(line, "height:")) {
            ventana_rows = atoi(strchr(line, ':') + 1);
        } else if (strstr(line, "width:")) {
            ventana_cols = atoi(strchr(line, ':') + 1);
            CLIENT_WIDTH = ventana_cols;
            TOTAL_WIDTH = MAX_CLIENTS * CLIENT_WIDTH;
        }else if (strstr(line, "- inicio:")) {
            fig_idx++;
            num_figuras++;
            row = 0;
            figuras[fig_idx].inicio_ms = atoi(strchr(line, ':') + 1);
        } else if (strstr(line, "fin:")) {
            figuras[fig_idx].fin_ms = atoi(strchr(line, ':') + 1);
        } else if(strstr(line, "pos_x_inicio:")){
            figuras[fig_idx].pos_x_inicio = atoi(strchr(line, ':') + 1);
            figuras[fig_idx].pos_global_x = figuras[fig_idx].pos_x_inicio;
        } else if(strstr(line, "pos_x_final:")){
            figuras[fig_idx].pos_x_final = atoi(strchr(line, ':') + 1);
        } else if(strstr(line, "angulo:")){
            figuras[fig_idx].angulo = atoi(strchr(line, ':') + 1);
        } else if(strstr(line, "periodo:")){
            figuras[fig_idx].periodo_angulo = atoi(strchr(line, ':') + 1);
        }
        else if (strstr(line, "pos_y:")) {
            figuras[fig_idx].pos_y = atoi(strchr(line, ':') + 1);
        } else if (strstr(line, "figura:")) {
            leyendo_figura = 1;
            continue;
        } else if (leyendo_figura) {
            if (line[0] == '\n' || strlen(line) < 2) continue;
            char *ptr = strchr(line, '|');
            if (!ptr) continue;
            ptr++;
            int col = 0;
            while (*ptr && *ptr != '\n' && col < MAX_COLS) {
                if (*ptr == '|') { ptr++; continue; }
                figuras[fig_idx].figura[row][col++] = *ptr++;
            }
            figuras[fig_idx].fig_cols = col;
            row++;
            figuras[fig_idx].fig_rows = row;
        }
        
        if (leyendo_figura && (line[0] == '\n' || feof(file))) {
            leyendo_figura = 0;
        }
    }
    fclose(file);
}

/*
    Funcion: Se encarga de agarrar una figura ascii y la rota 90,180 o 270 grados. Al ser una matriz se hace una operacion
    sencilla para el cambio de posiciones
    Entrada: Puntero a la figura, grados a girar
    Salida: Ninguna pero cambia las posiciones según la rotación
*/

void rotar(figura_anim *f, int grados) {

    char temp[MAX_ROWS][MAX_COLS];
    int r, c;

    if (grados == 90) {

        for (r = 0; r < f->fig_rows; r++)

            for (c = 0; c < f->fig_cols; c++)

                temp[c][f->fig_rows - 1 - r] = f->figura[r][c];

        int tmp = f->fig_rows;

        f->fig_rows = f->fig_cols;

        f->fig_cols = tmp;

    } else if (grados == 180) {

        for (r = 0; r < f->fig_rows; r++)

            for (c = 0; c < f->fig_cols; c++)

                temp[f->fig_rows - 1 - r][f->fig_cols - 1 - c] = f->figura[r][c];

    } else if (grados == 270) {

        for (r = 0; r < f->fig_rows; r++)

            for (c = 0; c < f->fig_cols; c++)

                temp[f->fig_cols - 1 - c][r] = f->figura[r][c];

        int tmp = f->fig_rows;

        f->fig_rows = f->fig_cols;

        f->fig_cols = tmp;
    } else {
        return;
    }

    for (r = 0; r < f->fig_rows; r++)
        for (c = 0; c < f->fig_cols; c++)
            f->figura[r][c] = temp[r][c];
}

/*
    Función: Controla la animación de una figura ASCII a través de sockets, incluyendo movimiento,
    rotación y visualización en múltiples clientes. Maneja el ciclo de vida completo
    de la animación desde inicio hasta finalización.
    
    Entrada: 
        - arg: Puntero a estructura hilo_args que contiene:
            * idx: Índice de la figura a animar
            * mutex: Mutex para sincronización entre hilos
    
    Salida: Ninguna (función void), pero realiza las siguientes operaciones:
        - Mueve la figura horizontalmente entre pos_x_inicio y pos_x_final
        - Rota la figura según el periodo_angulo especificado
        - Muestra la animación en todos los clientes conectados
        - Al finalizar, muestra "KABOOM" o "TERMINADO A TIEMPO" en la posición final
        - Libera recursos y termina el hilo correctamente
*/

void hilo_animacion_socket(void *arg) {
    hilo_args *args = (hilo_args*)arg;
    int idx = args->idx;
    figura_anim *f = &figuras[idx];
    int elapsed = 0;
    int duracion = f->fin_ms - f->inicio_ms;
    int angulo = f->angulo;
    f->explotado = 0;
    f->terminado = 0;
    usleep(f->inicio_ms * 1000);

    // (1: derecha, -1: izquierda)
    int direccion = (f->pos_x_final > f->pos_x_inicio) ? 1 : -1;

    // Mientras no haya terminado el tiempo o llegado a la pos final
    while (elapsed < duracion && 
          ((direccion == 1 && f->pos_x_inicio < f->pos_x_final) || 
           (direccion == -1 && f->pos_x_inicio > f->pos_x_final))) {
        
        // Rotaciones
        
        if (elapsed % f->periodo_angulo == 0) {
            my_mutex_lock(args->mutex);
            rotar(f, angulo);
            my_mutex_unlock(args->mutex);
        }
        
        my_mutex_lock(args->mutex);
        

        
        for (int i = 0; i < num_clients; i++) {

            char frame[2048] = "\033[H";
            char empty_line[f->fig_cols + 2];  
            memset(empty_line, ' ', f->fig_cols);
            empty_line[f->fig_cols] = '\n';
            empty_line[f->fig_cols + 1] = '\0';

            // Se rellena con filas vacias para llegar hasta la pos_y de la figura

            for (int empty_row = 0; empty_row < f->pos_y; empty_row++) {
                strcat(frame, empty_line);  
            }

            
            // Se dibuja la figura

            for (int row = 0; row < f->fig_rows; row++) {
                for (int col = 0; col < CLIENT_WIDTH; col++) {
                    int global_col = clients[i].offset + col;
                    int fig_col = global_col - f->pos_global_x;
            
                    if (fig_col >= 0 && fig_col < f->fig_cols) {
                        frame[strlen(frame)] = f->figura[row][fig_col];
                    } else {
                        frame[strlen(frame)] = ' ';
                    }
                }
                frame[strlen(frame)] = '\n';
                frame[strlen(frame)] = '\0';
            }

            // se manda la trama al cliente

            send(clients[i].fd, frame, strlen(frame), 0);
        }

        my_mutex_unlock(args->mutex);
        
        // Se actualizan las posiciones y se reinician si se llega al borde
        f->pos_x_inicio += direccion;
        f->pos_global_x += direccion;
        if (f->pos_global_x > TOTAL_WIDTH) f->pos_global_x = -f->fig_cols;
        if (f->pos_global_x < -f->fig_cols) f->pos_global_x = TOTAL_WIDTH;
        
        usleep(100000);
        elapsed += 100;
        my_pthread_yield();
    }

    // Si se acabó la animación 

    my_mutex_lock(args->mutex);
    
    for (int i = 0; i < num_clients; i++) {
        
        char frame[2048] = "\033[H";
        char empty_line[f->fig_cols + 2];
        memset(empty_line, ' ', f->fig_cols);
        empty_line[f->fig_cols] = '\n';
        empty_line[f->fig_cols + 1] = '\0';

        // Se rellena con filas vacias para llegar hasta la pos_y de la figura

        for (int empty_row = 0; empty_row < f->pos_y; empty_row++) {
            strcat(frame, empty_line);  
        }


        char cursor_pos[32];
        snprintf(cursor_pos, sizeof(cursor_pos), "\033[%d;%dH", 
                f->pos_y + 1,
                (f->pos_global_x - clients[i].offset) + 1);  
        
        strcat(frame, cursor_pos);
        
        // Se coloca en pantalla el mensaje correspondiente

        char mensaje[64];
        if (elapsed >= duracion) {
            f->explotado = 1;
            snprintf(mensaje, sizeof(mensaje), "KABOOM");
        } else {
            f->terminado = 1;
            snprintf(mensaje, sizeof(mensaje), "TERMINADO A TIEMPO");
        }
        
        strcat(frame, mensaje);
        
        send(clients[i].fd, frame, strlen(frame), 0);
    }

    my_mutex_unlock(args->mutex);
    usleep(2000000);
    my_pthread_end(NULL);
}


int main(int argc, char *argv[]) {
    int opt;
    char configFile[256]; // Archivo de configuración por defecto
    while ((opt = getopt(argc, argv, "c:")) != -1) {
        switch (opt) {
            case 'c':
                strncpy(configFile, argv[2], sizeof(configFile) - 1);
                configFile[sizeof(configFile) - 1] = '\0';
                break;
            default:
                fprintf(stderr, "Uso: %s -c [archivo_configuracion]\n", argv[2]);
                exit(EXIT_FAILURE);
                 
        }
    }
    leer_figuras_yaml(configFile);
    clients = malloc(monitores * sizeof(Client));
    if (!clients) {
        perror("Error al asignar memoria para los clientes");
        exit(1);
    }
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Crear socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(1);
    }

    // Configurar socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(1);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(1);
    }

    printf("Esperando %d clientes...\n", MAX_CLIENTS);
    while (num_clients < MAX_CLIENTS) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(1);
        }

        clients[num_clients].fd = new_socket;
        clients[num_clients].offset = num_clients * CLIENT_WIDTH;
        num_clients++;
        printf("Cliente conectado (%d/%d)\n", num_clients, MAX_CLIENTS);
    }

    
    cola_init(&cola);
    cola_init(&cola_RR);
    cola_init(&cola_lottery);
    cola_init(&lista_real_time);

    my_pthread hiloPrincipal;
    hiloPrincipal.tid = 0;
    hiloPrincipal.estado = CORRIENDO;
    hiloPrincipal.scheduler = tipo;
    hilo_actual = &hiloPrincipal;

    initscr();
    noecho();
    curs_set(FALSE);
    nodelay(stdscr, TRUE);
    

    my_mutex mutex_pantalla;
    mutex_pantalla.cola_esperando = &cola;
    my_mutex_init(&mutex_pantalla);

    my_pthread *hilos[MAX_FIGURAS];
    hilo_args args[MAX_FIGURAS];

    for (int i = 0; i < num_figuras; i++) {
        args[i].idx = i;
        args[i].mutex = &mutex_pantalla;
        my_pthread_create(&hilos[i], tipo, hilo_animacion_socket, &args[i], figuras[i].inicio_ms);
    }

    int tickets_iniciales = 100/num_figuras;

    for (int i = 0; i < num_figuras; i++) {
        hilos[i]->tickets = tickets_iniciales;
    }
    
    getcontext(&contexto_principal);
    iniciar_timer_scheduler();
    
    scheduler();

    endwin();
    free(clients);
    return 0;
}