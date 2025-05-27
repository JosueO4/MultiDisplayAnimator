#!/bin/bash

WIDTH=50
HEIGHT=10
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
gnome-terminal --geometry=${WIDTH}x${HEIGHT}+${POS1_X}+${POS1_Y} -- bash -c "./client; exec bash"
gnome-terminal --geometry=${WIDTH}x${HEIGHT}+${POS2_X}+${POS2_Y} -- bash -c "./client; exec bash"

