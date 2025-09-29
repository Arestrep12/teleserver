#include "dispatcher.h"
#include "handlers.h"
#include "coap_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void build_request(CoapMessage *req, CoapType type, CoapCode method,
                          const char *uri_path, const uint8_t *payload, size_t payload_len) {
    coap_message_init(req);
    req->type = type;
    req->code = method;
    req->message_id = 0x0102;
    req->token_length = 1;
    req->token[0] = 0x77;

    // Set Uri-Path options split by '/'
    char temp[128];
    size_t n = 0;
    while (uri_path && uri_path[n] == '/') n++; // strip leading '/'
    strncpy(temp, uri_path ? uri_path + n : "", sizeof temp - 1);
    temp[sizeof temp - 1] = '\0';

    char *seg = temp;
    while (seg && *seg) {
        char *slash = strchr(seg, '/');
        size_t len = slash ? (size_t)(slash - seg) : strlen(seg);
        if (len > 0) {
            coap_message_add_option(req, COAP_OPTION_URI_PATH, (const uint8_t *)seg, len);
        }
        if (!slash) break;
        seg = slash + 1;
    }

    if (payload && payload_len > 0) {
        if (payload_len <= sizeof(req->payload_buffer)) {
            memcpy(req->payload_buffer, payload, payload_len);
            req->payload = req->payload_buffer;
            req->payload_length = payload_len;
        }
    }
}

static void assert_token_and_id_mirrored(const CoapMessage *req, const CoapMessage *resp) {
    assert(req->message_id == resp->message_id);
    assert(req->token_length == resp->token_length);
    assert(memcmp(req->token, resp->token, req->token_length) == 0);
}

static void test_get_hello(void) {
    CoapMessage req, resp;
    build_request(&req, COAP_TYPE_CONFIRMABLE, COAP_METHOD_GET, "/hello", NULL, 0);

    int rc = dispatcher_handle_request(&req, &resp);
    assert(rc == 0);
    assert_token_and_id_mirrored(&req, &resp);
    assert(resp.type == COAP_TYPE_ACKNOWLEDGMENT);
    assert(resp.code == COAP_RESPONSE_CONTENT);
    assert(resp.payload && resp.payload_length == 5);
    assert(memcmp(resp.payload, "hello", 5) == 0);
    printf("✓ test_get_hello\n");
}

static int is_digits_only(const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; i++) if (p[i] < '0' || p[i] > '9') return 0;
    return 1;
}

static void test_get_time(void) {
    CoapMessage req, resp;
    build_request(&req, COAP_TYPE_NON_CONFIRMABLE, COAP_METHOD_GET, "/time", NULL, 0);

    int rc = dispatcher_handle_request(&req, &resp);
    assert(rc == 0);
    assert_token_and_id_mirrored(&req, &resp);
    assert(resp.type == COAP_TYPE_NON_CONFIRMABLE);
    assert(resp.code == COAP_RESPONSE_CONTENT);
    assert(resp.payload && resp.payload_length > 0);
    assert(is_digits_only(resp.payload, resp.payload_length));
    printf("✓ test_get_time\n");
}

static void test_post_echo(void) {
    CoapMessage req, resp;
    const char *msg = "abc";
    build_request(&req, COAP_TYPE_CONFIRMABLE, COAP_METHOD_POST, "/echo",
                  (const uint8_t *)msg, 3);

    int rc = dispatcher_handle_request(&req, &resp);
    assert(rc == 0);
    assert_token_and_id_mirrored(&req, &resp);
    assert(resp.type == COAP_TYPE_ACKNOWLEDGMENT);
    assert(resp.code == COAP_RESPONSE_CONTENT);
    assert(resp.payload && resp.payload_length == 3);
    assert(memcmp(resp.payload, msg, 3) == 0);
    printf("✓ test_post_echo\n");
}

static void test_not_found(void) {
    CoapMessage req, resp;
    build_request(&req, COAP_TYPE_CONFIRMABLE, COAP_METHOD_GET, "/nope", NULL, 0);

    int rc = dispatcher_handle_request(&req, &resp);
    assert(rc == 0);
    assert(resp.code == COAP_ERROR_NOT_FOUND);
    printf("✓ test_not_found\n");
}

static void test_method_not_allowed(void) {
    CoapMessage req, resp;
    const char *msg = "abc";
    build_request(&req, COAP_TYPE_CONFIRMABLE, COAP_METHOD_POST, "/hello",
                  (const uint8_t *)msg, 3);

    int rc = dispatcher_handle_request(&req, &resp);
    assert(rc == 0);
    assert(resp.code == COAP_ERROR_METHOD_NOT_ALLOWED);
    printf("✓ test_method_not_allowed\n");
}

int main(void) {
    printf("=== Tests de dispatcher ===\n");

    test_get_hello();
    test_get_time();
    test_post_echo();
    test_not_found();
    test_method_not_allowed();

    printf("✓ Todos los tests de dispatcher pasaron\n");
    return 0;
}