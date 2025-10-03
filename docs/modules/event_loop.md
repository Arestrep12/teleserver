# Event loop (event_loop_*.c, include/event_loop.h)

Visión general
- Abstracción de bucle de eventos con API uniforme, respaldada por:
  - macOS: kqueue (event_loop_kqueue.c)
  - Linux: epoll (event_loop_epoll.c)
- Soporta:
  - Monitoreo de FDs para lectura/escritura.
  - Timers one‑shot y periódicos.
  - Ejecución single‑thread (sin locking interno).

API principal
- event_loop_add_fd: registra FD y callback; se invoca con máscara de eventos.
- event_loop_add_timer: ejecuta TimerCallback al expirar; si periodic=true,
  reprograma automáticamente.
- event_loop_run:
  - timeout_ms < 0: corre hasta event_loop_stop().
  - timeout_ms >= 0: procesa una iteración y retorna.

Timers y cómputo de timeouts
- El backend calcula el próximo vencimiento de cualquier timer y ajusta el
  timeout de kevent/epoll_wait para despertar a tiempo.
- Al finalizar cada iteración se procesa process_timers() para disparar callbacks.

Diferencias backend
- kqueue: EVFILT_READ/EVFILT_WRITE, struct kevent con user_data (udata) apuntando
  al handler. Se usan EV_ADD/EV_ENABLE/EV_DELETE.
- epoll: EPOLLIN/EPOLLOUT, data.ptr con puntero al handler. Modificación con
  EPOLL_CTL_MOD.

Patrones de uso
- Registrar socket UDP con EVENT_READ y callback que haga recvfrom hasta EAGAIN.
- Usar timers para tareas periódicas (p. ej., mantenimiento/timeout housekeeping).

Límites
- MAX_FDS/MAX_EVENTS/MAX_TIMERS definidos en cada backend; ajustar si el proyecto
  crece en cantidad de descriptores.
