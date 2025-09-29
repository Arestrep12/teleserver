#include "dispatcher.h"
#include "time_source.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint64_t fixed_now(void) {
    return 1234567890ULL;
}

static void test_time_injection(void) {
    // Configurar fuente de tiempo fija
    TimeSource ts = { .now_ms = fixed_now };
    time_source_set(&ts);

    // Construir request GET /time
    CoapMessage req; coap_message_init(&req);
    req.type = COAP_TYPE_CONFIRMABLE;
    req.code = COAP_METHOD_GET;
    (void)coap_message_add_option(&req, COAP_OPTION_URI_PATH, (const uint8_t*)"time", 4);

    CoapMessage resp; coap_message_init(&resp);
    int rc = dispatcher_handle_request(&req, &resp);
    assert(rc == 0);
    assert(resp.code == COAP_RESPONSE_CONTENT);
    assert(resp.payload && resp.payload_length > 0);

    // Verificar que el payload coincide con fixed_now()
    char expected[32];
    snprintf(expected, sizeof(expected), "%llu", (unsigned long long)fixed_now());
    assert(resp.payload_length == strlen(expected));
    assert(memcmp(resp.payload, expected, resp.payload_length) == 0);

    printf("✓ test_time_injection (core)\n");

    // Restaurar fuente por defecto
    time_source_set(NULL);
}

int main(void) {
    printf("=== Tests de time source (core) ===\n");
    test_time_injection();
    printf("✓ Todos los tests de time source pasaron\n");
    return 0;
}
