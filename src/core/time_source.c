/*
 * time_source.c — Fuente de tiempo inyectable para tests.
 *
 * Permite reemplazar la función de obtención de tiempo en ms para pruebas
 * determinísticas.
 */
#include "time_source.h"
#include "platform.h"

static TimeSource *g_ts = 0;

/*
 * default_now_ms
 * --------------
 * Implementación por defecto basada en platform_get_time_ms().
 */
static uint64_t default_now_ms(void) {
    return platform_get_time_ms();
}

/*
 * time_source_now_ms
 * ------------------
 * Devuelve el tiempo actual en ms usando la fuente inyectada si existe; de lo
 * contrario, usa la implementación por defecto.
 */
uint64_t time_source_now_ms(void) {
    if (g_ts && g_ts->now_ms) return g_ts->now_ms();
    return default_now_ms();
}

/*
 * time_source_set
 * ---------------
 * Establece la fuente de tiempo a utilizar (se mantiene un puntero global).
 */
void time_source_set(TimeSource *ts) {
    g_ts = ts;
}
