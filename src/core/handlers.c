/*
 * handlers.c — Implementación de lógica de endpoints (producción/testing/legacy).
 *
 * Las funciones construyen respuestas CoAP completas (code, payload y options).
 * En errores de negocio devuelven 0 con resp->code en 4.xx/5.xx; sólo retornan
 * <0 ante errores internos irreparables (p.ej., buffers insuficientes).
 */
#include "handlers.h"
#include "time_source.h"
#include "telemetry_storage.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

/*
 * set_content_format_text
 * -----------------------
 * Establece Content-Format: text/plain (0) usando longitud 0 como codificación
 * del entero 0 según RFC 7252.
 */
static int set_content_format_text(CoapMessage *resp) {
    // Content-Format: text/plain (0) => entero 0 codificado como longitud 0
    // (RFC7252: representación de 0 es cadena de longitud 0)
    return coap_message_add_option(resp, COAP_OPTION_CONTENT_FORMAT, NULL, 0);
}

/*
 * handle_hello (Legacy)
 * ---------------------
 * GET /hello — devuelve "hello" para pruebas básicas de conectividad.
 */
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

/*
 * handle_time (Legacy)
 * --------------------
 * GET /time — devuelve el tiempo actual en ms (inyectable vía time_source).
 */
int handle_time(const CoapMessage *req, CoapMessage *resp) {
    (void)req;
    if (!resp) return -1;

    // Tiempo en milisegundos desde epoch (inyectable)
    uint64_t now = time_source_now_ms();
    int n = snprintf((char *)resp->payload_buffer, sizeof(resp->payload_buffer), "%llu",
                     (unsigned long long)now);
    if (n < 0) return -1;
    resp->payload = resp->payload_buffer;
    resp->payload_length = (size_t)n;
    (void)set_content_format_text(resp);
    resp->code = COAP_RESPONSE_CONTENT;
    return 0;
}

/*
 * handle_echo (Legacy)
 * --------------------
 * POST /echo — retorna el payload recibido sin modificaciones.
 */
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

// ============================================================================
// Rutas de Producción (API v1)
// ============================================================================

/*
 * set_content_format_json
 * -----------------------
 * Establece Content-Format: application/json (50).
 */
static int set_content_format_json(CoapMessage *resp) {
    // Content-Format: application/json (50) => 1 byte con valor 50
    uint8_t json_fmt = 50;
    return coap_message_add_option(resp, COAP_OPTION_CONTENT_FORMAT, &json_fmt, 1);
}

/*
 * is_valid_json
 * -------------
 * Validación ligera específica del proyecto: requiere llaves y presencia de
 * cuatro campos obligatorios. No valida tipos ni estructura detallada.
 */
static bool is_valid_json(const char *str, size_t len) {
    // Validación básica: debe empezar con '{' y terminar con '}'
    if (len < 2) return false;
    if (str[0] != '{' || str[len - 1] != '}') return false;
    // Verificar que contenga los 4 campos requeridos (validación simple)
    const char *required[] = {"temperatura", "humedad", "voltaje", "cantidad_producida"};
    for (size_t i = 0; i < 4; i++) {
        if (!strstr(str, required[i])) return false;
    }
    return true;
}

/*
 * handle_telemetry_post
 * ---------------------
 * POST /api/v1/telemetry — almacena el JSON recibido en el ring buffer.
 * Respuestas:
 * - 2.01 Created en éxito
 * - 4.00 Bad Request en JSON inválido o sin payload
 * - 5.00 Internal Server Error si falla el almacenamiento
 */
