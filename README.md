# TeleServer

Servidor concurrente CoAP sobre UDP en C.

## Características
- Codec CoAP conforme a RFC 7252 (perfil mínimo): decode/encode con extensiones 13/14
- Event loop multiplataforma (kqueue en macOS, epoll en Linux)
- Dispatcher + handlers de producción (API v1): POST/GET /api/v1/telemetry, GET /api/v1/health, GET /api/v1/status
- Servidor UDP integrado con event loop y codec

## Estructura
- include/: headers públicos (platform, event_loop, coap, codec, dispatcher, server)
- src/
  - platform/: sockets, utilidades, event loop por OS
  - coap/: utilidades de tipos, decode/encode
  - core/: dispatcher y handlers
  - server/: servidor UDP y main
  - clients/: (no se utiliza en este repo; el cliente vive en ../TeleClient)
- tests/: pruebas unitarias e integración (assert + sanitizers)
- docs/: documentación del plan y arquitectura
- bin/, build/: artefactos generados

## Requisitos
- clang o gcc, make
- Opcional: clang-tidy, clang-format, cppcheck, valgrind (Linux)

## Compilación
- Debug (con ASan/UBSan):
  make debug
- Release:
  make release

## Ejecutar servidor
```
./bin/tele_server_debug --port 5683 --verbose
```

## Tests
```
make test
```
Ejecuta todas las pruebas con sanitizers.

## Lint y formato
- Formato (clang-format):
```
make format
```
- Lint (clang-tidy y cppcheck):
```
make lint
```
Nota: instala primero clang-format/clang-tidy (e.g., Homebrew: `brew install llvm cppcheck`).

## Estilo y convenciones
- C: 4 espacios de indentación, llaves en la misma línea, -std=c11, -Wall -Wextra -Werror
- Seguridad: validar punteros, evitar desbordes, usar snprintf/strncpy; sin funciones peligrosas
- Ver WARP.md y docs/PLAN.md para más detalles

---

## Descripción del proyecto

TeleServer es un servidor CoAP (RFC 7252) orientado a IoT que opera sobre UDP y está pensado para:
- Recibir telemetría de dispositivos (ESP32 u otros) mediante POST /api/v1/telemetry
- Servir la telemetría almacenada a clientes de monitoreo (TeleClient) mediante GET /api/v1/telemetry
- Exponer endpoints de salud y estado para observabilidad

Características clave:
- Implementación propia de CoAP (decode/encode) sin dependencias externas
- Bucle de eventos no bloqueante (kqueue en macOS, epoll en Linux)
- Módulo de almacenamiento en memoria (ring buffer) desacoplado de CoAP
- Logging con niveles y trazas RX/TX de CoAP en modo verbose
- Pruebas unitarias e integración con sanitizers

## Arquitectura

Pipeline de procesamiento:
```
UDP Socket (non-blocking)
  → EventLoop detecta lectura
  → process_datagram() lee datagrama
  → coap_decode() → CoapMessage
  → dispatcher_handle_request() por método + Uri-Path
  → handle_*() construye respuesta
  → coap_encode()
  → sendto()
```

Módulos principales:
- platform/: abstracciones de SO (socket, event loop, tiempo)
- coap/: codec y utilidades (RFC 7252, extensiones 13/14)
- core/: routing y lógica (dispatcher, handlers, telemetry_storage, time_source)
- server/: integración (server.c) y punto de entrada (main.c)

## Uso de la API de Telemetry

Todos los endpoints usan CoAP. Las rutas se forman concatenando segmentos Uri‑Path (opción 11). El formato de contenido para JSON es application/json con valor 50 en Content‑Format.

### POST /api/v1/telemetry

Propósito: que el dispositivo envíe medidas de sensores en JSON.

- Método: POST
- Content‑Format: application/json (50)
- Payload: JSON con los 4 campos obligatorios

Ejemplo de payload:
```json
{
  "temperatura": 25.5,
  "humedad": 60.2,
  "voltaje": 3.7,
  "cantidad_producida": 150
}
```

Respuestas:
- 2.01 Created → {"status":"ok"}
- 4.00 Bad Request → {"error":"missing payload"} | {"error":"invalid json format"}
- 5.00 Internal Server Error → {"error":"storage error"}

Con TeleClient :
```bash
# Simular un ESP32 enviando telemetría
../TeleClient/bin/tele_client localhost:5683 POST /api/v1/telemetry \
  '{"temperatura":25.5,"humedad":60.2,"voltaje":3.7,"cantidad_producida":150}'
```

### GET /api/v1/telemetry

Propósito: que el cliente de monitoreo recupere los datos almacenados.

- Método: GET
- Sin payload
- Respuesta: 2.05 Content con un arreglo JSON, orden del más antiguo al más reciente

Ejemplo de respuesta:
```json
[
  {
    "data": {
      "temperatura": 25.5,
      "humedad": 60.2,
      "voltaje": 3.7,
      "cantidad_producida": 150
    },
    "timestamp": 1696294955123
  }
]
```

Con TeleClient:
```bash
../TeleClient/bin/tele_client localhost:5683 GET /api/v1/telemetry
```

### GET /api/v1/health

- Método: GET
- Respuesta: 2.05 Content
```json
{"status":"ok","service":"TeleServer"}
```

### GET /api/v1/status

- Método: GET
- Respuesta: 2.05 Content con métricas básicas
```json
{
  "uptime_ms": 123456789,
  "telemetry_received": 1543,
  "telemetry_stored": 100,
  "capacity": 100
}
```

## Límites y formatos
- Tamaño máximo de mensaje: 1472 bytes (evita fragmentación IP)
- Longitud de opción: hasta 270 bytes
- Token: 0–8 bytes
- Content‑Format:
  - text/plain → 0 (longitud 0 como representación de entero 0)
  - application/json → 50

## Ejecución rápida
```bash
# Compilar (debug con sanitizers)
make debug

# Ejecutar en modo verbose (puerto por defecto 5683)
./bin/tele_server_debug --port 5683 --verbose
```

## Pruebas e integración
```bash
# Ejecutar todos los tests (unitarios e integración)
make test

# End-to-end con TeleClient
../TeleClient/bin/tele_client localhost:5683 GET /api/v1/health
```

## Seguridad
- Estado actual: sin DTLS/autenticación (adecuado para VPC/red privada)
- Para internet pública: considerar DTLS, autenticación por tokens, rate limiting y firewall

## Documentación relacionada
- docs/API_V1.md — Especificación completa de la API
- docs/architecture.md — Detalle de arquitectura
- docs/testing.md — Estrategia de pruebas
- PRODUCCION_README.md — Guía de despliegue en producción
