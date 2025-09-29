#include "handlers.h"
#include <stdio.h>
#include <string.h>

static int set_content_format_text(CoapMessage *resp) {
    // Content-Format: text/plain (0) => entero 0 codificado como longitud 0
    // (RFC7252: representación de 0 es cadena de longitud 0)
    return coap_message_add_option(resp, COAP_OPTION_CONTENT_FORMAT, NULL, 0);
}

int handle_hello(const CoapMessage *req, CoapMessage *resp) {
    (void)req;
    if (!resp) return -1;

    const char *msg = "hello";
    size_t len = strlen(msg);
    if (len > sizeof(resp->payload_buffer)) return -1;
    memcpy(resp->payload_buffer, msg, len);
    resp->payload = resp->payload_buffer;
    resp->payload_length = len;
    (void)set_content_format_text(resp);
    resp->code = COAP_RESPONSE_CONTENT; // 2.05
    return 0;
}

int handle_time(const CoapMessage *req, CoapMessage *resp) {
    (void)req;
    if (!resp) return -1;

    // Tiempo en milisegundos desde epoch
    uint64_t now = platform_get_time_ms();
    int n = snprintf((char *)resp->payload_buffer, sizeof(resp->payload_buffer), "%llu",
                     (unsigned long long)now);
    if (n < 0) return -1;
    resp->payload = resp->payload_buffer;
    resp->payload_length = (size_t)n;
    (void)set_content_format_text(resp);
    resp->code = COAP_RESPONSE_CONTENT;
    return 0;
}

int handle_echo(const CoapMessage *req, CoapMessage *resp) {
    if (!req || !resp) return -1;

    if (req->payload && req->payload_length > 0) {
        if (req->payload_length > sizeof(resp->payload_buffer)) return -1;
        memcpy(resp->payload_buffer, req->payload, req->payload_length);
        resp->payload = resp->payload_buffer;
        resp->payload_length = req->payload_length;
    } else {
        // Sin payload, devolvemos vacío
        resp->payload = NULL;
        resp->payload_length = 0;
    }
    (void)set_content_format_text(resp);
    resp->code = COAP_RESPONSE_CONTENT;
    return 0;
}