#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "funciones.h"

#define MAX_PROCESOS 100

Proceso* runqueue[MAX_PROCESOS];
int runqueue_size = 0;
Proceso* bloqueados[MAX_PROCESOS];
int bloqueados_size = 0;

pthread_mutex_t mutex_runqueue = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_bloqueados = PTHREAD_MUTEX_INITIALIZER;

int quantum = 0;
float prob_bloqueo = 0;

void cargarProcesos(const char* archivo) {
    FILE* f = fopen(archivo, "r");
    if (!f) {
        perror("Error al abrir el archivo");
        exit(EXIT_FAILURE);
    }
    int pid = 0, llegada, servicio;
    while (fscanf(f, "%d %d", &llegada, &servicio) == 2) {
        Proceso* p = malloc(sizeof(Proceso));
        p->pid = pid++;
        p->tiempo_llegada = llegada;
        p->tiempo_servicio = servicio;
        p->tiempo_restante = servicio;
        p->esta_bloqueado = 0;

        runqueue[runqueue_size++] = p;
    }
    fclose(f);
}

void* planificador(void* arg) {
    Estadisticas* est = (Estadisticas*)arg;

    while (1) {
        pthread_mutex_lock(&mutex_runqueue);
        if (runqueue_size == 0) {
            pthread_mutex_unlock(&mutex_runqueue);
            break;
        }

        Proceso* p = runqueue[--runqueue_size];
        pthread_mutex_unlock(&mutex_runqueue);

        est->procesos_ejecutados++;

        // ¿Se bloquea?
        int se_bloquea = ((float)rand() / RAND_MAX) < prob_bloqueo;

        if (!se_bloquea) {
            int tiempo = (p->tiempo_restante > quantum) ? quantum : p->tiempo_restante;
            usleep(tiempo * 1000);
            p->tiempo_restante -= tiempo;
            est->tiempo_utilizado += tiempo;
        } else {
            int tiempo = rand() % quantum;
            usleep(tiempo * 1000);
            p->tiempo_restante -= tiempo;
            p->esta_bloqueado = 1;

            pthread_mutex_lock(&mutex_bloqueados);
            bloqueados[bloqueados_size++] = p;
            pthread_mutex_unlock(&mutex_bloqueados);
            continue;
        }

        // Si no finalizó, volver a la cola
        if (p->tiempo_restante > 0) {
            pthread_mutex_lock(&mutex_runqueue);
            runqueue[runqueue_size++] = p;
            pthread_mutex_unlock(&mutex_runqueue);
        } else {
            free(p);
        }
    }

    pthread_exit(NULL);
}

void* manejador_bloqueos(void* arg) {
    while (1) {
        pthread_mutex_lock(&mutex_bloqueados);
        if (bloqueados_size == 0) {
            pthread_mutex_unlock(&mutex_bloqueados);
            usleep(10000);
            continue;
        }

        for (int i = 0; i < bloqueados_size; ++i) {
            Proceso* p = bloqueados[i];
            int duracion = 100 + rand() % 401;

            usleep(duracion * 1000);
            p->esta_bloqueado = 0;

            pthread_mutex_lock(&mutex_runqueue);
            runqueue[runqueue_size++] = p;
            pthread_mutex_unlock(&mutex_runqueue);

            // Eliminarlo de la cola de bloqueados
            for (int j = i; j < bloqueados_size - 1; ++j)
                bloqueados[j] = bloqueados[j + 1];
            bloqueados_size--;
            i--;
        }

        pthread_mutex_unlock(&mutex_bloqueados);
        usleep(10000);
    }
}
