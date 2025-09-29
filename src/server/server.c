#include "server.h"
#include "platform.h"
#include "event_loop.h"
#include "coap_codec.h"
#include "dispatcher.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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

static void on_readable(int fd, EventType events, void *user_data) {
    (void)events;
    Server *srv = (Server *)user_data;
    if (!srv || fd != srv->sock) return;

    uint8_t buf[RECV_BUFFER_SIZE];
    struct sockaddr_storage peer;
    socklen_t peer_len = (socklen_t)sizeof(peer);

    for (;;) {
        ssize_t n = platform_socket_recvfrom(srv->sock, buf, sizeof(buf),
                                             (struct sockaddr *)&peer,
                                             &peer_len);
        if (n == PLATFORM_EAGAIN) break;
        if (n <= 0) break;

        CoapMessage req; coap_message_init(&req);
        int rc = coap_decode(&req, buf, (size_t)n);
        if (rc != 0) {
            if (srv->verbose) fprintf(stderr, "coap_decode error %d\n", rc);
            continue;
        }

        CoapMessage resp; coap_message_init(&resp);
        rc = dispatcher_handle_request(&req, &resp);
        if (rc != 0) {
            if (srv->verbose) fprintf(stderr, "dispatcher error %d\n", rc);
            continue;
        }

        uint8_t out[SEND_BUFFER_SIZE];
        int out_n = coap_encode(&resp, out, sizeof(out));
        if (out_n <= 0) {
            if (srv->verbose) fprintf(stderr, "coap_encode error %d\n", out_n);
            continue;
        }

        ssize_t sent = platform_socket_sendto(srv->sock, out, (size_t)out_n,
                                              (struct sockaddr *)&peer,
                                              peer_len);
        (void)sent; // en UDP, si falla, simplemente seguimos
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
        fprintf(stderr, "Server listening UDP/%u\n", (unsigned)srv->port);
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