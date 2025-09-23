#include "platform.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

int platform_socket_create_udp(void) {
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		fprintf(stderr, "Error creando socket UDP: %s\n", strerror(errno));
		return PLATFORM_ERROR;
	}
	return sock;
}

int platform_socket_bind(int sock, uint16_t port) {
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "Error en bind puerto %u: %s\n", (unsigned)port, strerror(errno));
		return PLATFORM_ERROR;
	}

	return PLATFORM_OK;
}

int platform_socket_set_nonblocking(int sock) {
	int flags = fcntl(sock, F_GETFL, 0);
	if (flags < 0) {
		return PLATFORM_ERROR;
	}

	if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
		fprintf(stderr, "Error configurando non-blocking: %s\n", strerror(errno));
		return PLATFORM_ERROR;
	}

	return PLATFORM_OK;
}

int platform_socket_set_reuseaddr(int sock) {
	int optval = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
		fprintf(stderr, "Error en SO_REUSEADDR: %s\n", strerror(errno));
		return PLATFORM_ERROR;
	}
	return PLATFORM_OK;
}

void platform_socket_close(int sock) {
	if (sock >= 0) {
		close(sock);
	}
}

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

ssize_t platform_socket_sendto(int sock, const void *buffer, size_t len,
                               const struct sockaddr *addr, socklen_t addrlen) {
	ssize_t bytes = sendto(sock, buffer, len, 0, addr, addrlen);
	if (bytes < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return PLATFORM_EAGAIN;
		}
		fprintf(stderr, "Error en sendto: %s\n", strerror(errno));
		return PLATFORM_ERROR;
	}
	return bytes;
}
