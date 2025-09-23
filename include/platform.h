#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>

// Detección de plataforma
#if defined(__APPLE__) && defined(__MACH__)
    #define PLATFORM_MACOS 1
    #define PLATFORM_NAME "macOS"
#elif defined(__linux__)
    #define PLATFORM_LINUX 1
    #define PLATFORM_NAME "Linux"
#else
    #error "Plataforma no soportada"
#endif

// Configuración de red
#define DEFAULT_PORT 5683
#define MAX_PACKET_SIZE 1472  // MTU típico menos headers IP/UDP
#define SOCKET_BACKLOG 128

// Códigos de error
typedef enum {
    PLATFORM_OK = 0,
    PLATFORM_ERROR = -1,
    PLATFORM_EAGAIN = -2,
    PLATFORM_ENOMEM = -3,
    PLATFORM_EINVAL = -4
} PlatformError;

// Funciones de socket
int platform_socket_create_udp(void);
int platform_socket_bind(int sock, uint16_t port);
int platform_socket_set_nonblocking(int sock);
int platform_socket_set_reuseaddr(int sock);
void platform_socket_close(int sock);

// I/O de red
ssize_t platform_socket_recvfrom(int sock, void *buffer, size_t len,
                                 struct sockaddr *addr, socklen_t *addrlen);
ssize_t platform_socket_sendto(int sock, const void *buffer, size_t len,
                               const struct sockaddr *addr, socklen_t addrlen);

// Utilidades
void platform_init(void);
void platform_cleanup(void);
uint64_t platform_get_time_ms(void);
const char *platform_error_string(int error);

#endif // PLATFORM_H
