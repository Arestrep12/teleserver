# TeleServer – Documentación

Bienvenido a la documentación del proyecto TeleServer. Aquí encontrarás una
explicación amplia y granular de la arquitectura, los módulos, la API pública,
las decisiones de diseño, cómo compilar/ejecutar y cómo validar el sistema con
pruebas.

Tabla de contenidos
- Arquitectura general: architecture.md
- Referencia de API por headers: reference.md
- Módulos (explicación detallada):
  - modules/server.md
  - modules/dispatcher.md
  - modules/handlers.md
  - modules/coap.md
  - modules/event_loop.md
  - modules/platform.md
  - modules/time_source.md
  - modules/logging.md
- Protocolo CoAP (modelo soportado): coap_protocol.md
- Compilación y ejecución: build.md
- Pruebas y cobertura: testing.md

Resumen rápido
- TeleServer es un servidor UDP que implementa un perfil mínimo de CoAP (RFC 7252).
- Usa un event loop portado a macOS (kqueue) y Linux (epoll) para E/S no bloqueante.
- El pipeline es: socket UDP -> decode CoAP -> dispatcher -> handler -> encode -> send.
- Existen pruebas unitarias, integración y end-to-end (con TeleClient) con sanitizers.

Convenciones
- C11, advertencias elevadas (-Wall -Wextra -Werror), 4 espacios.
- Validación rigurosa de límites y precondiciones.
- Tokens y Message ID se preservan (mirror) en respuestas.

Lectura sugerida
1) architecture.md para la vista global.
2) modules/coap.md y coap_protocol.md para entender el formato de mensajes.
3) modules/server.md y modules/dispatcher.md para el flujo de petición.
4) testing.md para reproducir el set de pruebas.
