#include "myPthreads.h"
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_ROWS 100
#define MAX_COLS 100
#define MAX_FIGURAS 10
#define MAX_ROWS 100
#define MAX_COLS 100

typedef struct {
    char figura[MAX_ROWS][MAX_COLS];
    int fig_rows, fig_cols;
    int inicio_ms, fin_ms;
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
void leer_figura(const char *filename);
void dibujar_figura(int start_y, int start_x);
void rotar_90();
void rotar_180();
void rotar_270();

my_mutex mutex_pantalla;
volatile int fin_animacion = 0;
//int ventana_rows = 24, ventana_cols = 80;
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
        } else if (strstr(line, "pos_y:")) {
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

void hilo_animacion(void *arg) {
    hilo_args *args = (hilo_args*)arg;
    int idx = args->idx;
    figura_anim *f = &figuras[idx];
    int max_y, max_x;
    int pos_x = 0;
    getmaxyx(stdscr, max_y, max_x);

    usleep(f->inicio_ms * 1000);
    int duracion = f->fin_ms - f->inicio_ms;
    int elapsed = 0;

    while (elapsed < duracion) {
        my_mutex_lock(args->mutex);
        clear();
        // Dibuja todas las figuras activas
        for (int i = 0; i < num_figuras; i++) {
            if (i == idx && elapsed < duracion) {
                for (int r = 0; r < figuras[i].fig_rows; r++)
                    for (int c = 0; c < figuras[i].fig_cols; c++)
                        if (figuras[i].figura[r][c] != ' ')
                            mvaddch(figuras[i].pos_y + r, pos_x + c, figuras[i].figura[r][c]);
            }
        }
        refresh();
        my_mutex_unlock(args->mutex);

        pos_x++;
        if (pos_x + f->fig_cols >= max_x) pos_x = 0;
        usleep(100000);
        elapsed += 100;
    }

    // Kaboom para esta figura
    my_mutex_lock(args->mutex);
    clear();
    mvprintw(f->pos_y + f->fig_rows/2, (max_x-6)/2, "kaboom");
    refresh();
    my_mutex_unlock(args->mutex);
    usleep(2000000);

    my_pthread_end(NULL);
}

void hilo_tiempo(void *arg) {
    int duracion = tiempo_fin_ms - tiempo_inicio_ms;
    usleep(duracion * 1000);
    fin_animacion = 1;

    my_mutex_lock(&mutex_pantalla);
    clear();
    mvprintw(ventana_rows/2, (ventana_cols-6)/2, "kaboom");
    refresh();
    my_mutex_unlock(&mutex_pantalla);

    usleep(1000000000); // Mostrar "kaboom" 2 segundos
    my_mutex_lock(&mutex_pantalla);
    endwin();
    my_mutex_unlock(&mutex_pantalla);
    exit(0);
}

int main() {
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

    for (int i = 0; i < num_figuras; i++) {
        args[i].idx = i;
        args[i].mutex = &mutex_pantalla;
        my_pthread_create(&hilos[i], tipo, hilo_animacion, &args[i], 7);
    }

    getcontext(&contexto_principal);
    iniciar_timer_scheduler();
    scheduler();

    endwin();
    return 0;
}