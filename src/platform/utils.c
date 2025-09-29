#include "platform.h"
#include "log.h"
#include <sys/time.h>

void platform_init(void) {
	// Inicialización específica de plataforma si es necesaria
LOG_INFO("Plataforma: %s\n", PLATFORM_NAME);
}

void platform_cleanup(void) {
	// Limpieza específica de plataforma si es necesaria
}

uint64_t platform_get_time_ms(void) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

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
