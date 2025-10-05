/*
 * utils.c — Utilidades de plataforma: init/cleanup, tiempo y strings de error.
 */
#include "platform.h"
#include "log.h"
#include <sys/time.h>

/*
 * platform_init
 * -------------
 * Inicialización opcional por plataforma. Actualmente solo imprime el nombre
 * de la plataforma para diagnóstico.
 */
void platform_init(void) {
	// Inicialización específica de plataforma si es necesaria
LOG_INFO("Plataforma: %s\n", PLATFORM_NAME);
}

/*
 * platform_cleanup
 * ----------------
 * Limpieza opcional por plataforma. Actualmente no se requiere acción.
 */
void platform_cleanup(void) {
	// Limpieza específica de plataforma si es necesaria
}

/*
 * platform_get_time_ms
 * --------------------
 * Devuelve el tiempo en milisegundos desde epoch (UTC). Implementación basada
 * en gettimeofday (suficiente para propósitos de temporización del event loop).
 */
uint64_t platform_get_time_ms(void) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

/*
 * platform_error_string
 * ---------------------
 * Traduce códigos PLATFORM_* a cadenas legibles para logs y diagnósticos.
 */
const char *platform_error_string(int error) {
	switch (error) {
		case PLATFORM_OK: return "OK";
		case PLATFORM_ERROR: return "Error general";
		case PLATFORM_EAGAIN: return "Recurso temporalmente no disponible";
		case PLATFORM_ENOMEM: return "Sin memoria";
		case PLATFORM_EINVAL: return "Argumento inválido";
		default: return "Error desconocido";
	}
}
