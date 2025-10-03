# Servidor (server.c / server.h)

Responsabilidad
- Gestionar el socket UDP, integrar el EventLoop y el pipeline CoAP end-to-end.
- API pública para crear, ejecutar y destruir la instancia del servidor.

Estructura interna (server.c)
- struct Server: contiene loop, sock, verbose y port.
- server_create(port, verbose):
  - Crea EventLoop y socket UDP.
  - Configura SO_REUSEADDR y O_NONBLOCK.
  - Realiza bind (port=0 => efímero) y guarda el puerto real con getsockname.
  - Registra el socket en el EventLoop con on_readable.
- server_run(srv, run_timeout_ms): ejecuta el bucle; si run_timeout_ms<0, corre
  hasta server_stop().
- on_readable: drena el socket con recvfrom en lazo hasta EAGAIN; para cada
  datagrama invoca process_datagram.
- process_datagram: coap_decode -> dispatcher_handle_request -> coap_encode -> sendto.
- server_stop: marca el loop para detenerse.
- server_get_port: devuelve el puerto efectivo (útil si se pasó 0).

Detalles importantes
- Manejo de errores conservador: si decode/dispatcher/encode falla, se omite el
  envío (y se loguea en modo verbose).
- No hace retransmisión ni semántica de confirmación CoAP más allá del tipo
  piggyback de ACK para CON y NON para NON; el control de retransmisión queda
  del lado del cliente (p. ej., TeleClient soporta reintentos/timeout).

Interacción con otros módulos
- platform/socket: I/O UDP no bloqueante y utilidades.
- event_loop: registro de FD y callbacks.
- coap_codec: serialización y parseo de mensajes.
- core/dispatcher: routing y selección de handlers.

Ejemplo de uso (binario)
- main.c parsea --port y --verbose, inicializa plataforma, crea servidor y
  llama a server_run en modo infinito.
