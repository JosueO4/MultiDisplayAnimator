#!/bin/bash

YAML="config.yaml"

# Verifica que exista el binario
if [ ! -f "./testCurses" ]; then
    echo "Compila primero tu programa con: gcc testCurses.c -o testCurses -lncurses"
    exit 1
fi

# Extraer tamaño desde el YAML
ROWS=$(grep "rows:" "$YAML" | awk '{print $2}')
COLS=$(grep "columns:" "$YAML" | awk '{print $2}')

# Verificar si comandos están disponibles
if command -v gnome-terminal &>/dev/null; then
    gnome-terminal --geometry=${COLS}x${ROWS} -- bash -c "./testCurses; exec bash"
elif command -v xterm &>/dev/null; then
    xterm -geometry ${COLS}x${ROWS} -e ./testCurses
else
    echo "No se encontró un emulador de terminal compatible (gnome-terminal o xterm)"
    exit 1
fi
