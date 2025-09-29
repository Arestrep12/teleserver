#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "coap.h"

// Procesa una request CoAP y construye la respuesta en 'resp'.
// - Retorna 0 si se pudo enrutar y responder, <0 si ocurrió un error.
int dispatcher_handle_request(const CoapMessage *req, CoapMessage *resp);

#endif // DISPATCHER_H