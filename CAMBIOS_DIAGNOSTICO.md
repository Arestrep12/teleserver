# Cambios para Diagnóstico de "dispatcher error -1"

## Fecha
2025-10-03

## Problema
El ESP32 enviaba tramas CoAP que provocaban "dispatcher error -1" en el servidor. Este error silencioso dificultaba el diagnóstico ya que el servidor simplemente droppeaba el paquete sin respuesta.

## Cambios Realizados

### 1. Arduino (arduino/Teleserver_ESP32.ino)

#### Ajuste de Content-Format en POST /echo
**Problema:** El sketch codificaba Content-Format text/plain (0) con longitud 1 y valor 0x00, cuando el RFC 7252 y el servidor esperan longitud 0 para el valor 0.

**Cambio:**
```c
// ANTES:
// Content-Format (12): text/plain (0) => delta=1, len=1, value=0x00
pkt[n++] = (uint8_t)((1 << 4) | 1);
pkt[n++] = 0x00;

// DESPUÉS:
// Content-Format (12): text/plain (0) => delta=1, len=0 (RFC7252: 0 se codifica con longitud 0)
pkt[n++] = (uint8_t)((1 << 4) | 0);
```

**Impacto:** Aunque no causaba el error -1, este cambio alinea el cliente con las prácticas del servidor y el RFC 7252.

### 2. Servidor - Dispatcher (src/core/dispatcher.c)

#### Logging de diagnóstico detallado

**Agregado:**
- Log de método y path en cada request: `LOG_INFO("dispatcher: method=%d path=\"%s\"\n", method, path);`
- Logs específicos para cada error 404/405 con el path y método que lo causaron
- Detalle adicional en el log "not a request": ahora incluye class y detail del code

**Beneficio:** Ahora es visible en los logs:
- Qué método y ruta llegó al dispatcher
- Por qué se rechaza (404, 405, invalid request)
- Si el código no es una request (class != 0)

### 3. Servidor - Server (src/server/server.c)

#### Respuesta 4.00 Bad Request en lugar de dropeo silencioso

**Problema:** Cuando dispatcher_handle_request retornaba -1, el servidor simplemente logueaba un warning y dropeaba el paquete sin enviar respuesta al cliente.

**Cambio:**
```c
if (rc != 0) {
    if (srv->verbose) LOG_WARN("dispatcher error %d, sending 4.00 Bad Request\n", rc);
    // Construir respuesta de error mínima
    coap_message_init(&resp);
    resp.version = COAP_VERSION;
    resp.message_id = req.message_id;
    resp.token_length = req.token_length;
    if (req.token_length > 0) {
        memcpy(resp.token, req.token, req.token_length);
    }
    if (req.type == COAP_TYPE_CONFIRMABLE) {
        resp.type = COAP_TYPE_ACKNOWLEDGMENT;
    } else {
        resp.type = COAP_TYPE_NON_CONFIRMABLE;
    }
    resp.code = COAP_ERROR_BAD_REQUEST;
}
```

**Beneficio:** 
- El ESP32 ahora recibe una respuesta CoAP con 4.00 Bad Request
- Permite confirmar que el servidor recibió el paquete
- Facilita el debugging desde el lado del cliente

## Causas Comunes de "dispatcher error -1"

El dispatcher solo devuelve -1 en estos casos:

1. **Mensaje CoAP inválido** (`coap_message_is_valid` = false):
   - Versión != 1
   - Tipo fuera del rango 0-3
   - TKL > 8
   - Opciones desordenadas (números de opción decrecientes)

2. **No es una request CoAP** (`coap_message_is_request` = false):
   - El code tiene class != 0 (es decir, es una respuesta 2.xx, 4.xx, 5.xx)
   - El code es 0 (Empty)

3. **Error en handler**:
   - Un handler (handle_hello, handle_time, handle_echo) retorna -1
   - Ejemplo: payload demasiado grande para el buffer (> 1472 bytes)

## Verificación

Todos los tests pasaron exitosamente después de los cambios:
- `test_dispatcher`: Confirma routing correcto y logs apropiados
- `test_server_integration`: Confirma server responde a datagramas UDP CoAP
- `test_server_client_integration`: Confirma interoperabilidad con TeleClient
- Todos los tests con sanitizers (ASan/UBSan) limpios

## Uso para Debugging

### Ejecutar servidor con logging detallado:
```bash
./bin/tele_server_debug --port 5683 --verbose
```

### Logs esperados para request válida (GET /hello):
```
[INFO] dispatcher: method=1 path="hello"
```

### Logs esperados para 404:
```
[INFO] dispatcher: method=1 path="nope"
[WARN] dispatcher: 404 Not Found for path="nope"
```

### Logs esperados para 405:
```
[INFO] dispatcher: method=2 path="hello"
[WARN] dispatcher: 405 Method Not Allowed for /hello (method=2)
```

### Logs esperados para error -1 (not a request):
```
[ERROR] dispatcher: not a request (code=69, class=2, detail=5)
[WARN] dispatcher error -1, sending 4.00 Bad Request
```

