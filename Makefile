
make:
	gcc -D_XOPEN_SOURCE=700 -o programa testHilos.c myPthreads.c scheduler.c

animacion:
	gcc -o testCurses animacion_hilos.c myPthreads.c scheduler.c curses2.c -lncurses

clean:
	rm -f testCurses
	

