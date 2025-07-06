/* --- desafio3.c --- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "funciones.h"

int num_nucleos;

int main(int argc, char* argv[]) {
    const char* archivo = NULL;
    /* Las variables quantum, prob_bloqueo y scale_factor vienen de funciones.c */
    /* scale_factor por defecto ya está inicializada a 0.001 */
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--data") == 0 && i+1 < argc) archivo = argv[++i];
        else if (strcmp(argv[i], "--numproc") == 0 && i+1 < argc) num_nucleos = atoi(argv[++i]);
        else if (strcmp(argv[i], "--quantum") == 0 && i+1 < argc) quantum = atoi(argv[++i]);
        else if (strcmp(argv[i], "--prob") == 0 && i+1 < argc) prob_bloqueo = atof(argv[++i]);
        else if (strcmp(argv[i], "--scale") == 0 && i+1 < argc) scale_factor = atof(argv[++i]);
        else {
            fprintf(stderr, "Uso: %s --data file --numproc val --quantum val --prob val [--scale val]\n", argv[0]);
            return 1;
        }
    }
    if (!archivo || num_nucleos <= 0 || quantum <= 0) {
        fprintf(stderr, "Faltan parámetros obligatorios\n");
        return 1;
    }

    printf("=== CONFIG =====\n data=%s\n procs=%d\n quantum=%d\n prob=%.2f\n scale=%.3f\n================\n",
           archivo, num_nucleos, quantum, prob_bloqueo, scale_factor);

    srand(time(NULL));
    cargarProcesos(archivo);

    pthread_t hilos[num_nucleos];
    Estadisticas estad[num_nucleos];
    for (int i = 0; i < num_nucleos; i++) {
        estad[i].id = i;
        estad[i].procesos_ejecutados = 0;
        estad[i].tiempo_utilizado    = 0;
        estad[i].tiempo_espera_inicial_total    = 0;
        estad[i].tiempo_espera_reencolado_total = 0;
        estad[i].tiempo_bloqueo_total = 0;
        estad[i].bloqueos_count       = 0;
        pthread_create(&hilos[i], NULL, planificador, &estad[i]);
    }
    pthread_t r, b;
    pthread_create(&r, NULL, hilo_reloj, NULL);
    pthread_create(&b, NULL, manejador_bloqueos, estad);

    for (int i = 0; i < num_nucleos; i++) pthread_join(hilos[i], NULL);
    pthread_join(r, NULL);
    pthread_join(b, NULL);

    /* Obtener tiempo total de simulación */
    int tiempo_total = obtener_tiempo_simulacion_total();
    
    printf("\n=== ESTADÍSTICAS ===\n");
    printf("Tiempo total de simulación: %d ms\n\n", tiempo_total);
    
    /* Estadísticas por CPU */
    int total_tiempo_utilizado = 0;
    int total_procesos = 0;
    int total_bloqueos = 0;
    float total_espera_inicial = 0;
    float total_espera_reencolado = 0;
    float total_tiempo_bloqueo = 0;
    
    for (int i = 0; i < num_nucleos; i++) {
        Estadisticas* e = &estad[i];
        float utilizacion = tiempo_total > 0 ? 100.0f * e->tiempo_utilizado / tiempo_total : 0.0f;
        
        printf("CPU %d: proc=%d CPU=%dms (%.1f%%) esp_ini=%.2fms esp_reen=%.2fms blk=%.2fms(%d)\n",
               e->id,
               e->procesos_ejecutados,
               e->tiempo_utilizado,
               utilizacion,
               e->procesos_ejecutados > 0 ? (float)e->tiempo_espera_inicial_total / e->procesos_ejecutados : 0.0f,
               e->procesos_ejecutados > 0 ? (float)e->tiempo_espera_reencolado_total / e->procesos_ejecutados : 0.0f,
               e->bloqueos_count > 0 ? (float)e->tiempo_bloqueo_total / e->bloqueos_count : 0.0f,
               e->bloqueos_count);
        
        total_tiempo_utilizado += e->tiempo_utilizado;
        total_procesos += e->procesos_ejecutados;
        total_bloqueos += e->bloqueos_count;
        total_espera_inicial += e->tiempo_espera_inicial_total;
        total_espera_reencolado += e->tiempo_espera_reencolado_total;
        total_tiempo_bloqueo += e->tiempo_bloqueo_total;
    }
    
    /* Estadísticas globales */
    printf("\n=== RESUMEN GLOBAL ===\n");
    printf("Utilización total del sistema: %.1f%%\n", 
           tiempo_total > 0 ? 100.0f * total_tiempo_utilizado / (tiempo_total * num_nucleos) : 0.0f);
    printf("Procesos ejecutados: %d\n", total_procesos);
    printf("Procesos únicos bloqueados: %d\n", total_bloqueos);
    printf("Espera inicial promedio: %.2f ms\n", 
           total_procesos > 0 ? total_espera_inicial / total_procesos : 0.0f);
    printf("Espera reencolado promedio: %.2f ms\n", 
           total_procesos > 0 ? total_espera_reencolado / total_procesos : 0.0f);
    printf("Tiempo bloqueo promedio: %.2f ms\n", 
           total_bloqueos > 0 ? total_tiempo_bloqueo / total_bloqueos : 0.0f);
    
    return 0;
}make 