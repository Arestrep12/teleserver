# Plataforma (platform/*.c, include/platform.h)

Objetivo
- Ofrecer utilidades de plataforma y un envoltorio del API de sockets con
  códigos de retorno unificados.

Funciones clave
- platform_init/cleanup: hooks de inicialización/limpieza (actualmente loguea la
  plataforma en uso).
- platform_get_time_ms: gettimeofday a milisegundos.
- platform_error_string: traducción de códigos PLATFORM_* a texto.
- Sockets:
  - platform_socket_create_udp: crea socket UDP y reporta error con LOG_ERROR.
  - platform_socket_bind: bind en INADDR_ANY y puerto dado.
  - platform_socket_set_nonblocking: configura O_NONBLOCK con fcntl.
  - platform_socket_set_reuseaddr: SO_REUSEADDR.
  - platform_socket_recvfrom: mapea EAGAIN/EWOULDBLOCK a PLATFORM_EAGAIN.
  - platform_socket_sendto: idem; registra warn en otros errores.

Consideraciones
- No se usan asignaciones dinámicas en la ruta de I/O.
- Los errores de sistema se traducen a códigos uniformes y se registran con LOG_*.

Integración
- server.c consume exclusivamente esta API para E/S de red.
