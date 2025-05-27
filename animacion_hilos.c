#include "myPthreads.h"
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
#define MAX_CLIENTS 2
#define CLIENT_WIDTH 100
#define TOTAL_WIDTH (MAX_CLIENTS * CLIENT_WIDTH)


typedef struct {
    int fd;
    int offset; // Posición inicial (0, 40, 80...)
} Client;

Client clients[MAX_CLIENTS];
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
} figura_anim;


// Variables y funciones de curses2.c

extern my_pthread *hilo_actual;
extern char figura[MAX_ROWS][MAX_COLS];
extern cola_hilos cola;
extern cola_hilos cola_RR;
extern cola_hilos cola_lottery;
extern cola_hilos lista_real_time;
extern int fig_rows, fig_cols;
extern int ventana_rows, ventana_cols;
extern int tiempo_inicio_ms, tiempo_fin_ms;
extern ucontext_t contexto_principal;
int monitores = 2;
my_mutex mutex_pantalla;
volatile int fin_animacion = 0;
int num_figuras = 0;
figura_anim figuras[MAX_FIGURAS];


void leer_figuras_yaml(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) { perror("No se pudo abrir el archivo YAML"); exit(1); }
    char line[256];
    int leyendo_figura = 0, fig_idx = -1, row = 0;
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "rows:")) {
            ventana_rows = atoi(strchr(line, ':') + 1);
        } else if (strstr(line, "columns:")) {
            ventana_cols = atoi(strchr(line, ':') + 1);
        } else if (strstr(line, "- inicio:")) {
            fig_idx++;
            num_figuras++;
            row = 0;
            figuras[fig_idx].inicio_ms = atoi(strchr(line, ':') + 1);
        } else if (strstr(line, "fin:")) {
            figuras[fig_idx].fin_ms = atoi(strchr(line, ':') + 1);
        } else if(strstr(line, "pos_x_inicio:")){
            figuras[fig_idx].pos_x_inicio = atoi(strchr(line, ':') + 1);
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
        // Detecta fin de figura
        if (leyendo_figura && (line[0] == '\n' || feof(file))) {
            leyendo_figura = 0;
        }
    }
    fclose(file);
}
typedef struct {
    int idx;
    my_mutex *mutex;
} hilo_args;


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
        return; // Ángulo inválido
    }

    // Copiar de vuelta
    for (r = 0; r < f->fig_rows; r++)
        for (c = 0; c < f->fig_cols; c++)
            f->figura[r][c] = temp[r][c];
}


void hilo_animacion_socket(void *arg) {
    hilo_args *args = (hilo_args*)arg;
    int idx = args->idx;
    figura_anim *f = &figuras[idx];

    int elapsed = 0;
    int duracion = f->fin_ms - f->inicio_ms;
    int angulo = f->angulo;
    int prev_pos_x = -1;

    usleep(f->inicio_ms * 1000);

    while (elapsed < duracion && f->pos_x_inicio < f->pos_x_final) {

        if (elapsed % f->periodo_angulo == 0) {
            my_mutex_lock(args->mutex);
            rotar(f, angulo);
            my_mutex_unlock(args->mutex);
        }

        my_mutex_lock(args->mutex);

        // Por cada cliente construimos el frame
        for (int i = 0; i < num_clients; i++) {
            char frame[2048] = "\033[H\033[2J"; // limpiar pantalla

            for (int row = 0; row < f->fig_rows; row++) {
                for (int col = 0; col < CLIENT_WIDTH; col++) {
                    int global_col = clients[i].offset + col;
                    int fig_col = global_col - f->pos_x_inicio;
            
                    if (fig_col >= 0 && fig_col < f->fig_cols) {
                        frame[strlen(frame)] = f->figura[row][fig_col];
                    } else {
                        frame[strlen(frame)] = ' ';
                    }
                }
                frame[strlen(frame)] = '\n';
                frame[strlen(frame)] = '\0';
            }

            send(clients[i].fd, frame, strlen(frame), 0);
        }

        my_mutex_unlock(args->mutex);

        f->pos_x_inicio++;
        usleep(100000);
        elapsed += 100;

        my_pthread_yield();
    }

    // Finalización: mensaje kaboom o terminado
    my_mutex_lock(args->mutex);

    for (int i = 0; i < num_clients; i++) {
        char frame[2048] = "\033[H\033[2J"; // limpiar pantalla

        for (int row = 0; row < f->fig_rows; row++) {
            for (int col = 0; col < CLIENT_WIDTH; col++) {
                frame[strlen(frame)] = ' ';
            }
            strcat(frame, "\n");
        }

        char mensaje[64];
        if (elapsed >= duracion) {
            snprintf(mensaje, sizeof(mensaje), "kaboom");
        } else {
            snprintf(mensaje, sizeof(mensaje), "Terminado a tiempo");
        }

        strcat(frame, "\n");
        strcat(frame, mensaje);

        send(clients[i].fd, frame, strlen(frame), 0);
    }

    my_mutex_unlock(args->mutex);

    usleep(2000000);
    my_pthread_end(NULL);
}


int main() {


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

    tipo_scheduler tipo = TIEMPOREAL;
    cola_init(&cola);
    cola_init(&cola_RR);
    cola_init(&cola_lottery);
    cola_init(&lista_real_time);

    my_pthread hiloPrincipal;
    hiloPrincipal.tid = 0;
    hiloPrincipal.estado = CORRIENDO;
    hiloPrincipal.scheduler = tipo;
    hilo_actual = &hiloPrincipal;

    leer_figuras_yaml("config.yaml");

    initscr();
    noecho();
    curs_set(FALSE);
    nodelay(stdscr, TRUE);
    resizeterm(ventana_rows, ventana_cols);

    my_mutex mutex_pantalla;
    mutex_pantalla.cola_esperando = &cola;
    my_mutex_init(&mutex_pantalla);

    my_pthread *hilos[MAX_FIGURAS];
    hilo_args args[MAX_FIGURAS];
    //printf("Numero: %d", num_figuras);
    for (int i = 0; i < num_figuras; i++) {
        args[i].idx = i;
        args[i].mutex = &mutex_pantalla;
        my_pthread_create(&hilos[i], tipo, hilo_animacion_socket, &args[i], figuras[i].inicio_ms);
    }

    getcontext(&contexto_principal);
    iniciar_timer_scheduler();

    
    
    scheduler();

    endwin();
    return 0;
}