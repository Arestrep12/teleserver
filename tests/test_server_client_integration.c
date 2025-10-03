#include "server.h"
#include "platform.h"

// Cliente CoAP real desde ../TeleClient
#include "client.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static Server *g_srv = NULL;
static volatile int g_server_running = 0;

static void *server_thread_fn(void *arg) {
    (void)arg;
    g_server_running = 1;
    // Corre hasta que alguien invoque server_stop()
    server_run(g_srv, -1);
    g_server_running = 0;
    return NULL;
}

static CoapClientConfig make_cfg(uint16_t port) {
    CoapClientConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = port;
    cfg.timeout_ms = 300;
    cfg.retries = 1;
    cfg.non = false;
    cfg.verbose = false;
    return cfg;
}

static void set_id_and_token(CoapMessage *req, uint16_t mid, uint8_t tok, uint8_t tkl) {
    // Helper para asegurar que client y server hagan match
    req->message_id = mid;
    req->token_length = tkl;
    if (tkl > 0) req->token[0] = tok;
}

static void test_get_hello_con(uint16_t port) {
    CoapClientConfig cfg = make_cfg(port);
    CoapMessage req, resp;
    assert(coap_build_get(&req, "/hello", false) == 0);
    set_id_and_token(&req, 0x1111, 0xA1, 1);
    int rc = coap_client_send(&cfg, &req, &resp);
    assert(rc == 0);
    assert(resp.code == COAP_RESPONSE_CONTENT);
    assert(resp.type == COAP_TYPE_ACKNOWLEDGMENT);
    assert(resp.payload && resp.payload_length == 5);
    assert(memcmp(resp.payload, "hello", 5) == 0);
    printf("✓ client GET /hello (CON)\n");
}

static void test_get_hello_non(uint16_t port) {
    CoapClientConfig cfg = make_cfg(port);
    CoapMessage req, resp;
    assert(coap_build_get(&req, "/hello", true) == 0);
    set_id_and_token(&req, 0x2222, 0xB2, 1);
    int rc = coap_client_send(&cfg, &req, &resp);
    assert(rc == 0);
    assert(resp.code == COAP_RESPONSE_CONTENT);
    // Para NON la respuesta también debe ser NON (según dispatcher)
    assert(resp.type == COAP_TYPE_NON_CONFIRMABLE);
    assert(resp.payload && resp.payload_length == 5);
    assert(memcmp(resp.payload, "hello", 5) == 0);
    printf("✓ client GET /hello (NON)\n");
}

static void test_post_echo_con(uint16_t port) {
    CoapClientConfig cfg = make_cfg(port);
    CoapMessage req, resp;
    const char *msg = "abc";
    assert(coap_build_post(&req, "/echo", (const uint8_t *)msg, 3, false) == 0);
    set_id_and_token(&req, 0x3333, 0xC3, 1);
    int rc = coap_client_send(&cfg, &req, &resp);
    assert(rc == 0);
    assert(resp.code == COAP_RESPONSE_CONTENT);
    assert(resp.payload && resp.payload_length == 3);
    assert(memcmp(resp.payload, msg, 3) == 0);
    printf("✓ client POST /echo (CON)\n");
}

static void test_post_echo_large(uint16_t port) {
    CoapClientConfig cfg = make_cfg(port);
    CoapMessage req, resp;
    uint8_t buf[128];
    for (size_t i = 0; i < sizeof(buf); i++) buf[i] = (uint8_t)('a' + (i % 26));
    assert(coap_build_post(&req, "/echo", buf, sizeof(buf), false) == 0);
    set_id_and_token(&req, 0x4444, 0xD4, 1);
    int rc = coap_client_send(&cfg, &req, &resp);
    assert(rc == 0);
    assert(resp.code == COAP_RESPONSE_CONTENT);
    assert(resp.payload && resp.payload_length == sizeof(buf));
    assert(memcmp(resp.payload, buf, sizeof(buf)) == 0);
    printf("✓ client POST /echo payload grande\n");
}

static void test_get_time(uint16_t port) {
    CoapClientConfig cfg = make_cfg(port);
    CoapMessage req, resp;
    assert(coap_build_get(&req, "/time", false) == 0);
    set_id_and_token(&req, 0x5555, 0xE5, 1);
    int rc = coap_client_send(&cfg, &req, &resp);
    assert(rc == 0);
    assert(resp.code == COAP_RESPONSE_CONTENT);
    // Debe ser un string numérico (millis)
    assert(resp.payload && resp.payload_length > 0);
    for (size_t i = 0; i < resp.payload_length; i++) {
        assert(((char *)resp.payload)[i] >= '0' && ((char *)resp.payload)[i] <= '9');
    }
    printf("✓ client GET /time\n");
}

static void test_not_found(uint16_t port) {
    CoapClientConfig cfg = make_cfg(port);
    CoapMessage req, resp;
    assert(coap_build_get(&req, "/nope", false) == 0);
    set_id_and_token(&req, 0x6666, 0xF6, 1);
    int rc = coap_client_send(&cfg, &req, &resp);
    assert(rc == 0);
    assert(resp.code == COAP_ERROR_NOT_FOUND);
    printf("✓ client 404 /nope\n");
}

static void test_method_not_allowed(uint16_t port) {
    CoapClientConfig cfg = make_cfg(port);
    CoapMessage req, resp;
    // /echo acepta solo POST
    assert(coap_build_get(&req, "/echo", false) == 0);
    set_id_and_token(&req, 0x7777, 0xA7, 1);
    int rc = coap_client_send(&cfg, &req, &resp);
    assert(rc == 0);
    assert(resp.code == COAP_ERROR_METHOD_NOT_ALLOWED);
    printf("✓ client 405 GET /echo\n");
}

int main(void) {
    printf("=== Tests integración con TeleClient ===\n");
    platform_init();

    g_srv = server_create(0, true);
    assert(g_srv != NULL);
    uint16_t port = server_get_port(g_srv);
    assert(port != 0);

    pthread_t thread;
    int prc = pthread_create(&thread, NULL, server_thread_fn, NULL);
    assert(prc == 0);
    // Pequeño delay para asegurar que el server está corriendo
    usleep(50 * 1000);

    test_get_hello_con(port);
    test_get_hello_non(port);
    test_post_echo_con(port);
    test_post_echo_large(port);
    test_get_time(port);
    test_not_found(port);
    test_method_not_allowed(port);

    server_stop(g_srv);
    pthread_join(thread, NULL);
    server_destroy(g_srv);
    g_srv = NULL;
    platform_cleanup();
    printf("✓ Todos los tests con TeleClient pasaron\n");
    return 0;
}
