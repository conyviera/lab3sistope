/* --- funciones.c --- */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "funciones.h"

#define MAX_PROCESOS 1000

// Estructuras de datos globales
Proceso* procesos_pendientes[MAX_PROCESOS];
int total_procesos = 0;

Proceso* runqueue[MAX_PROCESOS];
int runqueue_size = 0;

int procesos_terminados = 0;
int tiempo_actual = 0;

// Estructura mejorada para procesos bloqueados con tiempo de bloqueo
typedef struct {
    Proceso* proceso;
    int tiempo_bloqueo_restante;
    int tiempo_inicio_bloqueo;
} ProcesoBloqueado;

ProcesoBloqueado bloqueados[MAX_PROCESOS];
int bloqueados_size = 0;

// Mutex para sincronización
pthread_mutex_t mutex_runqueue = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_bloqueados = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_stats = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_tiempo = PTHREAD_MUTEX_INITIALIZER;

// Variables de configuración
int quantum = 0;
float prob_bloqueo = 0;

// Funciones auxiliares optimizadas
static inline int obtener_tiempo_actual() {
    pthread_mutex_lock(&mutex_tiempo);
    int t = tiempo_actual;
    pthread_mutex_unlock(&mutex_tiempo);
    return t;
}

static inline void incrementar_tiempo(int incremento) {
    pthread_mutex_lock(&mutex_tiempo);
    tiempo_actual += incremento;
    pthread_mutex_unlock(&mutex_tiempo);
}

/**
 * Entradas: archivo - nombre del archivo con los procesos
 * Salidas: ninguna
 * Descripción: Carga los procesos desde archivo y los almacena en memoria
 */
void cargarProcesos(const char* archivo) {
    FILE* f = fopen(archivo, "r");
    if (!f) {
        perror("Error al abrir el archivo");
        exit(EXIT_FAILURE);
    }

    int pid = 0;
    int tiempo_llegada, tiempo_servicio;
    
    while (fscanf(f, "%d %d %d", &pid, &tiempo_llegada, &tiempo_servicio) == 3) {
        if (total_procesos >= MAX_PROCESOS) {
            fprintf(stderr, "Error: Demasiados procesos\n");
            break;
        }
        
        Proceso* p = malloc(sizeof(Proceso));
        if (!p) {
            perror("Error al asignar memoria");
            exit(EXIT_FAILURE);
        }
        
        p->pid = pid;
        p->tiempo_llegada = tiempo_llegada;
        p->tiempo_servicio = tiempo_servicio;
        p->tiempo_restante = tiempo_servicio;
        p->esta_bloqueado = 0;
        p->tiempo_entrada_runqueue = -1;
        p->tiempo_primera_ejecucion = -1;
        p->fue_ejecutado = 0;
        p->last_core = -1;
        p->tiempo_total_espera = 0;
        p->tiempo_total_bloqueo = 0;
        p->veces_bloqueado = 0;

        procesos_pendientes[total_procesos++] = p;
        printf("[LOAD] Proceso PID %d, llegada %d, servicio %d\n", 
               p->pid, p->tiempo_llegada, p->tiempo_servicio);
    }
    fclose(f);
}

/**
 * Entradas: arg - puntero a estructura Estadisticas
 * Salidas: NULL
 * Descripción: Función principal del planificador que ejecuta procesos
 */
