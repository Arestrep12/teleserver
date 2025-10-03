# Arquitectura

Visión general
- Modelo: servidor UDP single‑process, event‑driven, no bloqueante.
- Bucle de eventos: abstracción propia (EventLoop) con backends kqueue (macOS)
  y epoll (Linux).
- Pipeline de petición:
  1) Socket UDP recibe datagrama.
  2) coap_decode convierte bytes en CoapMessage.
  3) dispatcher_handle_request enruta por Uri‑Path y método.
  4) Handler construye CoapMessage de respuesta.
  5) coap_encode serializa a bytes.
  6) sendto envía la respuesta al cliente.

Diagrama de flujo (alto nivel)

```
UDP socket (non-blocking) -> EventLoop -> process_datagram()
  -> coap_decode() -> dispatcher_handle_request()
    -> handle_*() -> coap_encode() -> sendto()
```

Capas y responsabilidades
- coap/ (encode, decode, utils):
  - Implementa serialización RFC 7252 con nibble extendido 13/14 y payload marker 0xFF.
  - Asegura orden ascendente de opciones y límites de longitud.
- core/ (dispatcher, handlers, time_source):
  - Dispatcher resuelve ruta y método. Mirror de token/id, tipo piggyback.
  - Handlers de ejemplo: hello, time, echo.
  - time_source hace injeción de fuente de tiempo para pruebas.
- platform/ (socket, event_loop_*):
  - Envolturas de socket y bucle de eventos con timers.
  - MacOS usa kqueue; Linux usa epoll. API uniforme.
- server/ (server.c, main.c):
  - Crea socket, registra FD en EventLoop, consume eventos y procesa datagramas.
  - Expone API server_* y binario ejecutable (main.c).

Concurrencia y rendimiento
- El diseño es orientado a eventos y no usa hilos internos (salvo en tests para
  levantar el server en paralelo). Esto simplifica el modelo y favorece
  latencia baja bajo carga moderada.
- I/O no bloqueante con buffers tamaño MTU (1472 bytes por defecto).
- Timers soportados en el EventLoop (periodicidad y one‑shot).

Gestión de errores
- Códigos negativos uniformes en platform y codec.
- Estrategia: validar todo (version, tkl, orden de opciones, límites de
  longitud). Si un datagrama es inválido, se descarta silenciosamente (server.c)
  para evitar amplificación.

Compatibilidad
- macOS y Linux. Fallará en compilación en otras plataformas (platform.h).

Extensibilidad
- Agregar nuevas rutas: implementar handler y extender dispatcher.c.
- Reemplazar backend de event loop: implementar EventLoop con la misma API.
- Persistencia o lógica de negocio: agregar módulos en core/ y usarlos desde
  handlers.
