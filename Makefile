
make:
	gcc -D_XOPEN_SOURCE=700 -o programa testHilos.c myPthreads.c scheduler.c

animacion:
	gcc -o animar animacion_hilos.c myPthreads.c scheduler.c -lncurses

clean:
	rm -f testCurses
	

