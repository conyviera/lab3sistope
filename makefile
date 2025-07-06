# Makefile para desafio3
CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -std=c99
TARGET = desafio3
SOURCES = desafio3.c funciones.c
OBJECTS = $(SOURCES:.c=.o)

# Regla principal
all: $(TARGET)

# Compilar ejecutable
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

# Compilar archivos objeto
%.o: %.c funciones.h
	$(CC) $(CFLAGS) -c $< -o $@

# Limpiar archivos generados
clean:
	rm -f $(OBJECTS) $(TARGET)

# Ejecutar con parámetros por defecto
run: $(TARGET)
	./$(TARGET) --data input.txt --numproc 4 --quantum 800 --prob 0.2

# Ejecutar con diferentes configuraciones para pruebas
test1: $(TARGET)
	./$(TARGET) --data input.txt --numproc 2 --quantum 400 --prob 0.1

test2: $(TARGET)
	./$(TARGET) --data input.txt --numproc 8 --quantum 1200 --prob 0.3

# Compilar con información de depuración
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Verificar sintaxis
check:
	$(CC) $(CFLAGS) -fsyntax-only $(SOURCES)

.PHONY: all clean run test1 test2 debug check