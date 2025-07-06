#ifndef FUNCIONES_H
#define FUNCIONES_H

extern int quantum;
extern float prob_bloqueo;


typedef struct {
    int pid;
    int tiempo_servicio;
    int tiempo_llegada;
    int tiempo_restante;
    int esta_bloqueado;
} Proceso;

typedef struct {
    int id;
    int procesos_ejecutados;
    int tiempo_utilizado;
    int tiempo_espera_total;
} Estadisticas;

void cargarProcesos(const char* archivo);
void* planificador(void* arg);
void* manejador_bloqueos(void* arg);
void* hilo_reloj(void* arg);



#endif
