/* --- desafio3.c --- */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "funciones.h"

int num_nucleos;
pthread_t hilo_ingresador;
pthread_t hilo_bloqueador;

/**
 * Entradas: argc - número de argumentos, argv - array de argumentos
 * Salidas: 0 si exitoso, 1 si error
 * Descripción: Función principal que inicializa y ejecuta la simulación
 */
int main(int argc, char* argv[]) {
    const char* archivo = NULL;
    
    // Inicializar valores por defecto
    num_nucleos = 0;
    quantum = 0;
    prob_bloqueo = 0.0;
    
    // Parsear argumentos de línea de comandos
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--data") && i + 1 < argc) {
            archivo = argv[++i];
        } else if (!strcmp(argv[i], "--numproc") && i + 1 < argc) {
            num_nucleos = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--quantum") && i + 1 < argc) {
            quantum = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--prob") && i + 1 < argc) {
            prob_bloqueo = atof(argv[++i]);
        }
    }
    
    // Validar parámetros
    if (!archivo || num_nucleos <= 0 || quantum <= 0 || prob_bloqueo < 0 || prob_bloqueo > 1) {
        fprintf(stderr, "Uso: %s --data file --numproc val --quantum val --prob val\n", argv[0]);
        fprintf(stderr, "Parámetros:\n");
        fprintf(stderr, "  --data: archivo de entrada con procesos\n");
        fprintf(stderr, "  --numproc: número de procesadores (>0)\n");
        fprintf(stderr, "  --quantum: quantum de tiempo en ms (>0)\n");
        fprintf(stderr, "  --prob: probabilidad de bloqueo (0.0-1.0)\n");
        return 1;
    }
    
    // Mostrar configuración
    printf("=== CONFIGURACIÓN DE LA SIMULACIÓN ===\n");
    printf("Archivo de datos: %s\n", archivo);
    printf("Número de procesadores: %d\n", num_nucleos);
    printf("Quantum: %d ms\n", quantum);
    printf("Probabilidad de bloqueo: %.2f\n", prob_bloqueo);
    printf("========================================\n\n");
    
    // Inicializar generador de números aleatorios
    srand(time(NULL));
    
    // Cargar procesos desde archivo
    printf("Cargando procesos...\n");
    cargarProcesos(archivo);
    printf("Procesos cargados exitosamente.\n\n");
    
    // Crear arrays para hilos y estadísticas
    pthread_t hilos[num_nucleos];
    Estadisticas estadisticas[num_nucleos];
    
    // Inicializar estadísticas
    for (int i = 0; i < num_nucleos; i++) {
        estadisticas[i].id = i;
        estadisticas[i].procesos_ejecutados = 0;
        estadisticas[i].tiempo_utilizado = 0;
        estadisticas[i].tiempo_espera_total = 0;
        estadisticas[i].tiempo_bloqueo_total = 0;
        estadisticas[i].bloqueos_count = 0;
        estadisticas[i].procesos_completados = 0;
        estadisticas[i].tiempo_inactivo = 0;
    }
    
    printf("Iniciando simulación...\n");
    
    // Crear hilos planificadores
    for (int i = 0; i < num_nucleos; i++) {
        if (pthread_create(&hilos[i], NULL, planificador, &estadisticas[i]) != 0) {
            perror("Error al crear hilo planificador");
            return 1;
        }
    }
    
    // Crear hilo reloj
    if (pthread_create(&hilo_ingresador, NULL, hilo_reloj, NULL) != 0) {
        perror("Error al crear hilo reloj");
        return 1;
    }
    
    // Crear hilo manejador de bloqueos
    if (pthread_create(&hilo_bloqueador, NULL, manejador_bloqueos, estadisticas) != 0) {
        perror("Error al crear hilo manejador de bloqueos");
        return 1;
    }
    
    // Esperar a que terminen todos los hilos planificadores con timeout
    printf("Esperando terminación de hilos...\n");
    for (int i = 0; i < num_nucleos; i++) {
        void* result;
        if (pthread_join(hilos[i], &result) != 0) {
            perror("Error al esperar hilo planificador");
        }
        printf("Hilo planificador %d terminado\n", i);
    }
    
    // Cancelar y esperar hilos auxiliares
    printf("Cancelando hilos auxiliares...\n");
    pthread_cancel(hilo_ingresador);
    pthread_cancel(hilo_bloqueador);
    
    pthread_join(hilo_ingresador, NULL);
    pthread_join(hilo_bloqueador, NULL);
    
    printf("Todos los hilos terminados.\n");
    
    printf("\nSimulación completada.\n");
    
    // Calcular estadísticas globales
    int total_procesos_ejecutados = 0;
    int total_tiempo_utilizado = 0;
    int total_tiempo_espera = 0;
    int total_bloqueos = 0;
    int total_tiempo_bloqueo = 0;
    
    // Imprimir estadísticas detalladas
    printf("\n=== ESTADÍSTICAS POR PROCESADOR ===\n");
    for (int i = 0; i < num_nucleos; i++) {
        Estadisticas e = estadisticas[i];
        
        printf("\nProcesador %d:\n", e.id);
        printf("  Procesos ejecutados: %d\n", e.procesos_ejecutados);
        printf("  Tiempo de CPU utilizado: %d ms\n", e.tiempo_utilizado);
        
        if (e.procesos_ejecutados > 0) {
            printf("  Tiempo promedio de espera: %.2f ms\n", 
                   (float)e.tiempo_espera_total / e.procesos_ejecutados);
        } else {
            printf("  Tiempo promedio de espera: 0.00 ms\n");
        }
        
        if (e.bloqueos_count > 0) {
            printf("  Tiempo promedio de bloqueo: %.2f ms\n", 
                   (float)e.tiempo_bloqueo_total / e.bloqueos_count);
        } else {
            printf("  Tiempo promedio de bloqueo: 0.00 ms\n");
        }
        
        printf("  Número de bloqueos manejados: %d\n", e.bloqueos_count);
        
        // Calcular utilización del procesador
        int tiempo_total_simulacion = e.tiempo_utilizado + e.tiempo_inactivo;
        if (tiempo_total_simulacion > 0) {
            float utilizacion = (float)e.tiempo_utilizado / tiempo_total_simulacion * 100;
            printf("  Utilización del procesador: %.2f%%\n", utilizacion);
        }
        
        // Actualizar totales
        total_procesos_ejecutados += e.procesos_ejecutados;
        total_tiempo_utilizado += e.tiempo_utilizado;
        total_tiempo_espera += e.tiempo_espera_total;
        total_bloqueos += e.bloqueos_count;
        total_tiempo_bloqueo += e.tiempo_bloqueo_total;
    }
    
    // Estadísticas globales
    printf("\n=== ESTADÍSTICAS GLOBALES ===\n");
    printf("Total de procesos ejecutados: %d\n", total_procesos_ejecutados);
    printf("Tiempo total de CPU utilizado: %d ms\n", total_tiempo_utilizado);
    printf("Tiempo promedio de CPU por procesador: %.2f ms\n", 
           (float)total_tiempo_utilizado / num_nucleos);
    
    if (total_procesos_ejecutados > 0) {
        printf("Tiempo promedio de espera global: %.2f ms\n", 
               (float)total_tiempo_espera / total_procesos_ejecutados);
    }
    
    if (total_bloqueos > 0) {
        printf("Tiempo promedio de bloqueo global: %.2f ms\n", 
               (float)total_tiempo_bloqueo / total_bloqueos);
    }
    
    printf("Total de bloqueos en el sistema: %d\n", total_bloqueos);
    printf("================================\n");
    
    return 0;
}