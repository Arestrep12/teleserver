#include "coap.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_code_utils(void) {
    CoapCode code = COAP_RESPONSE_CONTENT;
    assert(coap_code_class(code) == 2);
    assert(coap_code_detail(code) == 5);

    code = coap_make_code(4, 4);
    assert(code == COAP_ERROR_NOT_FOUND);
    printf("✓ test_code_utils\n");
}

static void test_message_init(void) {
    CoapMessage msg;
    coap_message_init(&msg);

    assert(msg.version == COAP_VERSION);
    assert(msg.type == COAP_TYPE_CONFIRMABLE);
    assert(msg.option_count == 0);
    assert(msg.payload_length == 0);
    printf("✓ test_message_init\n");
}

static void test_options_and_uri(void) {
    CoapMessage msg;
    coap_message_init(&msg);

    const char *p1 = "sensor";
    const char *p2 = "temp";

    int r = coap_message_add_option(&msg, COAP_OPTION_URI_PATH,
                                    (const uint8_t *)p1, strlen(p1));
    assert(r == 0);
    r = coap_message_add_option(&msg, COAP_OPTION_URI_PATH,
                                (const uint8_t *)p2, strlen(p2));
    assert(r == 0);

    assert(msg.option_count == 2);

    char path[64];
    int len = coap_message_get_uri_path(&msg, path, sizeof(path));
    assert(len > 0);
    assert(strcmp(path, "sensor/temp") == 0);
    printf("✓ test_options_and_uri\n");
}

static void test_validation_flags(void) {
    CoapMessage msg;
    coap_message_init(&msg);

    assert(coap_message_is_valid(&msg));

    msg.code = COAP_METHOD_GET;
    assert(coap_message_is_request(&msg));
    assert(!coap_message_is_response(&msg));

    msg.code = COAP_RESPONSE_CONTENT;
    assert(!coap_message_is_request(&msg));
    assert(coap_message_is_response(&msg));
    printf("✓ test_validation_flags\n");
}

int main(void) {
    printf("=== Tests de tipos CoAP ===\n");

    test_code_utils();
    test_message_init();
    test_options_and_uri();
    test_validation_flags();

    printf("✓ Todos los tests de tipos CoAP pasaron\n");
    return 0;
}
