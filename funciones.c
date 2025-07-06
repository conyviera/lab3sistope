#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "funciones.h"

#define MAX_PROCESOS 1000

Proceso* procesos_pendientes[MAX_PROCESOS];  // todos los procesos
int total_procesos = 0;

Proceso* runqueue[MAX_PROCESOS];
int runqueue_size = 0;

int procesos_terminados = 0;
int tiempo_actual = 0;

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

    int pid;
    float llegada_f;
    int servicio;
    while (fscanf(f, "%d %f %d", &pid, &llegada_f, &servicio) == 3) {
        Proceso* p = malloc(sizeof(Proceso));
        p->pid = pid;
        p->tiempo_llegada = (int)llegada_f;
        p->tiempo_servicio = servicio;
        p->tiempo_restante = servicio;
        p->esta_bloqueado = 0;

        procesos_pendientes[total_procesos++] = p;

        printf("[LOAD] Proceso PID %d, llegada %d, servicio %d\n", p->pid, p->tiempo_llegada, p->tiempo_servicio);
    }
    fclose(f);
}

void* planificador(void* arg) {
    Estadisticas* est = (Estadisticas*)arg;

    while (1) {
        if (procesos_terminados >= total_procesos) break;
        pthread_mutex_lock(&mutex_runqueue);
        if (runqueue_size == 0) {
            pthread_mutex_unlock(&mutex_runqueue);
            usleep(1000);  // Esperar un poco y volver a intentar
            continue;
        }

        Proceso* p = runqueue[0];
        for (int j = 0; j < runqueue_size - 1; j++)
            runqueue[j] = runqueue[j + 1];
        runqueue_size--;
        
        pthread_mutex_unlock(&mutex_runqueue);

        est->procesos_ejecutados++;
        printf("[CPU %d] Ejecutando PID %d, restante %d\n", est->id, p->pid, p->tiempo_restante);

        int se_bloquea = ((float)rand() / RAND_MAX) < prob_bloqueo;

        if (!se_bloquea) {
            int tiempo = (p->tiempo_restante > quantum) ? quantum : p->tiempo_restante;
            printf("[CPU %d] PID %d sin bloqueo, ejecutando %d ms\n", est->id, p->pid, tiempo);
            usleep(tiempo * 1000);
            p->tiempo_restante -= tiempo;
            est->tiempo_utilizado += tiempo;
        } else {
            int tiempo = rand() % quantum;
            printf("[CPU %d] PID %d se bloquea tras %d ms\n", est->id, p->pid, tiempo);
            usleep(tiempo * 1000);
            p->tiempo_restante -= tiempo;
            p->esta_bloqueado = 1;

            pthread_mutex_lock(&mutex_bloqueados);
            bloqueados[bloqueados_size++] = p;
            pthread_mutex_unlock(&mutex_bloqueados);
            continue;
        }

        if (p->tiempo_restante > 0) {
            printf("[CPU %d] PID %d vuelve a la cola con %d ms restantes\n", est->id, p->pid, p->tiempo_restante);
            pthread_mutex_lock(&mutex_runqueue);
            runqueue[runqueue_size++] = p;
            pthread_mutex_unlock(&mutex_runqueue);
        } else {
            printf("[CPU %d] PID %d finalizado\n", est->id, p->pid);
            free(p);
            __sync_fetch_and_add(&procesos_terminados, 1);  // atómico y seguro entre hilos

        }
    }

    pthread_exit(NULL);
}

void* manejador_bloqueos(void* arg) {
    while (1) {
        if (procesos_terminados >= total_procesos) break;
        pthread_mutex_lock(&mutex_bloqueados);
        if (bloqueados_size == 0) {
            pthread_mutex_unlock(&mutex_bloqueados);
            usleep(10000);
            continue;
        }

        for (int i = 0; i < bloqueados_size; ++i) {
            Proceso* p = bloqueados[i];
            int duracion = 100 + rand() % 401;

            printf("[IO] PID %d en RIO %d ms\n", p->pid, duracion);
            usleep(duracion * 1000);
            p->esta_bloqueado = 0;

            pthread_mutex_lock(&mutex_runqueue);
            runqueue[runqueue_size++] = p;
            pthread_mutex_unlock(&mutex_runqueue);
            printf("[IO] PID %d vuelve a runqueue\n", p->pid);

            for (int j = i; j < bloqueados_size - 1; ++j)
                bloqueados[j] = bloqueados[j + 1];
            bloqueados_size--;
            i--;
        }

        pthread_mutex_unlock(&mutex_bloqueados);
        usleep(10000);
    }
    total_procesos++;

}

void* hilo_reloj(void* arg) {
    while (procesos_terminados < total_procesos) {
        for (int i = 0; i < total_procesos; i++) {
            Proceso* p = procesos_pendientes[i];
            if (p != NULL && p->tiempo_llegada <= tiempo_actual) {
                pthread_mutex_lock(&mutex_runqueue);
                runqueue[runqueue_size++] = p;
                pthread_mutex_unlock(&mutex_runqueue);
                procesos_pendientes[i] = NULL;  // ya fue liberado
                printf("[RELOJ] PID %d ingresado a runqueue en t=%d\n", p->pid, tiempo_actual);
            }
        }
        usleep(10000);  // avanza 10 ms
        tiempo_actual += 10;
    }
    pthread_exit(NULL);
}
