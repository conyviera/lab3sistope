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
    /* Nuevos campos para separar tipos de espera */
    int tiempo_espera_inicial;     /* Espera antes de primera ejecución */
    int tiempo_espera_reencolado;  /* Espera en reencolados posteriores */
    int ya_tuvo_primer_bloqueo;    /* Flag para contar solo un bloqueo por proceso */
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
    int tiempo_espera_inicial_total;    /* Suma de esperas antes de primera ejecución */
    int tiempo_espera_reencolado_total; /* Suma de esperas en reencolados */
    int tiempo_bloqueo_total;
    int bloqueos_count;                 /* Cuenta procesos únicos bloqueados */
} Estadisticas;

/* Prototipos */
void cargarProcesos(const char* archivo);
void* planificador(void* arg);
void* manejador_bloqueos(void* arg);
void* hilo_reloj(void* arg);
int obtener_tiempo_simulacion_total(void);

#endif /* FUNCIONES_H */