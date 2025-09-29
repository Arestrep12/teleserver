# TeleServer

Servidor concurrente CoAP sobre UDP en C.

## Características
- Codec CoAP conforme a RFC 7252 (perfil mínimo): decode/encode con extensiones 13/14
- Event loop multiplataforma (kqueue en macOS, epoll en Linux)
- Dispatcher + handlers de ejemplo: GET /hello, GET /time, POST /echo
- Servidor UDP integrado con event loop y codec

## Estructura
- include/: headers públicos (platform, event_loop, coap, codec, dispatcher, server)
- src/
  - platform/: sockets, utilidades, event loop por OS
  - coap/: utilidades de tipos, decode/encode
  - core/: dispatcher y handlers
  - server/: servidor UDP y main
  - clients/: reservado para cliente CLI (Fase 8)
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