/*
 * server.c — Integración del bucle de eventos, socket UDP y dispatcher CoAP.
 *
 * Responsabilidades
 * - Crear y gestionar un socket UDP no bloqueante.
 * - Registrar el socket en el EventLoop y procesar datagramas recibidos.
 * - Decodificar mensajes CoAP, enrutar la petición y codificar la respuesta.
 * - Evitar amplificación: datagramas inválidos se descartan silenciosamente.
 *
 * Concurrencia
 * - Diseño single-threaded, orientado a eventos. No se usan hilos internos.
 *
 * Errores y logging
 * - En modo --verbose, se registran RX/TX de CoAP y advertencias de codec/dispatcher.
 * - Las funciones retornan códigos PLATFORM_* cuando aplica; en fallos de
 *   decodificación/codificación se omite la respuesta para evitar comportamientos
 *   inesperados.
 */
#include "server.h"
#include "platform.h"
#include "event_loop.h"
#include "coap_codec.h"
#include "dispatcher.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define RECV_BUFFER_SIZE COAP_MAX_MESSAGE_SIZE
#define SEND_BUFFER_SIZE COAP_MAX_MESSAGE_SIZE

struct Server {
    EventLoop *loop;
    int sock;
    bool verbose;
    uint16_t port;
};

/*
 * process_datagram
 * -----------------
 * Decodifica un datagrama UDP como CoAP, lo enruta via dispatcher y envía la
 * respuesta resultante al mismo peer. Si la decodificación falla, el datagrama
 * se descarta sin respuesta para evitar amplificación.
 *
 * Parámetros
 * - srv: instancia del servidor (propietaria del socket y flags de verbose).
 * - buf/n: bytes del datagrama recibido.
 * - peer/peer_len: dirección del remitente (IPv4/IPv6).
 *
 * Comportamiento
 * - Loggea RX/TX en modo verbose.
 * - Construye una respuesta 4.00 si el dispatcher retorna error lógico.
 */
static void process_datagram(Server *srv,
                             const uint8_t *buf, size_t n,
                             const struct sockaddr *peer, socklen_t peer_len) {
    CoapMessage req; coap_message_init(&req);
    int rc = coap_decode(&req, buf, n);
    if (rc != 0) {
        if (srv->verbose) LOG_WARN("coap_decode error %d\n", rc);
        return;
    }

    // Log de entrada (request CoAP válido)
    if (srv->verbose) {
        log_coap_rx(&req, peer, peer_len);
    }

    CoapMessage resp; coap_message_init(&resp);
    rc = dispatcher_handle_request(&req, &resp);
    if (rc != 0) {
        if (srv->verbose) LOG_WARN("dispatcher error %d, sending 4.00 Bad Request\n", rc);
        // Construir respuesta de error mínima
        coap_message_init(&resp);
        resp.version = COAP_VERSION;
        resp.message_id = req.message_id;
        resp.token_length = req.token_length;
        if (req.token_length > 0) {
            memcpy(resp.token, req.token, req.token_length);
        }
        if (req.type == COAP_TYPE_CONFIRMABLE) {
            resp.type = COAP_TYPE_ACKNOWLEDGMENT;
        } else {
            resp.type = COAP_TYPE_NON_CONFIRMABLE;
        }
        resp.code = COAP_ERROR_BAD_REQUEST;
    }

    uint8_t out[SEND_BUFFER_SIZE];
    int out_n = coap_encode(&resp, out, sizeof(out));
    if (out_n <= 0) {
        if (srv->verbose) LOG_WARN("coap_encode error %d\n", out_n);
        return;
    }

    // Log de salida (solo 2.xx)
    if (srv->verbose) {
        log_coap_tx(&resp, peer, peer_len);
    }

    (void)platform_socket_sendto(srv->sock, out, (size_t)out_n, peer, peer_len);
}

/*
 * on_readable
 * -----------
 * Callback registrado en el EventLoop para el socket UDP del servidor. Extrae
 * todos los datagramas pendientes y delega su procesamiento a process_datagram.
 *
 * Notas
 * - Usa un bucle hasta que recvfrom retorna EAGAIN (no más datos).
 */
