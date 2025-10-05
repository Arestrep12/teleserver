/*
 * socket.c — Wrappers de sockets UDP (POSIX) con manejo de errores uniforme.
 *
 * Provee funciones para crear, configurar y operar sockets UDP de manera
 * portátil entre macOS y Linux, devolviendo códigos PLATFORM_*.
 */
#include "platform.h"
#include "log.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

/*
 * platform_socket_create_udp
 * --------------------------
 * Crea un socket UDP IPv4.
 *
 * Retorna
 * - Descriptor de socket >= 0 en éxito; PLATFORM_ERROR en fallo.
 */
int platform_socket_create_udp(void) {
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
LOG_ERROR("Error creando socket UDP: %s\n", strerror(errno));
		return PLATFORM_ERROR;
	}
	return sock;
}

/*
 * platform_socket_bind
 * --------------------
 * Enlaza el socket a INADDR_ANY:port. Úsese con port=0 para puerto efímero.
 */
int platform_socket_bind(int sock, uint16_t port) {
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
LOG_ERROR("Error en bind puerto %u: %s\n", (unsigned)port, strerror(errno));
		return PLATFORM_ERROR;
	}

	return PLATFORM_OK;
}

/*
 * platform_socket_set_nonblocking
 * -------------------------------
 * Activa O_NONBLOCK en el descriptor.
 */
int platform_socket_set_nonblocking(int sock) {
	int flags = fcntl(sock, F_GETFL, 0);
	if (flags < 0) {
		return PLATFORM_ERROR;
	}

	if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
LOG_ERROR("Error configurando non-blocking: %s\n", strerror(errno));
		return PLATFORM_ERROR;
	}

	return PLATFORM_OK;
}

/*
 * platform_socket_set_reuseaddr
 * -----------------------------
 * Habilita SO_REUSEADDR para permitir reenlaces rápidos en desarrollo.
 */
int platform_socket_set_reuseaddr(int sock) {
	int optval = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
LOG_ERROR("Error en SO_REUSEADDR: %s\n", strerror(errno));
		return PLATFORM_ERROR;
	}
	return PLATFORM_OK;
}

/*
 * platform_socket_close
 * ---------------------
 * Cierra el descriptor si es válido.
 */
void platform_socket_close(int sock) {
	if (sock >= 0) {
		close(sock);
	}
}

/*
 * platform_socket_recvfrom
 * ------------------------
 * Recibe un datagrama UDP. Retorna PLATFORM_EAGAIN si no hay datos (modo
 * no bloqueante), PLATFORM_ERROR en error, o cantidad de bytes en éxito.
 */
ssize_t platform_socket_recvfrom(int sock, void *buffer, size_t len,
                                 struct sockaddr *addr, socklen_t *addrlen) {
	ssize_t bytes = recvfrom(sock, buffer, len, 0, addr, addrlen);
	if (bytes < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return PLATFORM_EAGAIN;
		}
		return PLATFORM_ERROR;
	}
	return bytes;
}

/*
 * platform_socket_sendto
 * ----------------------
 * Envía un datagrama UDP. Retorna PLATFORM_EAGAIN si el socket no puede enviar
 * momentáneamente (no bloqueante), PLATFORM_ERROR en error, o bytes enviados.
 */
ssize_t platform_socket_sendto(int sock, const void *buffer, size_t len,
                               const struct sockaddr *addr, socklen_t addrlen) {
	ssize_t bytes = sendto(sock, buffer, len, 0, addr, addrlen);
	if (bytes < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return PLATFORM_EAGAIN;
		}
LOG_WARN("Error en sendto: %s\n", strerror(errno));
		return PLATFORM_ERROR;
	}
	return bytes;
}
