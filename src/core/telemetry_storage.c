/*
 * telemetry_storage.c — Ring buffer en memoria para JSON de telemetría.
 *
 * Características
 * - Capacidad fija (TELEMETRY_MAX_ENTRIES) con inserción circular.
 * - Cada entrada conserva el JSON (texto) y un timestamp en ms.
 * - API sin dependencias de CoAP.
 */
#include "telemetry_storage.h"
#include "time_source.h"
#include <string.h>
#include <stdio.h>

// Estado interno del storage (singleton)
typedef struct {
    TelemetryEntry entries[TELEMETRY_MAX_ENTRIES];
    size_t head;            // Índice donde se inserta el siguiente
    size_t count;           // Cantidad actual de entradas
    size_t total_received;  // Total de mensajes recibidos
    uint64_t last_received_ms;
} TelemetryStorage;

static TelemetryStorage g_storage = {0};

/*
 * telemetry_storage_init
 * ----------------------
 * Inicializa/zera el estado interno del almacenamiento.
 */
void telemetry_storage_init(void) {
    memset(&g_storage, 0, sizeof(g_storage));
}

/*
 * telemetry_storage_add
 * ---------------------
 * Inserta un JSON crudo en el ring buffer asignando timestamp actual.
 *
 * Retorna 0 en éxito; negativo en error (longitud inválida o args nulos).
 */
int telemetry_storage_add(const char *json, size_t json_len) {
    if (!json || json_len == 0) return -1;
    if (json_len >= TELEMETRY_MAX_JSON_SIZE) return -2;

    // Copiar JSON al siguiente slot del ring buffer
    TelemetryEntry *entry = &g_storage.entries[g_storage.head];
    memcpy(entry->json, json, json_len);
    entry->json[json_len] = '\0';
    entry->json_length = json_len;
    entry->timestamp_ms = time_source_now_ms();

    // Actualizar índices del ring buffer
    g_storage.head = (g_storage.head + 1) % TELEMETRY_MAX_ENTRIES;
    if (g_storage.count < TELEMETRY_MAX_ENTRIES) {
        g_storage.count++;
    }
    g_storage.total_received++;
    g_storage.last_received_ms = entry->timestamp_ms;

    return 0;
}

/*
 * telemetry_storage_get_all
 * -------------------------
 * Copia hasta max_entries elementos en 'out' en orden cronológico (antiguo →
 * reciente). Devuelve la cantidad copiada.
 */
size_t telemetry_storage_get_all(TelemetryEntry *out, size_t max_entries) {
    if (!out || max_entries == 0) return 0;

    size_t copy_count = g_storage.count < max_entries ? g_storage.count : max_entries;
    
    // Si el buffer no está lleno, las entradas están al principio (0..count-1)
    if (g_storage.count < TELEMETRY_MAX_ENTRIES) {
        memcpy(out, g_storage.entries, copy_count * sizeof(TelemetryEntry));
    } else {
        // Buffer lleno: copiar desde head (más antiguo) hasta el final, luego desde 0
        size_t first_part = TELEMETRY_MAX_ENTRIES - g_storage.head;
        if (first_part > copy_count) first_part = copy_count;
        memcpy(out, &g_storage.entries[g_storage.head], first_part * sizeof(TelemetryEntry));
        
        if (copy_count > first_part) {
            size_t second_part = copy_count - first_part;
            memcpy(&out[first_part], g_storage.entries, second_part * sizeof(TelemetryEntry));
        }
    }

    return copy_count;
}

/*
 * telemetry_storage_get_stats
 * ---------------------------
 * Lee métricas básicas del almacenamiento.
 */
void telemetry_storage_get_stats(TelemetryStats *stats) {
    if (!stats) return;
    stats->total_received = g_storage.total_received;
    stats->current_count = g_storage.count;
    stats->capacity = TELEMETRY_MAX_ENTRIES;
    stats->last_received_ms = g_storage.last_received_ms;
}

/*
 * telemetry_storage_clear
 * -----------------------
 * Limpia el contenido del ring buffer.
 */
void telemetry_storage_clear(void) {
    g_storage.head = 0;
    g_storage.count = 0;
    g_storage.total_received = 0;
    g_storage.last_received_ms = 0;
}

/*
 * telemetry_storage_serialize_json
 * --------------------------------
 * Serializa todas las entradas en un arreglo JSON sin dependencias externas.
 * Retorna longitud escrita o negativo en error.
 */
int telemetry_storage_serialize_json(char *out, size_t out_size) {
    if (!out || out_size == 0) return -1;

    TelemetryEntry entries[TELEMETRY_MAX_ENTRIES];
    size_t count = telemetry_storage_get_all(entries, TELEMETRY_MAX_ENTRIES);

    // Construir JSON array manualmente (sin dependencias externas)
    size_t offset = 0;
    int n;

    // Inicio del array
    n = snprintf(out + offset, out_size - offset, "[");
    if (n < 0 || (size_t)n >= out_size - offset) return -2;
    offset += (size_t)n;

    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            n = snprintf(out + offset, out_size - offset, ",");
            if (n < 0 || (size_t)n >= out_size - offset) return -2;
            offset += (size_t)n;
        }

        // Agregar objeto: {"data": {...}, "timestamp": ...}
        n = snprintf(out + offset, out_size - offset, 
                     "{\"data\":%.*s,\"timestamp\":%llu}",
                     (int)entries[i].json_length, entries[i].json,
                     (unsigned long long)entries[i].timestamp_ms);
        if (n < 0 || (size_t)n >= out_size - offset) return -2;
        offset += (size_t)n;
    }

    // Cierre del array
    n = snprintf(out + offset, out_size - offset, "]");
    if (n < 0 || (size_t)n >= out_size - offset) return -2;
    offset += (size_t)n;

    return (int)offset;
}
