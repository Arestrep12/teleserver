# TeleServer — Plan de Desarrollo

Este plan detalla las fases, entregables y criterios de aceptación para construir
un servidor CoAP concurrente en C sobre UDP, conforme a las reglas de WARP.md.

Fuentes de verdad:
- WARP.md (arquitectura, reglas C, comandos de desarrollo y despliegue)
- Este documento (plan operativo, checklist, estado)

Principios y restricciones
- Solo API POSIX de sockets (sin librerías de terceros para CoAP ni frameworks)
- C11, -Wall -Wextra -Werror; debug con ASan/UBSan; 4 espacios de indentación
- macOS (desarrollo): kqueue; Linux/AWS: epoll; fallback poll/select si hiciera falta
- Arquitectura por módulos: platform/, coap/, core/, server/, clients/

Estado actual (2025-09-29)
- Fase 1 (Infraestructura de build): COMPLETADA
- Fase 2 (Plataforma/sockets): COMPLETADA
- Fase 3 (Event loop kqueue/epoll): COMPLETADA
- Fase 4 (Tipos y constantes CoAP): COMPLETADA
- Fase 5 (Codec CoAP decode/encode): COMPLETADA
- Fase 6 (Núcleo/dispatcher + handlers): COMPLETADA
- Fase 7 (Servidor UDP principal — integración): COMPLETADA
- Fase 8 (Cliente CoAP mínimo — CLI): COMPLETADA
- Fase 9 (Logging y observabilidad): COMPLETADA
- Fase 0 (Kickoff y convenciones): COMPLETADA

Checklist de progreso
- [x] Fase 0: Kickoff y convenciones
- [x] Fase 1: Makefile y sistema de build
- [x] Fase 2: Wrappers de plataforma/sockets + tests
- [x] Fase 3: Event loop (kqueue/epoll)
- [x] Fase 4: Tipos y constantes CoAP (RFC 7252)
- [x] Fase 5: Codec CoAP (decode/encode)
- [x] Fase 6: Núcleo/dispatcher + handlers
- [x] Fase 7: Servidor UDP principal (integración)
- [x] Fase 8: Cliente CoAP mínimo (CLI)
- [x] Fase 9: Logging y observabilidad

---

Fase 0 — Kickoff y convenciones
Objetivo
- Establecer estructura del proyecto y convenciones de desarrollo (formato/lint).
Entregables
- Estructura: include/, src/(platform|coap|core|server|clients), tests/, docs/, bin/, build/
- .gitignore, LICENSE (MIT u otra), README.md, .clang-format, .clang-tidy
Pasos clave
- Crear carpetas y archivos base; documentar estructura y comandos
Verificación
- ls/tree muestran estructura y archivos; revisión del README
Criterios de aceptación
- Convenciones definidas y versionadas; no se trackean binarios ni build

Fase 1 — Infraestructura de build (COMPLETADA)
Objetivo
- Makefile con targets: debug, release, test, lint, format, clean
Entregables
- Makefile con detección de SO (Linux/macOS) y compilación incremental
Verificación
- make debug/release/test ejecutan correctamente; sin warnings (por -Werror)

Fase 2 — Plataforma y sockets (COMPLETADA)
Objetivo
- Wrappers seguros POSIX para UDP no bloqueante y utilidades de tiempo/errores
Entregables
- include/platform.h, src/platform/socket.c, src/platform/utils.c
- tests/test_platform.c
Verificación
- make test (todos los tests verdes con sanitizers)

Fase 3 — Event loop multiplataforma (kqueue/epoll/select)
Objetivo
- Bucle de eventos no bloqueante con timers y callbacks, independiente del OS
Entregables
- include/event_loop.h
- src/platform/event_loop_kqueue.c (macOS)
- src/platform/event_loop_epoll.c (Linux)
- tests/test_event_loop.c
Pasos clave
- API: create/destroy, add/remove/modify fd; add/remove timer; run/stop/is_running
- Timers in-process sencillos (array fijo)
Verificación
- Tests: alta/baja fd; timer; una iteración con timeout
Criterios de aceptación
- kqueue operativo en macOS; epoll operativo en Linux; sin bloqueos ni spin

Fase 4 — Tipos y constantes CoAP (RFC 7252)
Objetivo
- Definir estructuras/constantes y helpers de CoAP compatibles con perfil mínimo
Entregables
- include/coap.h; src/coap/utils.c; tests/test_coap_types.c
Pasos clave
- CoapMessage, opciones, content-format; helpers: class/detail, to_string
Verificación
- Tests de tipos y helpers
Criterios de aceptación
- Límites: token ≤ 8; opciones ordenadas; payload marker 0xFF

Fase 5 — Codec CoAP (decode/encode)
Objetivo
- Decodificador/codificador robustos con validación (extensiones 13/14)
Entregables
- include/coap_codec.h; src/coap/decode.c; src/coap/encode.c; tests/test_coap_codec.c
Verificación
- Round-trip encode→decode estable; rechazo de casos inválidos
Criterios de aceptación
- Tests con sanitizers pasan; sin overflows/UB

Fase 6 — Núcleo/dispatcher + handlers
Objetivo
- Pipeline validación → routing → handler → respuesta (GET /hello, /time; POST /echo)
Entregables
- include/dispatcher.h, include/handlers.h; src/core/dispatcher.c, src/core/handlers.c
- tests/test_dispatcher.c
Verificación
- Respuestas 2.xx/4.xx correctas; logs legibles

Fase 7 — Servidor UDP principal (integración)
Objetivo
- Integrar event loop + sockets + codec + dispatcher
Entregables
- include/server.h; src/server/server.c; src/server/main.c (flags: --port, --verbose)
- tests/test_server_integration.c
Verificación
- GET /hello (2.05) y POST /echo funcionales con sanitizers

Fase 8 — Cliente CoAP mínimo (CLI)
Objetivo
- Herramienta CLI para GET/POST con reintentos y timeout
Entregables
- include/client.h; src/clients/client.c; src/clients/cli.c
Verificación
- Pruebas manuales: get /hello; post /echo "mensaje"

Fase 9 — Logging y observabilidad
Objetivo
- Macro/log con niveles (ERROR/WARN/INFO/DEBUG) controlable por build/flag
Entregables
- include/log.h (+ posible src/log.c) o macros
Verificación
- Logs limpios; niveles respetados; silencioso en release


---

Riesgos y mitigaciones
- Desbordes de buffer: uso de tamaños acotados, memcpy/snprintf con límites, tests
- Bloqueos: sockets non-blocking + event loop; evitar operaciones bloqueantes en callbacks
- Portabilidad: limitarse a C11/POSIX; rutas separadas por OS (kqueue/epoll)
- Seguridad: validar tamaños y punteros; no confiar en input de red; sin UB

