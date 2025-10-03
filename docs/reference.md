# Referencia de API por headers

Esta sección enumera la API pública expuesta por los headers en include/ con
descripciones de parámetros, retornos y consideraciones.

platform.h
- platform_init(void): Inicializa recursos de plataforma; actualmente imprime la
  plataforma objetivo.
- platform_cleanup(void): Limpieza opcional.
- platform_get_time_ms(void) -> uint64_t: Tiempo actual en ms.
- platform_error_string(int) -> const char*: Texto para un código de error.
- Sockets:
  - platform_socket_create_udp(void) -> int: Crea socket UDP (>=0 ok).
  - platform_socket_bind(int sock, uint16_t port) -> int: Enlaza socket.
  - platform_socket_set_nonblocking(int sock) -> int: O_NONBLOCK.
  - platform_socket_set_reuseaddr(int sock) -> int: Reutilización.
  - platform_socket_close(int sock): Cierra.
  - platform_socket_recvfrom(...), platform_socket_sendto(...): I/O no bloqueante
    con códigos PLATFORM_*.

event_loop.h
- EventLoop*: tipo opaco del bucle.
- event_loop_create/destroy
- FDs:
  - event_loop_add_fd(loop, fd, events, callback, user): registra FD con
    máscara EVENT_READ/EVENT_WRITE. callback(fd, events, user_data).
  - event_loop_remove_fd, event_loop_modify_fd
- Timers:
  - event_loop_add_timer(loop, timeout_ms, periodic, cb, user) -> id
  - event_loop_remove_timer(loop, id)
- Ejecución:
  - event_loop_run(loop, timeout_ms): <0 error; timeout<0 corre hasta stop.
  - event_loop_stop(loop)
  - event_loop_is_running(loop) -> bool

coap.h
- Tipos del protocolo: CoapType, CoapCode, CoapOption, CoapContentFormat.
- CoapMessage: representación en memoria con opciones ordenadas y payload_buffer.
- Utilidades:
  - coap_type_to_string, coap_code_to_string, coap_option_to_string.
  - coap_code_class/detail, coap_make_code.
  - coap_message_init/clear.
  - coap_message_add_option(msg, number, value, length) -> int: Inserta en orden.
  - coap_message_find_option, coap_message_get_uri_path.
  - coap_message_is_valid/is_request/is_response.

coap_codec.h
- coap_decode(msg, buffer, length) -> int: 0 OK, <0 error (EINVAL/EMALFORMED/...)
- coap_encode(msg, out, out_size) -> int: bytes escritos o <0 fallo (E2SMALL,...)
- coap_message_can_encode(msg) -> bool: Validación previa a encode.

server.h
- Server*: tipo opaco del servidor.
- server_create(port, verbose) -> Server* (port=0 => efímero).
- server_destroy
- server_run(loop, timeout_ms) -> int: -1 error; <0 códigos platform.
- server_stop: Señaliza detener el loop si está en modo infinito.
- server_get_port(const Server*) -> uint16_t: puerto efectivo.

dispatcher.h
- dispatcher_handle_request(const CoapMessage* req, CoapMessage* resp) -> int
  - Retorna 0 en éxito (resp listo). Nunca envía por socket.

handlers.h
- handle_hello(req, resp): GET /hello -> "hello" (text/plain).
- handle_time(req, resp): GET /time -> milisegundos desde epoch.
- handle_echo(req, resp): POST /echo -> eco del payload.

log.h
- log_set_level(LogLevel), log_set_stream(FILE*)
- log_printf(level, fmt, ...)
- Macros: LOG_ERROR/WARN/INFO/DEBUG

time_source.h
- time_source_set(TimeSource* ts): Inyecta fuente de tiempo (o NULL para default).
- time_source_now_ms(void): Lee ms usando la fuente actual.

Consideraciones generales
- Todos los punteros deben ser válidos (no NULL) salvo que se indique.
- En coap_message_add_option se preserva el orden de opciones (inserción ordenada).
- coap_decode valida versión, TKL, tipo y que la opción 15 no aparezca.
- Las funciones platform_* retornan PLATFORM_OK/ERROR/EAGAIN/EINVAL según contexto.
