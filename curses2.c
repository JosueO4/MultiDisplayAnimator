#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_ROWS 100
#define MAX_COLS 100

char figura[MAX_ROWS][MAX_COLS];
int fig_rows = 0;
int fig_cols = 0;
int ventana_rows = 24;   // Por defecto
int ventana_cols = 80;   // Por defecto
int leyendo_figura = 0;
int leyendo_tiempo = 0;
int tiempo_inicio_ms = 0;
int tiempo_fin_ms = 10000;
void leer_figura(const char *filename) {


    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("No se pudo abrir el archivo YAML");
        exit(1);
    }

    char line[256];
    int leyendo_figura = 0;
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "ventana:", 8) == 0) {
            continue;
        } else if (strstr(line, "rows:")) {
            ventana_rows = atoi(strchr(line, ':') + 1);
        } else if (strstr(line, "columns:")) {
            ventana_cols = atoi(strchr(line, ':') + 1);
        } else if (strstr(line, "tiempo:")) {
            leyendo_tiempo = 1;
            continue;
        } else if (leyendo_tiempo && strstr(line, "inicio:")) {
            printf("ENCONTRÉ EL TIEMPO DE INICIO\n");
            tiempo_inicio_ms = atoi(strchr(line, ':') + 1);
        } else if (leyendo_tiempo && strstr(line, "fin:")) {
            tiempo_fin_ms = atoi(strchr(line, ':') + 1);
        
        } else if (strstr(line, "figura:")) {
            leyendo_figura = 1;
            continue;
        } else if (leyendo_figura) {
            if (line[0] == '\n' || strlen(line) < 2)
                continue;

            char *ptr = strchr(line, '|');
            if (!ptr) continue;
            ptr++; // Salta primer '|'

            int col = 0;
            while (*ptr && *ptr != '\n' && col < MAX_COLS) {
                if (*ptr == '|') {
                    ptr++;
                    continue;
                }
                figura[fig_rows][col++] = *ptr++;
            }
            fig_cols = col;
            fig_rows++;
        }
    }

    fclose(file);
}

void dibujar_figura(int start_y, int start_x) {
    for (int i = 0; i < fig_rows; i++) {
        for (int j = 0; j < fig_cols; j++) {
            if (figura[i][j] != ' ')
                mvaddch(start_y + i, start_x + j, figura[i][j]);
        }
    }
}






void copiar_figura(char destino[MAX_ROWS][MAX_COLS], char origen[MAX_ROWS][MAX_COLS], int rows, int cols) {
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            destino[i][j] = origen[i][j];
}

void rotar_90() {
    char nueva[MAX_ROWS][MAX_COLS];
    int new_rows = fig_cols;
    int new_cols = fig_rows;

    for (int i = 0; i < fig_rows; i++) {
        for (int j = 0; j < fig_cols; j++) {
            nueva[j][fig_rows - 1 - i] = figura[i][j];
        }
    }

    copiar_figura(figura, nueva, new_rows, new_cols);
    fig_rows = new_rows;
    fig_cols = new_cols;
}

void rotar_180() {
    char nueva[MAX_ROWS][MAX_COLS];

    for (int i = 0; i < fig_rows; i++) {
        for (int j = 0; j < fig_cols; j++) {
            nueva[fig_rows - 1 - i][fig_cols - 1 - j] = figura[i][j];
        }
    }

    copiar_figura(figura, nueva, fig_rows, fig_cols);
}

void rotar_270() {
    char nueva[MAX_ROWS][MAX_COLS];
    int new_rows = fig_cols;
    int new_cols = fig_rows;

    for (int i = 0; i < fig_rows; i++) {
        for (int j = 0; j < fig_cols; j++) {
            nueva[fig_cols - 1 - j][i] = figura[i][j];
        }
    }

    copiar_figura(figura, nueva, new_rows, new_cols);
    fig_rows = new_rows;
    fig_cols = new_cols;
}


