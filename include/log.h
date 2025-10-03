#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <sys/socket.h>

#include "coap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN  = 1,
    LOG_LEVEL_INFO  = 2,
    LOG_LEVEL_DEBUG = 3
} LogLevel;

void log_set_level(LogLevel level);
void log_set_stream(FILE *stream);
void log_printf(LogLevel level, const char *fmt, ...);

// Logs auxiliares para CoAP
// - RX: registra requests CoAP válidas (método, path, peer, MID, TKL, payload bytes)
// - TX: registra respuestas exitosas (2.xx) con peer, MID y tamaño de payload
void log_coap_rx(const CoapMessage *msg, const struct sockaddr *peer, socklen_t peer_len);
void log_coap_tx(const CoapMessage *msg, const struct sockaddr *peer, socklen_t peer_len);

#define LOG_ERROR(...) log_printf(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_WARN(...)  log_printf(LOG_LEVEL_WARN,  __VA_ARGS__)
#define LOG_INFO(...)  log_printf(LOG_LEVEL_INFO,  __VA_ARGS__)
#define LOG_DEBUG(...) log_printf(LOG_LEVEL_DEBUG, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // LOG_H