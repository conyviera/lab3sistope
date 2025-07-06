all: desafio3

desafio3: desafio3.c funciones.c funciones.h
	gcc -o desafio3 desafio3.c funciones.c -lpthread

clean:
	rm -f desafio3
