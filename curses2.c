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

int main() {
    leer_figura("config.yaml");

    initscr();
    noecho();
    curs_set(FALSE);
    nodelay(stdscr, TRUE);

    // Intenta redimensionar la ventana si es posible
    resizeterm(ventana_rows, ventana_cols);

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    int pos_x = 0;
    int pos_y = 2;

while (1) {
    clear();
    dibujar_figura(pos_y, pos_x);
    refresh();

    pos_x++;
    if (pos_x + fig_cols >= max_x)
        pos_x = 0;

    usleep(100000);

    int ch = getch();
    if (ch == 'q')
        break;
    else if (ch == 'r') // rotar 90°
        rotar_90();
    else if (ch == 't') // rotar 180°
        rotar_180();
    else if (ch == 'y') // rotar 270°
        rotar_270();
}

    endwin();
    return 0;
}
