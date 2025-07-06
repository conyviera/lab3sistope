#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include "funciones.h"

int num_nucleos;
pthread_t hilo_ingresador;


int main(int argc, char* argv[]) {
    const char* archivo = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--data")) archivo = argv[++i];
        else if (!strcmp(argv[i], "--numproc")) num_nucleos = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--quantum")) quantum = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--prob")) prob_bloqueo = atof(argv[++i]);
    }

    if (!archivo || num_nucleos <= 0 || quantum <= 0) {
        fprintf(stderr, "Uso: ./desafio3 --data file --numproc val --quantum val --prob val\n");
        return 1;
    }

    srand(time(NULL));
    cargarProcesos(archivo);

    pthread_t hilos[num_nucleos];
    Estadisticas estadisticas[num_nucleos];

    for (int i = 0; i < num_nucleos; i++) {
        estadisticas[i].id = i;
        estadisticas[i].procesos_ejecutados = 0;
        estadisticas[i].tiempo_utilizado = 0;
        estadisticas[i].tiempo_espera_total = 0;
        pthread_create(&hilos[i], NULL, planificador, &estadisticas[i]);
    }

    pthread_create(&hilo_ingresador, NULL, hilo_reloj, NULL);


    pthread_t hilo_bloqueador;
    pthread_create(&hilo_bloqueador, NULL, manejador_bloqueos, NULL);

    for (int i = 0; i < num_nucleos; i++) {
    pthread_join(hilos[i], NULL);
    }

    pthread_join(hilo_ingresador, NULL);       // si agregaste el hilo reloj
    pthread_cancel(hilo_bloqueador);           // opcional pero recomendable
    pthread_join(hilo_bloqueador, NULL);       // espera que termine

    for (int i = 0; i < num_nucleos; i++) {
        Estadisticas e = estadisticas[i];
        printf("Procesador %d:\n", e.id);
        printf("  Procesos ejecutados: %d\n", e.procesos_ejecutados);
        printf("  Tiempo de CPU usado: %d ms\n", e.tiempo_utilizado);
    }

    return 0;

}
