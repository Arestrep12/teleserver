# Pruebas y cobertura

Tipos de pruebas
- Unitarias: validan piezas aisladas (codec CoAP, tipos, dispatcher, event loop,
  plataforma, time source).
- Integración: server UDP end-to-end con un socket UDP simple.
- End-to-end con TeleClient: usa el cliente real (../TeleClient) para generar
  requests y validar respuestas reales del server.

Ejecución
- `make test` compila y ejecuta todos los binarios de tests bajo build/tests/.
- Se compila con ASan/UBSan para detectar defectos de memoria y UB.

Cobertura funcional
- test_coap_codec.c: round-trip encode/decode, extensiones 13/14, errores
  (TKL inválido, nibble 15, opciones fuera de orden, buffer pequeño, etc.).
- test_coap_types.c: utilidades de códigos, inicialización de mensajes, manejo de
  opciones y verificación de validación.
- test_dispatcher.c: rutas GET /hello, GET /time, POST /echo, 404 y 405.
- test_event_loop.c: creación/destroy, add/remove FD, timer, evento de lectura.
- test_platform.c: creación de socket, bind, nonblocking, tiempo.
- test_time_source.c: inyección de fuente y lectura.
- test_server_integration.c: servidor real + cliente UDP simple.
- test_server_client_integration.c: servidor real en hilo + TeleClient real con
  GET/POST y validaciones (CON/NON, payload grande, 404, 405).

Cómo añadir nuevas pruebas
- Crea tests/test_*.c; el Makefile los compila automáticamente e inyecta las
  fuentes del servidor (y, si corresponde, el cliente).

Notas
- Evita sleeps largos; usa timeouts pequeños y reintentos cuando aplique.
- Para depurar, habilita `verbose=true` en server_create o aumenta nivel de LOG.
