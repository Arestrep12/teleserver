/*
 * log.c — Utilidades de logging con niveles y helpers específicos de CoAP.
 *
 * Permite redirigir salida y controlar el nivel. Incluye funciones para
 * formatear endpoints y registrar RX/TX de CoAP con información relevante.
 */
#include "log.h"

#include <time.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "coap.h"

static LogLevel g_level = LOG_LEVEL_INFO; // por defecto: INFO
static FILE *g_stream = NULL;             // NULL => usar stderr

/*
 * log_set_level
 * -------------
 * Establece el nivel mínimo que será impreso por log_printf.
 */
void log_set_level(LogLevel level) {
    g_level = level;
}

/*
 * log_set_stream
 * --------------
 * Redirige la salida de logs a 'stream'. Si es NULL, se usa stderr.
 */
void log_set_stream(FILE *stream) {
    g_stream = stream;
}

/*
 * level_to_str
 * ------------
 * Mapea LogLevel a etiqueta textual.
 */
static const char *level_to_str(LogLevel lvl) {
    switch (lvl) {
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        default: return "LOG";
    }
}

/*
 * log_printf
 * ----------
 * Imprime un mensaje con prefijo de nivel si 'level' es >= g_level. Acepta
 * formato printf y argumentos variables.
 */
void log_printf(LogLevel level, const char *fmt, ...) {
    if (level > g_level) return;
    FILE *out = g_stream ? g_stream : stderr;

    // Prefijo simple con nivel
    fprintf(out, "[%s] ", level_to_str(level));

    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
}

/*
 * format_sockaddr
 * ----------------
 * Convierte una sockaddr (IPv4/IPv6) a la forma "ip:puerto" para logs.
 */
static void format_sockaddr(const struct sockaddr *sa, socklen_t sa_len,
                            char *out, size_t out_size) {
    if (!sa || !out || out_size == 0) return;
    out[0] = '\0';
    if (sa->sa_family == AF_INET && sa_len >= (socklen_t)sizeof(struct sockaddr_in)) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)sa;
        char ip[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &in->sin_addr, ip, sizeof ip)) {
            snprintf(out, out_size, "%s:%u", ip, (unsigned)ntohs(in->sin_port));
        }
    }
#ifdef AF_INET6
    else if (sa->sa_family == AF_INET6 && sa_len >= (socklen_t)sizeof(struct sockaddr_in6)) {
        const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *)sa;
        char ip[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, &in6->sin6_addr, ip, sizeof ip)) {
            snprintf(out, out_size, "[%s]:%u", ip, (unsigned)ntohs(in6->sin6_port));
        }
    }
#endif
    else {
        snprintf(out, out_size, "unknown");
    }
}

/*
 * log_coap_rx
 * -----------
 * Registra un request CoAP recibido: método, ruta, peer, MID, TKL y tamaño de
 * payload. Requiere msg != NULL.
 */
void log_coap_rx(const CoapMessage *msg, const struct sockaddr *peer, socklen_t peer_len) {
    if (!msg) return;
    char peer_str[64]; format_sockaddr(peer, peer_len, peer_str, sizeof(peer_str));
    char path[128];
    int pl = coap_message_get_uri_path(msg, path, sizeof(path));
    if (pl < 0) snprintf(path, sizeof(path), "(invalid)");
    const char *mstr = coap_code_to_string(msg->code);
    log_printf(LOG_LEVEL_INFO, "RX %s %s from %s mid=%u tkl=%u payload=%zuB\n",
               mstr ? mstr : "METHOD", path[0] ? path : "(root)", peer_str,
               (unsigned)msg->message_id, (unsigned)msg->token_length,
               msg->payload_length);
}

/*
 * log_coap_tx
 * -----------
 * Registra una respuesta CoAP enviada (sólo clases 2.xx): código, peer, MID y
 * tamaño de payload. Requiere msg != NULL.
 */
void log_coap_tx(const CoapMessage *msg, const struct sockaddr *peer, socklen_t peer_len) {
    if (!msg) return;
    if (coap_code_class(msg->code) != 2) return; // solo éxitos 2.xx
    char peer_str[64]; format_sockaddr(peer, peer_len, peer_str, sizeof(peer_str));
    const char *rstr = coap_code_to_string(msg->code);
    log_printf(LOG_LEVEL_INFO, "TX %s to %s mid=%u payload=%zuB\n",
               rstr ? rstr : "2.xx", peer_str,
               (unsigned)msg->message_id, msg->payload_length);
}
