#define _POSIX_C_SOURCE 200809L
#include <unistd.h>     /* usleep */
#include "funciones.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_PROCESOS 1000

/* Config globals */
int    quantum       = 0;
float  prob_bloqueo  = 0.0f;
double scale_factor  = 1.0;

Proceso* procesos_pendientes[MAX_PROCESOS];
int      total_procesos = 0;

Proceso* runqueue[MAX_PROCESOS];
int      runqueue_size  = 0;

volatile int procesos_terminados = 0;
static int    tiempo_actual      = 0;

ProcesoBloqueado bloqueados[MAX_PROCESOS];
int              bloqueados_size = 0;

static pthread_mutex_t mutex_runqueue   = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_bloqueados = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_stats      = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_tiempo     = PTHREAD_MUTEX_INITIALIZER;

/* Tiempo simulado */
static inline int obtener_tiempo_actual(void) {
    pthread_mutex_lock(&mutex_tiempo);
    int t = tiempo_actual;
    pthread_mutex_unlock(&mutex_tiempo);
    return t;
}
static inline void incrementar_tiempo(int ms) {
    pthread_mutex_lock(&mutex_tiempo);
    tiempo_actual += ms;
    pthread_mutex_unlock(&mutex_tiempo);
}

/* Carga procesos admitiendo llegadas con decimales */
void cargarProcesos(const char* archivo) {
    FILE* f = fopen(archivo, "r");
    if (!f) { perror("fopen"); exit(EXIT_FAILURE); }
    int pid, servicio;
    double llegada_f;
    while (total_procesos < MAX_PROCESOS &&
           fscanf(f, "%d %lf %d", &pid, &llegada_f, &servicio) == 3) {
        Proceso* p = malloc(sizeof *p);
        if (!p) { perror("malloc"); exit(EXIT_FAILURE); }
        p->pid              = pid;
        p->tiempo_llegada   = (int)(llegada_f + 0.5);
        p->tiempo_servicio  = servicio;
        p->tiempo_restante  = servicio;
        p->esta_bloqueado   = 0;
        p->tiempo_entrada_runqueue  = -1;
        p->tiempo_primera_ejecucion = -1;
        p->fue_ejecutado    = 0;
        p->last_core        = -1;
        p->veces_bloqueado  = 0;
        p->tiempo_total_espera   = 0;
        p->tiempo_total_bloqueo  = 0;
        procesos_pendientes[total_procesos++] = p;
        printf("[LOAD] PID=%d llegada=%.2f→%d servicio=%d\n",
               pid, llegada_f, p->tiempo_llegada, servicio);
    }
    fclose(f);
}

/* Planificador Round Robin, acumula toda la espera */
void* planificador(void* arg) {
    Estadisticas* est = arg;
    while (__sync_fetch_and_add(&procesos_terminados, 0) < total_procesos) {
        pthread_mutex_lock(&mutex_runqueue);
        if (!runqueue_size) {
            pthread_mutex_unlock(&mutex_runqueue);
            usleep((unsigned)(1 * 1000 * scale_factor));
            continue;
        }
        Proceso* p = runqueue[0];
        for (int i = 1; i < runqueue_size; i++) runqueue[i-1] = runqueue[i];
        runqueue_size--;
        pthread_mutex_unlock(&mutex_runqueue);

        int ahora = obtener_tiempo_actual();
        /* Medir tiempo de espera desde última encolada */
        if (p->tiempo_entrada_runqueue >= 0) {
            int espera = ahora - p->tiempo_entrada_runqueue;
            p->tiempo_total_espera += espera;
            pthread_mutex_lock(&mutex_stats);
            est->tiempo_espera_total += espera;
            pthread_mutex_unlock(&mutex_stats);
        }

        est->procesos_ejecutados++;
        p->last_core = est->id;

        int bloquea = ((float)rand()/RAND_MAX) < prob_bloqueo;
        int uso = p->tiempo_restante < quantum ? p->tiempo_restante : quantum;
        if (bloquea) {
            int antes = rand() % (quantum + 1);
            if (antes > p->tiempo_restante) antes = p->tiempo_restante;
            uso = antes;
        }

        printf("[CPU %d|t=%d] PID=%d usa %d ms%s\n",
               est->id, ahora, p->pid, uso,
               bloquea ? " (bloquea)" : "");

        usleep((unsigned)(uso * 1000 * scale_factor));
        incrementar_tiempo(uso);
        p->tiempo_restante -= uso;
        est->tiempo_utilizado += uso;

        if (bloquea && p->tiempo_restante > 0) {
            /* Evento de bloqueo: contar solo una vez */
            p->esta_bloqueado = 1;
            p->veces_bloqueado++;
            pthread_mutex_lock(&mutex_stats);
            est->bloqueos_count++;
            pthread_mutex_unlock(&mutex_stats);

            pthread_mutex_lock(&mutex_bloqueados);
            bloqueados[bloqueados_size++] = (ProcesoBloqueado){
                .proceso                = p,
                .tiempo_bloqueo_restante = 100 + rand()%401,
                .tiempo_inicio_bloqueo  = obtener_tiempo_actual()
            };
            pthread_mutex_unlock(&mutex_bloqueados);
            continue;
        }

        if (p->tiempo_restante <= 0) {
            printf("[CPU %d|t=%d] PID=%d terminado\n",
                   est->id, obtener_tiempo_actual(), p->pid);
            __sync_add_and_fetch(&procesos_terminados, 1);
            free(p);
        } else {
            /* Re-encolar y marcar tiempo de entrada */
            pthread_mutex_lock(&mutex_runqueue);
            p->tiempo_entrada_runqueue = obtener_tiempo_actual();
            runqueue[runqueue_size++] = p;
            pthread_mutex_unlock(&mutex_runqueue);
        }
    }
    return NULL;
}

