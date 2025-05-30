#!/bin/bash

WIDTH=$(grep 'width:' config.yaml | awk '{print $2}')
HEIGHT=$(grep 'height:' config.yaml | awk '{print $2}')
MONITORES=$(grep 'monitores:' config.yaml | awk '{print $2}')
POS1_X=100
POS1_Y=100
POS2_X=600
POS2_Y=100
POS3_X=1100
POS3_Y=100

make animacion
[ ! -f client ] && gcc client.c -o client

gnome-terminal --geometry=${WIDTH}x${HEIGHT}+0+0 -- bash -c "./testCurses; exec bash"
sleep 1
for ((i=0; i<MONITORES; i++)); do
    POS_X=$((100 + i * 500)) # Calcula la posición X para cada cliente
    POS_Y=100
    gnome-terminal --geometry=${WIDTH}x${HEIGHT}+${POS_X}+${POS_Y} -- bash -c "./client; exec bash"
done
