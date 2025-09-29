#ifndef HANDLERS_H
#define HANDLERS_H

#include <stddef.h>
#include <stdint.h>
#include "coap.h"
#include "platform.h"

// Retorna 0 en éxito, <0 en error
int handle_hello(const CoapMessage *req, CoapMessage *resp);
int handle_time(const CoapMessage *req, CoapMessage *resp);
int handle_echo(const CoapMessage *req, CoapMessage *resp);

#endif // HANDLERS_H