void* planificador(void* arg) {
    Estadisticas* est = (Estadisticas*)arg;
    
    while (__sync_fetch_and_add(&procesos_terminados, 0) < total_procesos) {
        
        // Obtener proceso de la runqueue
        pthread_mutex_lock(&mutex_runqueue);
        if (runqueue_size == 0) {
            pthread_mutex_unlock(&mutex_runqueue);
            usleep(1000); // Esperar 1ms si no hay procesos
            continue;
        }
        
        Proceso* p = runqueue[0];
        // Remover proceso de la runqueue (FIFO)
        for (int j = 0; j < runqueue_size - 1; j++) {
            runqueue[j] = runqueue[j + 1];
        }
        runqueue_size--;
        pthread_mutex_unlock(&mutex_runqueue);
        
        // Actualizar estadísticas
        est->procesos_ejecutados++;
        p->last_core = est->id;
        
        int tiempo_inicio_ejecucion = obtener_tiempo_actual();
        
        // Calcular tiempo de espera si es la primera ejecución
        if (!p->fue_ejecutado) {
            p->tiempo_primera_ejecucion = tiempo_inicio_ejecucion;
            p->fue_ejecutado = 1;
            if (p->tiempo_entrada_runqueue >= 0) {
                int tiempo_espera = p->tiempo_primera_ejecucion - p->tiempo_entrada_runqueue;
                p->tiempo_total_espera += tiempo_espera;
                est->tiempo_espera_total += tiempo_espera;
            }
        }
        
        printf("[CPU %d] Ejecutando PID %d, restante %d ms\n", 
               est->id, p->pid, p->tiempo_restante);
        
        // Determinar si el proceso se bloquea
        int se_bloquea = ((float)rand() / RAND_MAX) < prob_bloqueo;
        
        if (!se_bloquea) {
            // Ejecución normal sin bloqueo
            int tiempo_ejecucion = (p->tiempo_restante > quantum) ? quantum : p->tiempo_restante;
            
            printf("[CPU %d] PID %d ejecutando %d ms sin bloqueo\n", 
                   est->id, p->pid, tiempo_ejecucion);
            
            // Simular ejecución (sin dormir tanto tiempo real)
            usleep(tiempo_ejecucion * 10); // Reducir el tiempo real de simulación
            
            // Actualizar tiempos
            p->tiempo_restante -= tiempo_ejecucion;
            est->tiempo_utilizado += tiempo_ejecucion;
            incrementar_tiempo(tiempo_ejecucion);
            
        } else {
            // El proceso se bloquea durante la ráfaga
            int tiempo_antes_bloqueo = rand() % (quantum + 1);
            if (tiempo_antes_bloqueo > p->tiempo_restante) {
                tiempo_antes_bloqueo = p->tiempo_restante;
            }
            
            printf("[CPU %d] PID %d se bloquea después de %d ms\n", 
                   est->id, p->pid, tiempo_antes_bloqueo);
            
            // Simular ejecución antes del bloqueo
            if (tiempo_antes_bloqueo > 0) {
                usleep(tiempo_antes_bloqueo * 10); // Reducir tiempo real
                p->tiempo_restante -= tiempo_antes_bloqueo;
                est->tiempo_utilizado += tiempo_antes_bloqueo;
                incrementar_tiempo(tiempo_antes_bloqueo);
            }
            
            // Si el proceso terminó antes del bloqueo
            if (p->tiempo_restante <= 0) {
                printf("[CPU %d] PID %d terminó antes del bloqueo\n", est->id, p->pid);
                __sync_fetch_and_add(&procesos_terminados, 1);
                free(p);
                continue;
            }
            
            // Agregar a cola de bloqueados
            p->esta_bloqueado = 1;
            p->veces_bloqueado++;
            
            pthread_mutex_lock(&mutex_bloqueados);
            bloqueados[bloqueados_size].proceso = p;
            bloqueados[bloqueados_size].tiempo_bloqueo_restante = 100 + rand() % 401; // 100-500ms
            bloqueados[bloqueados_size].tiempo_inicio_bloqueo = obtener_tiempo_actual();
            bloqueados_size++;
            pthread_mutex_unlock(&mutex_bloqueados);
            
            continue;
        }
        
        // Verificar si el proceso terminó
        if (p->tiempo_restante <= 0) {
            printf("[CPU %d] PID %d terminado\n", est->id, p->pid);
            __sync_fetch_and_add(&procesos_terminados, 1);
            free(p);
        } else {
            // Devolver proceso a la runqueue
            printf("[CPU %d] PID %d devuelto a runqueue con %d ms restantes\n", 
                   est->id, p->pid, p->tiempo_restante);
            
            pthread_mutex_lock(&mutex_runqueue);
            p->tiempo_entrada_runqueue = obtener_tiempo_actual();
            runqueue[runqueue_size++] = p;
            pthread_mutex_unlock(&mutex_runqueue);
        }
    }
    
    pthread_exit(NULL);
}