int handle_telemetry_post(const CoapMessage *req, CoapMessage *resp) {
    if (!req || !resp) return -1;

    // Validar que hay payload
    if (!req->payload || req->payload_length == 0) {
        resp->code = COAP_ERROR_BAD_REQUEST;
        const char *msg = "{\"error\":\"missing payload\"}";
        size_t len = strlen(msg);
        memcpy(resp->payload_buffer, msg, len);
        resp->payload = resp->payload_buffer;
        resp->payload_length = len;
        (void)set_content_format_json(resp);
        return 0;
    }

    // Validar JSON básico
    if (!is_valid_json((const char *)req->payload, req->payload_length)) {
        resp->code = COAP_ERROR_BAD_REQUEST;
        const char *msg = "{\"error\":\"invalid json format\"}";
        size_t len = strlen(msg);
        memcpy(resp->payload_buffer, msg, len);
        resp->payload = resp->payload_buffer;
        resp->payload_length = len;
        (void)set_content_format_json(resp);
        LOG_WARN("telemetry_post: invalid JSON received\n");
        return 0;
    }

    // Guardar en storage
    int rc = telemetry_storage_add((const char *)req->payload, req->payload_length);
    if (rc != 0) {
        resp->code = COAP_ERROR_INTERNAL;
        const char *msg = "{\"error\":\"storage error\"}";
        size_t len = strlen(msg);
        memcpy(resp->payload_buffer, msg, len);
        resp->payload = resp->payload_buffer;
        resp->payload_length = len;
        (void)set_content_format_json(resp);
        LOG_ERROR("telemetry_post: storage_add error %d\n", rc);
        return 0;
    }

    // Respuesta exitosa
    resp->code = COAP_RESPONSE_CREATED; // 2.01
    const char *msg = "{\"status\":\"ok\"}";
    size_t len = strlen(msg);
    memcpy(resp->payload_buffer, msg, len);
    resp->payload = resp->payload_buffer;
    resp->payload_length = len;
    (void)set_content_format_json(resp);
    LOG_INFO("telemetry_post: stored %zu bytes\n", req->payload_length);
    return 0;
}

/*
 * handle_telemetry_get
 * --------------------
 * GET /api/v1/telemetry — retorna todas las entradas en un arreglo JSON.
 */
int handle_telemetry_get(const CoapMessage *req, CoapMessage *resp) {
    (void)req;
    if (!resp) return -1;

    // Serializar todas las entradas a JSON array
    int json_len = telemetry_storage_serialize_json(
        (char *)resp->payload_buffer, sizeof(resp->payload_buffer));
    
    if (json_len < 0) {
        resp->code = COAP_ERROR_INTERNAL;
        const char *msg = "{\"error\":\"serialization error\"}";
        size_t len = strlen(msg);
        memcpy(resp->payload_buffer, msg, len);
        resp->payload = resp->payload_buffer;
        resp->payload_length = len;
        (void)set_content_format_json(resp);
        LOG_ERROR("telemetry_get: serialization error\n");
        return 0;
    }

    resp->code = COAP_RESPONSE_CONTENT; // 2.05
    resp->payload = resp->payload_buffer;
    resp->payload_length = (size_t)json_len;
    (void)set_content_format_json(resp);
    LOG_INFO("telemetry_get: returned %d bytes\n", json_len);
    return 0;
}

/*
 * handle_health
 * -------------
 * GET /api/v1/health — health check simple del servicio.
 */
int handle_health(const CoapMessage *req, CoapMessage *resp) {
    (void)req;
    if (!resp) return -1;

    const char *msg = "{\"status\":\"ok\",\"service\":\"TeleServer\"}";
    size_t len = strlen(msg);
    if (len > sizeof(resp->payload_buffer)) return -1;
    memcpy(resp->payload_buffer, msg, len);
    resp->payload = resp->payload_buffer;
    resp->payload_length = len;
    (void)set_content_format_json(resp);
    resp->code = COAP_RESPONSE_CONTENT;
    return 0;
}

/*
 * handle_status
 * -------------
 * GET /api/v1/status — estadísticas del servidor: uptime, conteos y capacidad.
 */
int handle_status(const CoapMessage *req, CoapMessage *resp) {
    (void)req;
    if (!resp) return -1;

    TelemetryStats stats;
    telemetry_storage_get_stats(&stats);
    uint64_t now = time_source_now_ms();

    int n = snprintf((char *)resp->payload_buffer, sizeof(resp->payload_buffer),
                     "{\"uptime_ms\":%llu,\"telemetry_received\":%zu,"
                     "\"telemetry_stored\":%zu,\"capacity\":%zu}",
                     (unsigned long long)now,
                     stats.total_received,
                     stats.current_count,
                     stats.capacity);
    if (n < 0 || (size_t)n >= sizeof(resp->payload_buffer)) return -1;
    
    resp->payload = resp->payload_buffer;
    resp->payload_length = (size_t)n;
    (void)set_content_format_json(resp);
    resp->code = COAP_RESPONSE_CONTENT;
    return 0;
}

/*
 * handle_test_echo (Testing)
 * -------------------------
 * POST /test/echo — echo para depuración. Implementación delega en handle_echo.
 */
int handle_test_echo(const CoapMessage *req, CoapMessage *resp) {
    // Idéntico a handle_echo (mantener separado para semántica)
    return handle_echo(req, resp);
}
