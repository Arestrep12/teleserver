/*
 * dispatcher.c — Enrutamiento de requests CoAP a handlers de aplicación.
 *
 * Responsabilidades
 * - Construir respuesta base (mirror de token/message_id, tipo piggyback/ NON).
 * - Resolver path (Uri-Path) y método, validando 404 y 405 cuando corresponda.
 * - Separar rutas de producción, testing y legacy.
 */
#include "dispatcher.h"
#include "handlers.h"
#include "log.h"
#include <string.h>

/*
 * init_response_from_request
 * -------------------------
 * Inicializa la respuesta a partir de la request: espejo de token/message_id y
 * tipo piggyback ACK/ NON según el tipo de la request.
 */
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

/*
 * method_from_code
 * ----------------
 * Traduce un CoapCode de clase 0.xx al entero del método (GET=1, POST=2,...).
 * Retorna 0 si no es un método.
 */
static int method_from_code(CoapCode code) {
    // Devuelve 1=GET,2=POST,3=PUT,4=DELETE o 0 si no es método
    if (coap_code_class(code) != 0) return 0;
    return (int)code;
}

/*
 * dispatcher_handle_request
 * -------------------------
 * Punto de entrada del routing. Valida que la request sea válida y de clase
 * método, extrae el path y decide el handler correspondiente. En errores de
 * routing, establece resp->code con 4.04/4.05/4.00 y retorna 0 (respuesta
 * válida codificable). Retorna <0 sólo ante errores no recuperables.
 */
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

    // === Routing API v1 (Producción) ===
    if (strcmp(path, "api/v1/telemetry") == 0) {
        if (method == COAP_METHOD_POST) {
            return handle_telemetry_post(req, resp);
        } else if (method == COAP_METHOD_GET) {
            return handle_telemetry_get(req, resp);
        } else {
            LOG_WARN("dispatcher: 405 Method Not Allowed for /api/v1/telemetry (method=%d)\n", method);
            resp->code = COAP_ERROR_METHOD_NOT_ALLOWED;
            return 0;
        }
    }
    
    if (strcmp(path, "api/v1/health") == 0) {
        if (method != COAP_METHOD_GET) {
            LOG_WARN("dispatcher: 405 Method Not Allowed for /api/v1/health (method=%d)\n", method);
            resp->code = COAP_ERROR_METHOD_NOT_ALLOWED;
            return 0;
        }
        return handle_health(req, resp);
    }
    
    if (strcmp(path, "api/v1/status") == 0) {
        if (method != COAP_METHOD_GET) {
            LOG_WARN("dispatcher: 405 Method Not Allowed for /api/v1/status (method=%d)\n", method);
            resp->code = COAP_ERROR_METHOD_NOT_ALLOWED;
            return 0;
        }
        return handle_status(req, resp);
    }

    // === Routing de Testing ===
    if (strcmp(path, "test/echo") == 0) {
        if (method != COAP_METHOD_POST) {
            LOG_WARN("dispatcher: 405 Method Not Allowed for /test/echo (method=%d)\n", method);
            resp->code = COAP_ERROR_METHOD_NOT_ALLOWED;
            return 0;
        }
        return handle_test_echo(req, resp);
    }

    // === Routing Legacy (deprecado, mantener para compatibilidad) ===
    if (strcmp(path, "hello") == 0) {
        if (method != COAP_METHOD_GET) {
            LOG_WARN("dispatcher: 405 Method Not Allowed for /hello (method=%d)\n", method);
            resp->code = COAP_ERROR_METHOD_NOT_ALLOWED;
            return 0;
        }
        return handle_hello(req, resp);
    }
    
    if (strcmp(path, "time") == 0) {
        if (method != COAP_METHOD_GET) {
            LOG_WARN("dispatcher: 405 Method Not Allowed for /time (method=%d)\n", method);
            resp->code = COAP_ERROR_METHOD_NOT_ALLOWED;
            return 0;
        }
        return handle_time(req, resp);
    }
    
    if (strcmp(path, "echo") == 0) {
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