/* Manejador de bloqueos: acumula solo tiempo, no cuenta eventos */
void* manejador_bloqueos(void* arg) {
    Estadisticas* stats = arg;
    while (__sync_fetch_and_add(&procesos_terminados, 0) < total_procesos) {
        pthread_mutex_lock(&mutex_bloqueados);
        for (int i = 0; i < bloqueados_size; i++) {
            ProcesoBloqueado* pb = &bloqueados[i];
            usleep((unsigned)(10 * 1000 * scale_factor));
            incrementar_tiempo(10);
            pb->tiempo_bloqueo_restante -= 10;
            /* Acumular 10 ms de bloqueo */
            pthread_mutex_lock(&mutex_stats);
            stats[pb->proceso->last_core].tiempo_bloqueo_total += 10;
            pthread_mutex_unlock(&mutex_stats);

            if (pb->tiempo_bloqueo_restante <= 0) {
                Proceso* p = pb->proceso;
                p->esta_bloqueado = 0;
                int fin = obtener_tiempo_actual();
                p->tiempo_total_bloqueo += fin - pb->tiempo_inicio_bloqueo;

                pthread_mutex_lock(&mutex_runqueue);
                p->tiempo_entrada_runqueue = fin;
                runqueue[runqueue_size++] = p;
                pthread_mutex_unlock(&mutex_runqueue);
                printf("[IO|t=%d] PID=%d sale de I/O\n", fin, p->pid);

                /* Eliminar de bloqueados */
                for (int j = i; j+1 < bloqueados_size; j++)
                    bloqueados[j] = bloqueados[j+1];
                bloqueados_size--;
                i--;
            }
        }
        pthread_mutex_unlock(&mutex_bloqueados);
    }
    return NULL;
}


/* Hilo reloj que avanza 10ms simulados */
void* hilo_reloj(void* arg) {
    (void)arg;
    while (__sync_fetch_and_add(&procesos_terminados, 0) < total_procesos) {
        int now = obtener_tiempo_actual();
        for (int i = 0; i < total_procesos; i++) {
            Proceso* p = procesos_pendientes[i];
            if (p && p->tiempo_llegada <= now) {
                pthread_mutex_lock(&mutex_runqueue);
                p->tiempo_entrada_runqueue = now;
                runqueue[runqueue_size++] = p;
                pthread_mutex_unlock(&mutex_runqueue);
                procesos_pendientes[i] = NULL;
                printf("[RELOJ|t=%d] PID=%d en runqueue\n", now, p->pid);
            }
        }
        usleep((unsigned)(10 * 1000 * scale_factor));
        incrementar_tiempo(10);
    }
    return NULL;
}