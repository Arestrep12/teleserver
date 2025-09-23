# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

Objetivo del repositorio
- Servidor concurrente en C desplegable en AWS, sin dependencias de runtime
  aparte de la API de sockets POSIX. Protocolo principal: CoAP sobre UDP.
  Dispositivos Arduino enviarán peticiones directamente por IP:puerto. El
  servidor responderá a clientes usando un CoAP "personalizado" (idéntico en
  lo esencial al estándar, pero sin campos opcionales en la request).
  El código debe seguir buenas prácticas de C conforme a
  <rule:velvKjFt2ajcCFxIMG8cFJ>.

Arquitectura de alto nivel (propuesta)
- Transporte UDP + bucle de eventos no bloqueante:
  - Linux/AWS: epoll
  - macOS (desarrollo): kqueue
  - Fallback de portabilidad: poll/select
- Módulos principales (orientativo, no exhaustivo):
  - platform/: abstracciones de I/O (epoll/kqueue/select) y utilidades de sockets
  - coap/: codec (parseo/serialización) para el perfil mínimo de CoAP; validación
    estricta; sin opcionales en requests salientes personalizadas
  - core/: dispatcher de mensajes y manejadores; pipeline validación → routing →
    handler → respuesta
  - server/: inicialización, configuración, logging, workers/concurrencia
  - clients/: emisor del CoAP personalizado hacia clientes
- Concurrencia
  - Sockets no bloqueantes + event loop
  - Pool de hilos para handlers CPU‑bound si es necesario
- Flujo de datos
  1) UDP datagrama entrante → 2) coap_decode → 3) dispatcher/handler →
     4) coap_encode → 5) sendto()

Convenciones y reglas C
- Seguir estrictamente <rule:velvKjFt2ajcCFxIMG8cFJ>. Resumen clave:
  - Estilo: 4 espacios; llaves en la misma línea; nombres: lower_snake_case para
    funciones/variables, UPPER_SNAKE_CASE para constantes, PascalCase para
    structs/enums/typedefs.
  - Seguridad: inicializar variables, validar punteros, comprobar todos los
    códigos de retorno, evitar desbordes, preferir snprintf/strncpy, prohibidas
    gets/strcpy/scanf sin width.
  - Compilación: -std=c11 -Wall -Wextra -Werror; usar -g en debug; considerar
    ASan/UBSan.
  - Depuración: assert, gdb/lldb; sanitizers cuando sea posible.
  - Portabilidad: C11/POSIX, evitar comportamiento indefinido.

Comandos de desarrollo
- Prerrequisitos
  - Toolchain: clang o gcc, make. Opcional: clang-tidy, cppcheck, valgrind (Linux).

- Compilar (sin Makefile aún)
  - Debug (ASan/UBSan):
    clang -std=c11 -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer \
      -Wall -Wextra -Werror -Iinclude \
      -o bin/tele_server $(find src -name '*.c')
  - Release:
    clang -std=c11 -O2 -DNDEBUG -Wall -Wextra -Werror -Iinclude \
      -o bin/tele_server $(find src -name '*.c')
  - Nota: con gcc, sustituir clang por gcc (flags equivalentes).

- Ejecutar el servidor
  - ./bin/tele_server  # por defecto escuchará UDP/5683 (CoAP) si así se define.

- Linting
  - clang-tidy (sin compile_commands.json):
    clang-tidy -checks='*' src/**/*.c -- -std=c11 -Iinclude
  - cppcheck:
    cppcheck --enable=all --inconclusive --std=c11 \
      --suppress=missingIncludeSystem src include

- Tests (sin framework externo)
  - Organización: tests/test_*.c usando assert.h; binarios independientes.
  - Ejecutar todas:
    for f in tests/test_*.c; do \
      clang -std=c11 -g -Wall -Wextra -Werror -Iinclude \
        -o build/tests/$(basename "${f%.c}") "$f" $(find src -name '*.c') && \
        ./build/tests/$(basename "${f%.c}"); \
    done
  - Ejecutar una sola prueba:
    TEST=tests/test_coap.c; \
    clang -std=c11 -g -Wall -Wextra -Werror -Iinclude \
      -o build/tests/test_coap "$TEST" $(find src -name '*.c') && \
      ./build/tests/test_coap

- Formato (opcional)
  - clang-format -i $(find src include -name '*.[ch]')

AWS (operativo mínimo)
- Servicio UDP (CoAP). Abrir el puerto UDP correspondiente (por defecto 5683)
  en el Security Group del recurso que lo exponga (EC2/NLB/ALB compatible).
- Despliegue típico:
  - Compilar release en una distro compatible (p.ej., Amazon Linux 2).
  - Transferir binario y ejecutar como servicio (systemd) con límites de fds
    adecuados y Restart=on-failure.
- UDP no tiene conexión: gestionar timeouts/reintentos a nivel de protocolo si
  aplica.

Estructura prevista (breve)
- include/: headers públicos
- src/: platform/, coap/, core/, server/, clients/
- tests/: pruebas unitarias sin dependencias externas
- bin/, build/: artefactos generados
- docs/: especificación del "CoAP personalizado" (añadir cuando esté lista)

Notas
- Sólo API de sockets; no usar librerías de terceros para CoAP ni frameworks de
  servidor.
- El "CoAP personalizado" debe mantener compatibilidad semántica básica con la
  RFC 7252, omitiendo opcionales en las peticiones salientes. Documentar en docs/
  cuando esté disponible.
