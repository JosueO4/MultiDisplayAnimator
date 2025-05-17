#!/bin/zsh

# YAML
YAML="config.yaml"

# Extraer tamaño
ROWS=$(grep "rows:" "$YAML" | awk '{print $2}')
COLS=$(grep "columns:" "$YAML" | awk '{print $2}')

# Convertir a píxeles aproximados (cada celda ≈ 9x20 px)
WIDTH=$((COLS * 6))
HEIGHT=$((ROWS * 15))

# Ruta completa del ejecutable
EXEC_PATH="$(pwd)/testCurses"

# Validación
if [[ ! -f "$EXEC_PATH" ]]; then
    echo "No se encontró el ejecutable figura. Compílalo con: gcc main.c -o figura -lncurses"
    exit 1
fi

# Ejecutar AppleScript para abrir nueva ventana de Terminal.app
osascript <<EOF
tell application "Terminal"
    do script "cd \"$(pwd)\"; ./testCurses; exec zsh"
    delay 0.5
    set bounds of front window to {100, 100, $((100 + WIDTH)), $((100 + HEIGHT))}
    activate
end tell
EOF