static void on_readable(int fd, EventType events, void *user_data) {
    (void)events;
    Server *srv = (Server *)user_data;
    if (!srv || fd != srv->sock) return;

    uint8_t buf[RECV_BUFFER_SIZE];
    struct sockaddr_storage peer;

    for (;;) {
        socklen_t peer_len = (socklen_t)sizeof(peer);
        ssize_t n = platform_socket_recvfrom(srv->sock, buf, sizeof(buf),
                                             (struct sockaddr *)&peer,
                                             &peer_len);
        if (n == PLATFORM_EAGAIN) break;
        if (n <= 0) break;
        process_datagram(srv, buf, (size_t)n, (const struct sockaddr *)&peer, peer_len);
    }
}

/*
 * query_bound_port
 * -----------------
 * Obtiene el puerto efectivo al que fue enlazado el socket (útil cuando se
 * solicita el puerto 0 para que el SO asigne uno efímero).
 */
static uint16_t query_bound_port(int sock) {
    struct sockaddr_in addr;
    socklen_t len = (socklen_t)sizeof(addr);
    memset(&addr, 0, sizeof(addr));
    if (getsockname(sock, (struct sockaddr *)&addr, &len) == 0) {
        return ntohs(addr.sin_port);
    }
    return 0;
}

/*
 * server_create
 * -------------
 * Crea una instancia de Server, inicializa el EventLoop, configura el socket UDP
 * (SO_REUSEADDR, O_NONBLOCK) y lo enlaza al puerto indicado. Registra el socket
 * en el loop para eventos de lectura.
 *
 * Parámetros
 * - port: puerto UDP (0 permite puerto efímero asignado por el SO).
 * - verbose: habilita logs informativos y de CoAP RX/TX.
 *
 * Retorno
 * - Puntero Server válido en éxito; NULL en error.
 */
Server *server_create(uint16_t port, bool verbose) {
    Server *srv = (Server *)calloc(1, sizeof(Server));
    if (!srv) return NULL;
    srv->verbose = verbose;

    srv->loop = event_loop_create();
    if (!srv->loop) { free(srv); return NULL; }

    int sock = platform_socket_create_udp();
    if (sock < 0) { event_loop_destroy(srv->loop); free(srv); return NULL; }

    if (platform_socket_set_reuseaddr(sock) != PLATFORM_OK) {
        platform_socket_close(sock); event_loop_destroy(srv->loop); free(srv); return NULL;
    }
    if (platform_socket_set_nonblocking(sock) != PLATFORM_OK) {
        platform_socket_close(sock); event_loop_destroy(srv->loop); free(srv); return NULL;
    }
    if (platform_socket_bind(sock, port) != PLATFORM_OK) {
        platform_socket_close(sock); event_loop_destroy(srv->loop); free(srv); return NULL;
    }

    srv->sock = sock;
    srv->port = query_bound_port(sock);

    int rc = event_loop_add_fd(srv->loop, srv->sock, EVENT_READ, on_readable, srv);
    if (rc != PLATFORM_OK) {
        platform_socket_close(sock);
        event_loop_destroy(srv->loop);
        free(srv);
        return NULL;
    }

    if (srv->verbose) {
        LOG_INFO("Server listening UDP/%u\n", (unsigned)srv->port);
    }

    return srv;
}

/*
 * server_destroy
 * --------------
 * Libera recursos asociados al servidor: desregistra el FD del loop, cierra el
 * socket y destruye el EventLoop.
 */
void server_destroy(Server *srv) {
    if (!srv) return;
    if (srv->loop && srv->sock >= 0) {
        event_loop_remove_fd(srv->loop, srv->sock);
    }
    if (srv->sock >= 0) platform_socket_close(srv->sock);
    if (srv->loop) event_loop_destroy(srv->loop);
    free(srv);
}

/*
 * server_run
 * ----------
 * Ejecuta el bucle de eventos.
 * - run_timeout_ms < 0: corre indefinidamente hasta server_stop().
 * - run_timeout_ms >= 0: procesa una iteración con ese timeout y retorna.
 *
 * Retorna
 * - PLATFORM_OK en éxito; PLATFORM_* en error.
 */
int server_run(Server *srv, int run_timeout_ms) {
    if (!srv) return PLATFORM_EINVAL;
    return event_loop_run(srv->loop, run_timeout_ms);
}

/*
 * server_stop
 * -----------
 * Señala al EventLoop para detener su ejecución en la próxima oportunidad.
 */
void server_stop(Server *srv) {
    if (!srv) return;
    event_loop_stop(srv->loop);
}

/*
 * server_get_port
 * ---------------
 * Devuelve el puerto efectivo al que está enlazado el socket del servidor.
 */
uint16_t server_get_port(const Server *srv) {
    return srv ? srv->port : 0;
}