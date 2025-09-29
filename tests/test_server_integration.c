#include "server.h"
#include "coap_codec.h"
#include "coap.h"
#include "platform.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int udp_client_socket(void) {
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    assert(s >= 0);
    set_nonblock(s);
    return s;
}

static void build_get(CoapMessage *msg, const char *path, CoapType type) {
    coap_message_init(msg);
    msg->type = type;
    msg->code = COAP_METHOD_GET;
    msg->message_id = 0x2222;
    msg->token_length = 1;
    msg->token[0] = 0x42;

    // Uri-Path split
    const char *p = path;
    while (*p == '/') p++;
    const char *start = p;
    for (;;) {
        const char *slash = strchr(start, '/');
        size_t len = slash ? (size_t)(slash - start) : strlen(start);
        if (len > 0) {
            coap_message_add_option(msg, COAP_OPTION_URI_PATH,
                                    (const uint8_t *)start, len);
        }
        if (!slash) break;
        start = slash + 1;
    }
}

static void build_post_echo(CoapMessage *msg, const uint8_t *payload, size_t n) {
    coap_message_init(msg);
    msg->type = COAP_TYPE_CONFIRMABLE;
    msg->code = COAP_METHOD_POST;
    msg->message_id = 0x3333;
    msg->token_length = 1;
    msg->token[0] = 0x24;
    coap_message_add_option(msg, COAP_OPTION_URI_PATH, (const uint8_t *)"echo", 4);
    if (payload && n > 0) {
        memcpy(msg->payload_buffer, payload, n);
        msg->payload = msg->payload_buffer;
        msg->payload_length = n;
    }
}

static ssize_t run_and_recv(Server *srv, int client,
                            uint8_t *in, size_t in_sz,
                            struct sockaddr_in *src, socklen_t *slen) {
    for (int i = 0; i < 20; i++) {
        server_run(srv, 20);
        ssize_t r = recvfrom(client, in, in_sz, 0,
                             (struct sockaddr *)src, slen);
        if (r > 0) return r;
    }
    return -1;
}

static void test_get_hello(Server *srv, int client) {
    uint8_t out[COAP_MAX_MESSAGE_SIZE];
    CoapMessage req; build_get(&req, "/hello", COAP_TYPE_CONFIRMABLE);
    int n = coap_encode(&req, out, sizeof(out));
    assert(n > 0);

    struct sockaddr_in dst; memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons(server_get_port(srv));
    dst.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    ssize_t sent = sendto(client, out, (size_t)n, 0,
                          (struct sockaddr *)&dst, sizeof(dst));
    assert(sent == n);

    uint8_t in[COAP_MAX_MESSAGE_SIZE];
    struct sockaddr_in src; socklen_t slen = sizeof(src); memset(&src, 0, sizeof(src));
    ssize_t r = run_and_recv(srv, client, in, sizeof(in), &src, &slen);
    assert(r > 0);

    CoapMessage resp; coap_message_init(&resp);
    assert(coap_decode(&resp, in, (size_t)r) == 0);
    assert(resp.code == COAP_RESPONSE_CONTENT);
    assert(resp.type == COAP_TYPE_ACKNOWLEDGMENT);
    assert(resp.payload && resp.payload_length == 5);
    assert(memcmp(resp.payload, "hello", 5) == 0);
    printf("✓ server GET /hello\n");
}

static void test_post_echo(Server *srv, int client) {
    const char *msg = "abc";
    uint8_t out[COAP_MAX_MESSAGE_SIZE];
    CoapMessage req; build_post_echo(&req, (const uint8_t *)msg, 3);
    int n = coap_encode(&req, out, sizeof(out));
    assert(n > 0);

    struct sockaddr_in dst; memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons(server_get_port(srv));
    dst.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    ssize_t sent = sendto(client, out, (size_t)n, 0,
                          (struct sockaddr *)&dst, sizeof(dst));
    assert(sent == n);

    uint8_t in[COAP_MAX_MESSAGE_SIZE];
    struct sockaddr_in src; socklen_t slen = sizeof(src); memset(&src, 0, sizeof(src));
    ssize_t r = run_and_recv(srv, client, in, sizeof(in), &src, &slen);
    assert(r > 0);

    CoapMessage resp; coap_message_init(&resp);
    assert(coap_decode(&resp, in, (size_t)r) == 0);
    assert(resp.code == COAP_RESPONSE_CONTENT);
    assert(resp.payload && resp.payload_length == 3);
    assert(memcmp(resp.payload, msg, 3) == 0);
    printf("✓ server POST /echo\n");
}

int main(void) {
    printf("=== Tests de integración del servidor ===\n");
    platform_init();

    Server *srv = server_create(0, true);
    assert(srv != NULL);

    int client = udp_client_socket();

    test_get_hello(srv, client);
    test_post_echo(srv, client);

    close(client);
    server_destroy(srv);
    platform_cleanup();
    printf("✓ Todos los tests de server integración pasaron\n");
    return 0;
}