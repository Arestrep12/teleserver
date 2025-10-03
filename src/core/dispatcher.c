#include "dispatcher.h"
#include "handlers.h"
#include "log.h"
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
    if (!req || !resp) {
        LOG_ERROR("dispatcher: NULL pointer (req=%p, resp=%p)\n", (void*)req, (void*)resp);
        return -1;
    }
    if (!coap_message_is_valid(req)) {
        LOG_ERROR("dispatcher: invalid message (version=%u, type=%u, token_len=%u)\n",
                  req->version, req->type, req->token_length);
        return -1;
    }
    if (!coap_message_is_request(req)) {
        LOG_ERROR("dispatcher: not a request (code=%u, class=%u, detail=%u)\n",
                  req->code, coap_code_class(req->code), coap_code_detail(req->code));
        return -1;
    }

    init_response_from_request(req, resp);

    // Obtener ruta
    char path[128];
    int path_len = coap_message_get_uri_path(req, path, sizeof(path));
    if (path_len < 0) {
        LOG_WARN("dispatcher: failed to extract uri_path\n");
        resp->code = COAP_ERROR_BAD_REQUEST;
        return 0;
    }

    int method = method_from_code(req->code);
    if (method == 0) {
        LOG_WARN("dispatcher: invalid method (code=%u)\n", req->code);
        resp->code = COAP_ERROR_BAD_REQUEST;
        return 0;
    }

    LOG_INFO("dispatcher: method=%d path=\"%s\"\n", method, path);

    // Routing simple
    if (strcmp(path, "hello") == 0) {
        if (method != COAP_METHOD_GET) {
            LOG_WARN("dispatcher: 405 Method Not Allowed for /hello (method=%d)\n", method);
            resp->code = COAP_ERROR_METHOD_NOT_ALLOWED;
            return 0;
        }
        return handle_hello(req, resp);
    } else if (strcmp(path, "time") == 0) {
        if (method != COAP_METHOD_GET) {
            LOG_WARN("dispatcher: 405 Method Not Allowed for /time (method=%d)\n", method);
            resp->code = COAP_ERROR_METHOD_NOT_ALLOWED;
            return 0;
        }
        return handle_time(req, resp);
    } else if (strcmp(path, "echo") == 0) {
        if (method != COAP_METHOD_POST) {
            LOG_WARN("dispatcher: 405 Method Not Allowed for /echo (method=%d)\n", method);
            resp->code = COAP_ERROR_METHOD_NOT_ALLOWED;
            return 0;
        }
        return handle_echo(req, resp);
    }

    // No encontrado
    LOG_WARN("dispatcher: 404 Not Found for path=\"%s\"\n", path);
    resp->code = COAP_ERROR_NOT_FOUND;
    return 0;
}
