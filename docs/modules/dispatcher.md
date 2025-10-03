# Dispatcher (dispatcher.c / dispatcher.h)

Responsabilidad
- Validar que la petición sea CoAP válida y de tipo request.
- Preparar la respuesta espejando token y message_id, y el tipo de respuesta:
  - CON -> ACK (piggyback)
  - NON -> NON
- Resolver método y ruta (Uri-Path) y delegar al handler.

Flujo
1) Validación básica: coap_message_is_valid + coap_message_is_request.
2) init_response_from_request: copia version, token, id y tipo ACK/NON.
3) Obtener ruta con coap_message_get_uri_path().
4) Mapear método (GET, POST, ...) por coap_code_class/detail.
5) Routing simple por string exacto: "hello", "time", "echo".
6) Devolver códigos de error de cliente si corresponde:
   - 4.05 Method Not Allowed cuando la ruta existe pero el método no aplica.
   - 4.04 Not Found si no hay coincidencia de ruta.

Rutas actuales
- GET /hello -> handle_hello
- GET /time  -> handle_time
- POST /echo -> handle_echo

Extensiones
- Para agregar /foo:
  - Implementar int handle_foo(const CoapMessage*, CoapMessage*)
  - Añadir rama en dispatcher.c para "foo" y validar método.
