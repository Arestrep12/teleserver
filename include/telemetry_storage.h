#ifndef TELEMETRY_STORAGE_H
#define TELEMETRY_STORAGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Capacidad del ring buffer (últimos N mensajes)
#define TELEMETRY_MAX_ENTRIES 100

// Tamaño máximo de un JSON de telemetría (bytes)
#define TELEMETRY_MAX_JSON_SIZE 512

// Estructura para una entrada de telemetría
typedef struct {
    char json[TELEMETRY_MAX_JSON_SIZE];
    size_t json_length;
    uint64_t timestamp_ms;  // Timestamp de recepción
} TelemetryEntry;

// Estadísticas del storage
typedef struct {
    size_t total_received;   // Total de mensajes recibidos desde el inicio
    size_t current_count;    // Cantidad actual en el buffer
    size_t capacity;         // Capacidad máxima del buffer
    uint64_t last_received_ms; // Timestamp del último mensaje
} TelemetryStats;

// Inicializa el módulo de storage
void telemetry_storage_init(void);

// Agrega un nuevo JSON de telemetría
// Retorna 0 en éxito, <0 en error
int telemetry_storage_add(const char *json, size_t json_len);

// Obtiene todas las entradas almacenadas
// Retorna el número de entradas copiadas
// out: buffer de salida (array de TelemetryEntry)
// max_entries: capacidad del buffer out
size_t telemetry_storage_get_all(TelemetryEntry *out, size_t max_entries);

// Obtiene estadísticas del storage
void telemetry_storage_get_stats(TelemetryStats *stats);

// Limpia todo el storage (para testing)
void telemetry_storage_clear(void);

// Serializa todas las entradas a un JSON array
// Retorna el tamaño del JSON generado, o <0 en error
// out: buffer de salida
// out_size: capacidad del buffer
int telemetry_storage_serialize_json(char *out, size_t out_size);

#endif // TELEMETRY_STORAGE_H
