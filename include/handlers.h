#ifndef HANDLERS_H
#define HANDLERS_H

#include <stddef.h>
#include <stdint.h>
#include "coap.h"
#include "platform.h"

// Retorna 0 en éxito, <0 en error

// === Rutas de Producción (API v1) ===
// POST /api/v1/telemetry - Recibe JSON de telemetría desde ESP32
int handle_telemetry_post(const CoapMessage *req, CoapMessage *resp);

// GET /api/v1/telemetry - Devuelve todos los JSON almacenados
int handle_telemetry_get(const CoapMessage *req, CoapMessage *resp);

// GET /api/v1/health - Health check
int handle_health(const CoapMessage *req, CoapMessage *resp);

// GET /api/v1/status - Estadísticas del servidor
int handle_status(const CoapMessage *req, CoapMessage *resp);

// === Rutas de Testing ===
// POST /test/echo - Echo para debugging
int handle_test_echo(const CoapMessage *req, CoapMessage *resp);

// === Rutas Legacy (deprecadas, mantener para compatibilidad) ===
int handle_hello(const CoapMessage *req, CoapMessage *resp);
int handle_time(const CoapMessage *req, CoapMessage *resp);
int handle_echo(const CoapMessage *req, CoapMessage *resp);

#endif // HANDLERS_H
