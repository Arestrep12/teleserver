# Compilación y ejecución

Requisitos
- Compilador C (clang o gcc), make.
- macOS o Linux. En macOS se utiliza kqueue; en Linux, epoll.
- Opcional: clang-format, clang-tidy, cppcheck.

Objetivos make
- debug (por defecto): símbolos + sanitizers (ASan/UBSan)
  - Ejecuta: `make debug`
  - Binario: `bin/tele_server_debug`
- release: optimizado
  - Ejecuta: `make release`
  - Binario: `bin/tele_server`
- test: compila y ejecuta pruebas
  - Ejecuta: `make test`
- format: aplica clang-format a fuentes y headers
- lint: ejecuta clang-tidy y cppcheck (si están instalados)
- clean: limpia build/ y bin/

Ejecución del servidor
- Ejemplo:
```
./bin/tele_server_debug --port 5683 --verbose
```
- Parámetros:
  - --port N (uint16): puerto UDP (0 = efímero, el puerto efectivo se imprime con --verbose)
  - --verbose: activa logs de INFO

Notas de plataforma
- macOS: se usa event_loop_kqueue.c; ver `PLATFORM_MACOS` en platform.h.
- Linux: se usa event_loop_epoll.c; ver `PLATFORM_LINUX`.

Troubleshooting
- "Address already in use": asegúrate de usar SO_REUSEADDR (ya habilitado) o cambia de puerto.
- "Operation would block": esperado con sockets no bloqueantes; el server reintenta en el próximo evento.
- Sanitizers detectan accesos inválidos o UB en tiempo de ejecución.
