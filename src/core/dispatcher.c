#include "dispatcher.h"
#include "handlers.h"
#include <string.h>

static void init_response_from_request(const CoapMessage *req, CoapMessage *resp) {
    coap_message_init(resp);
    // Mirror token y message_id; versión constante
    resp->version = COAP_VERSION;
    resp->message_id = req->message_id;
    resp->token_length = req->token_length;
    if (req->token_length > 0) {
        memcpy(resp->token, req->token, req->token_length);
    }
    // Tipo de respuesta piggybacked o NON
    if (req->type == COAP_TYPE_CONFIRMABLE) {
        resp->type = COAP_TYPE_ACKNOWLEDGMENT;
    } else if (req->type == COAP_TYPE_NON_CONFIRMABLE) {
        resp->type = COAP_TYPE_NON_CONFIRMABLE;
    } else {
        // Para otros tipos, mantenemos ACK por defecto
        resp->type = COAP_TYPE_ACKNOWLEDGMENT;
    }
}

static int method_from_code(CoapCode code) {
    // Devuelve 1=GET,2=POST,3=PUT,4=DELETE o 0 si no es método
    if (coap_code_class(code) != 0) return 0;
    return (int)code;
}

int dispatcher_handle_request(const CoapMessage *req, CoapMessage *resp) {
    if (!req || !resp) return -1;
    if (!coap_message_is_valid(req)) return -1;
    if (!coap_message_is_request(req)) return -1;

    init_response_from_request(req, resp);

    // Obtener ruta
    char path[128];
    int path_len = coap_message_get_uri_path(req, path, sizeof(path));
    if (path_len < 0) {
        resp->code = COAP_ERROR_BAD_REQUEST;
        return 0;
    }

    int method = method_from_code(req->code);
    if (method == 0) {
        resp->code = COAP_ERROR_BAD_REQUEST;
        return 0;
    }

    // Routing simple
    if (strcmp(path, "hello") == 0) {
        if (method != COAP_METHOD_GET) {
            resp->code = COAP_ERROR_METHOD_NOT_ALLOWED;
            return 0;
        }
        return handle_hello(req, resp);
    } else if (strcmp(path, "time") == 0) {
        if (method != COAP_METHOD_GET) {
            resp->code = COAP_ERROR_METHOD_NOT_ALLOWED;
            return 0;
        }
        return handle_time(req, resp);
    } else if (strcmp(path, "echo") == 0) {
        if (method != COAP_METHOD_POST) {
            resp->code = COAP_ERROR_METHOD_NOT_ALLOWED;
            return 0;
        }
        return handle_echo(req, resp);
    }

    // No encontrado
    resp->code = COAP_ERROR_NOT_FOUND;
    return 0;
}