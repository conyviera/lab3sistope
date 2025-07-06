/* --- funciones.h --- */
#ifndef FUNCIONES_H
#define FUNCIONES_H

#include <pthread.h>

/* Variables de configuración (definidas en funciones.c) */
extern int    quantum;
extern float  prob_bloqueo;
extern double scale_factor;

/* Estructura de un proceso */
typedef struct {
    int pid;
    int tiempo_servicio;
    int tiempo_llegada;
    int tiempo_restante;
    int esta_bloqueado;
    int tiempo_entrada_runqueue;
    int tiempo_primera_ejecucion;
    int fue_ejecutado;
    int last_core;
    int veces_bloqueado;
    int tiempo_total_espera;
    int tiempo_total_bloqueo;
} Proceso;

/* Nodo para procesos bloqueados */
typedef struct {
    Proceso* proceso;
    int      tiempo_bloqueo_restante;
    int      tiempo_inicio_bloqueo;
} ProcesoBloqueado;

/* Estadísticas por núcleo */
typedef struct {
    int id;
    int procesos_ejecutados;
    int tiempo_utilizado;
    int tiempo_espera_total;
    int tiempo_bloqueo_total;
    int bloqueos_count;
} Estadisticas;

/* Prototipos */
void cargarProcesos(const char* archivo);
void* planificador(void* arg);
void* manejador_bloqueos(void* arg);
void* hilo_reloj(void* arg);

#endif /* FUNCIONES_H */

