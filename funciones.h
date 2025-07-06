#ifndef FUNCIONES_H
#define FUNCIONES_H

#include <pthread.h>

// Variables globales de configuración
extern int quantum;
extern float prob_bloqueo;

// Estructura optimizada para procesos
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
    
    // Nuevos campos para mejores estadísticas
    int tiempo_total_espera;
    int tiempo_total_bloqueo;
    int veces_bloqueado;
} Proceso;

// Estructura optimizada para estadísticas
typedef struct {
    int id;
    int procesos_ejecutados;
    int tiempo_utilizado;
    int tiempo_espera_total;
    int tiempo_bloqueo_total;
    int bloqueos_count;
    
    // Nuevos campos para análisis de rendimiento
    int procesos_completados;
    int tiempo_inactivo;
} Estadisticas;

// Declaraciones de funciones
void cargarProcesos(const char* archivo);
void* planificador(void* arg);
void* manejador_bloqueos(void* arg);
void* hilo_reloj(void* arg);

#endif