/**
 * Entradas: arg - puntero a array de estadísticas
 * Salidas: NULL
 * Descripción: Maneja los procesos bloqueados simulando E/S en porciones de 10ms
 */
void* manejador_bloqueos(void* arg) {
    Estadisticas* estadisticas = (Estadisticas*)arg;
    
    while (__sync_fetch_and_add(&procesos_terminados, 0) < total_procesos) {
        pthread_mutex_lock(&mutex_bloqueados);
        
        if (bloqueados_size == 0) {
            pthread_mutex_unlock(&mutex_bloqueados);
            usleep(1000); // Reducir tiempo de espera
            continue;
        }
        
        // Procesar cada proceso bloqueado en porciones de 10ms
        for (int i = 0; i < bloqueados_size; i++) {
            ProcesoBloqueado* pb = &bloqueados[i];
            
            // Simular 10ms de E/S
            pb->tiempo_bloqueo_restante -= 10;
            
            // Si el proceso terminó su E/S
            if (pb->tiempo_bloqueo_restante <= 0) {
                Proceso* p = pb->proceso;
                p->esta_bloqueado = 0;
                
                // Calcular tiempo total de bloqueo
                int tiempo_total_bloqueo = obtener_tiempo_actual() - pb->tiempo_inicio_bloqueo;
                p->tiempo_total_bloqueo += tiempo_total_bloqueo;
                
                // Actualizar estadísticas del procesador que lo bloqueó
                if (p->last_core >= 0) {
                    pthread_mutex_lock(&mutex_stats);
                    estadisticas[p->last_core].tiempo_bloqueo_total += tiempo_total_bloqueo;
                    estadisticas[p->last_core].bloqueos_count++;
                    pthread_mutex_unlock(&mutex_stats);
                }
                
                // Devolver proceso a runqueue
                pthread_mutex_lock(&mutex_runqueue);
                p->tiempo_entrada_runqueue = obtener_tiempo_actual();
                runqueue[runqueue_size++] = p;
                pthread_mutex_unlock(&mutex_runqueue);
                
                printf("[IO] PID %d completó E/S, vuelve a runqueue\n", p->pid);
                
                // Remover de la lista de bloqueados
                for (int j = i; j < bloqueados_size - 1; j++) {
                    bloqueados[j] = bloqueados[j + 1];
                }
                bloqueados_size--;
                i--; // Ajustar índice
            }
        }
        
        pthread_mutex_unlock(&mutex_bloqueados);
        
        // Simular 10ms de tiempo real (reducido)
        usleep(1000);
    }
    
    pthread_exit(NULL);
}

/**
 * Entradas: arg - no utilizado
 * Salidas: NULL
 * Descripción: Hilo reloj que ingresa procesos a la runqueue según su tiempo de llegada
 */
void* hilo_reloj(void* arg) {
    while (__sync_fetch_and_add(&procesos_terminados, 0) < total_procesos) {
        int tiempo_act = obtener_tiempo_actual();
        
        // Verificar procesos pendientes
        for (int i = 0; i < total_procesos; i++) {
            Proceso* p = procesos_pendientes[i];
            if (p != NULL && p->tiempo_llegada <= tiempo_act) {
                // Agregar proceso a runqueue
                pthread_mutex_lock(&mutex_runqueue);
                p->tiempo_entrada_runqueue = tiempo_act;
                runqueue[runqueue_size++] = p;
                pthread_mutex_unlock(&mutex_runqueue);
                
                procesos_pendientes[i] = NULL;
                printf("[RELOJ] PID %d ingresado a runqueue en t=%d\n", p->pid, tiempo_act);
            }
        }
        
        // Avanzar tiempo de simulación
        usleep(10000); // Simular 10ms
        incrementar_tiempo(10);
    }
    
    pthread_exit(NULL);
}