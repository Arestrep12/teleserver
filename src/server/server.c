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

static void process_datagram(Server *srv,
                             const uint8_t *buf, size_t n,
                             const struct sockaddr *peer, socklen_t peer_len) {
    CoapMessage req; coap_message_init(&req);
    int rc = coap_decode(&req, buf, n);
    if (rc != 0) {
        if (srv->verbose) LOG_WARN("coap_decode error %d\n", rc);
        return;
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

    (void)platform_socket_sendto(srv->sock, out, (size_t)out_n, peer, peer_len);
}

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

static uint16_t query_bound_port(int sock) {
    struct sockaddr_in addr;
    socklen_t len = (socklen_t)sizeof(addr);
    memset(&addr, 0, sizeof(addr));
    if (getsockname(sock, (struct sockaddr *)&addr, &len) == 0) {
        return ntohs(addr.sin_port);
    }
    return 0;
}

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

void server_destroy(Server *srv) {
    if (!srv) return;
    if (srv->loop && srv->sock >= 0) {
        event_loop_remove_fd(srv->loop, srv->sock);
    }
    if (srv->sock >= 0) platform_socket_close(srv->sock);
    if (srv->loop) event_loop_destroy(srv->loop);
    free(srv);
}

int server_run(Server *srv, int run_timeout_ms) {
    if (!srv) return PLATFORM_EINVAL;
    return event_loop_run(srv->loop, run_timeout_ms);
}

void server_stop(Server *srv) {
    if (!srv) return;
    event_loop_stop(srv->loop);
}

uint16_t server_get_port(const Server *srv) {
    return srv ? srv->port : 0;
}