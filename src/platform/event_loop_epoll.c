#if defined(__linux__)

/*
 * event_loop_epoll.c — Implementación de EventLoop usando epoll (Linux).
 *
 * API uniforme (event_loop_*) con soporte de timers internos y parada cooperativa.
 */
#include "event_loop.h"
#include "platform.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EVENTS 64
#define MAX_FDS    1024
#define MAX_TIMERS 64

typedef struct FdHandler {
    int fd;
    EventCallback callback;
    void *user_data;
    EventType events;
    bool active;
} FdHandler;

typedef struct Timer {
    int id;
    uint64_t timeout_ms;
    bool periodic;
    TimerCallback callback;
    void *user_data;
    uint64_t next_fire;
    bool active;
} Timer;

struct EventLoop {
    int epoll_fd;
    bool running;
    FdHandler handlers[MAX_FDS];
    Timer timers[MAX_TIMERS];
    int next_timer_id;
};

/*
 * compute_wait_timeout
 * --------------------
 * Determina el timeout para epoll_wait combinando el próximo disparo de los
 * timers internos y el timeout solicitado para una ejecución de una sola vuelta.
 */
static int compute_wait_timeout(EventLoop *loop, int run_timeout_ms) {
    uint64_t now = platform_get_time_ms();
    int64_t ms_to_timer = -1;
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!loop->timers[i].active) continue;
        int64_t delta = (int64_t)loop->timers[i].next_fire - (int64_t)now;
        if (delta < 0) delta = 0;
        if (ms_to_timer < 0 || delta < ms_to_timer) ms_to_timer = delta;
    }
    if (run_timeout_ms >= 0 && ms_to_timer >= 0) return (run_timeout_ms < ms_to_timer) ? run_timeout_ms : (int)ms_to_timer;
    if (run_timeout_ms >= 0) return run_timeout_ms;
    if (ms_to_timer >= 0) return (int)ms_to_timer;
    return 1000;
}

/*
 * process_timers
 * --------------
 * Dispara callbacks de timers vencidos y reprograma los periódicos.
 */
static void process_timers(EventLoop *loop) {
    uint64_t now = platform_get_time_ms();
    for (int i = 0; i < MAX_TIMERS; i++) {
        Timer *t = &loop->timers[i];
        if (!t->active) continue;
        if (t->next_fire <= now) {
            t->callback(t->user_data);
            if (t->periodic) t->next_fire = now + t->timeout_ms; else t->active = false;
        }
    }
}

/*
 * event_loop_create
 * -----------------
 * Crea una instancia EventLoop basada en epoll.
 */
EventLoop *event_loop_create(void) {
    EventLoop *loop = calloc(1, sizeof(EventLoop));
    if (!loop) return NULL;
    loop->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (loop->epoll_fd < 0) { free(loop); return NULL; }
    loop->next_timer_id = 1;
    return loop;
}

/*
 * event_loop_destroy
 * ------------------
 * Cierra el epoll y libera memoria del EventLoop.
 */
void event_loop_destroy(EventLoop *loop) {
    if (!loop) return;
    if (loop->epoll_fd >= 0) close(loop->epoll_fd);
    free(loop);
}

/*
 * event_loop_add_fd
 * -----------------
 * Registra un FD en epoll con intereses de lectura/escritura.
 */
int event_loop_add_fd(EventLoop *loop, int fd, EventType events,
                      EventCallback callback, void *user_data) {
    if (!loop || fd < 0 || fd >= MAX_FDS || !callback) return PLATFORM_EINVAL;
    FdHandler *h = &loop->handlers[fd];
    h->fd = fd; h->callback = callback; h->user_data = user_data; h->events = events; h->active = true;
    struct epoll_event ev; memset(&ev, 0, sizeof(ev));
    if (events & EVENT_READ) ev.events |= EPOLLIN;
    if (events & EVENT_WRITE) ev.events |= EPOLLOUT;
    ev.data.ptr = h;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) { h->active = false; return PLATFORM_ERROR; }
    return PLATFORM_OK;
}

/*
 * event_loop_remove_fd
 * --------------------
 * Quita un FD previamente agregado.
 */
int event_loop_remove_fd(EventLoop *loop, int fd) {
    if (!loop || fd < 0 || fd >= MAX_FDS) return PLATFORM_EINVAL;
    FdHandler *h = &loop->handlers[fd];
    if (!h->active) return PLATFORM_OK;
    epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    h->active = false; return PLATFORM_OK;
}

/*
 * event_loop_modify_fd
 * --------------------
 * Modifica los intereses de un FD activo en epoll.
 */
int event_loop_modify_fd(EventLoop *loop, int fd, EventType events) {
    if (!loop || fd < 0 || fd >= MAX_FDS) return PLATFORM_EINVAL;
    FdHandler *h = &loop->handlers[fd];
    if (!h->active) return PLATFORM_EINVAL;
    struct epoll_event ev; memset(&ev, 0, sizeof(ev));
    if (events & EVENT_READ) ev.events |= EPOLLIN;
    if (events & EVENT_WRITE) ev.events |= EPOLLOUT;
    ev.data.ptr = h;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) return PLATFORM_ERROR;
    h->events = events; return PLATFORM_OK;
}

/*
 * event_loop_add_timer
 * --------------------
 * Crea un timer interno gestionado por el bucle. Retorna ID (>0) o -1.
 */
int event_loop_add_timer(EventLoop *loop, uint64_t timeout_ms,
                         bool periodic, TimerCallback callback, void *user_data) {
    if (!loop || !callback) return -1;
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!loop->timers[i].active) {
            loop->timers[i].id = loop->next_timer_id++;
            loop->timers[i].timeout_ms = timeout_ms;
            loop->timers[i].periodic = periodic;
            loop->timers[i].callback = callback;
            loop->timers[i].user_data = user_data;
            loop->timers[i].next_fire = platform_get_time_ms() + timeout_ms;
            loop->timers[i].active = true;
            return loop->timers[i].id;
        }
    }
    return -1;
}

/*
 * event_loop_remove_timer
 * -----------------------
 * Desactiva un timer por su identificador.
 */
void event_loop_remove_timer(EventLoop *loop, int timer_id) {
    if (!loop) return;
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (loop->timers[i].active && loop->timers[i].id == timer_id) { loop->timers[i].active = false; break; }
    }
}

/*
 * event_loop_run
 * --------------
 * Bucle principal usando epoll_wait. Despacha eventos y procesa timers.
 * Si timeout_ms >= 0, ejecuta una sola vuelta y retorna.
 */
int event_loop_run(EventLoop *loop, int timeout_ms) {
    if (!loop) return PLATFORM_EINVAL;
    loop->running = true;
    do {
        int wait_ms = compute_wait_timeout(loop, timeout_ms);
        struct epoll_event evs[MAX_EVENTS];
        int n = epoll_wait(loop->epoll_fd, evs, MAX_EVENTS, wait_ms);
        if (n < 0) {
            if (errno == EINTR) { process_timers(loop); if (timeout_ms >= 0) break; else continue; }
            return PLATFORM_ERROR;
        }
        for (int i = 0; i < n; i++) {
            FdHandler *h = (FdHandler *)evs[i].data.ptr;
            if (!h || !h->active) continue;
            EventType et = 0;
            if (evs[i].events & EPOLLIN)  et |= EVENT_READ;
            if (evs[i].events & EPOLLOUT) et |= EVENT_WRITE;
            if (evs[i].events & EPOLLERR) et |= EVENT_ERROR;
            h->callback(h->fd, et, h->user_data);
        }
        process_timers(loop);
        if (timeout_ms >= 0) break;
    } while (loop->running);
    return PLATFORM_OK;
}

/*
 * event_loop_stop
 * ---------------
 * Señala al bucle que debe detenerse.
 */
void event_loop_stop(EventLoop *loop) { if (loop) loop->running = false; }

/*
 * event_loop_is_running
 * ---------------------
 * Indica si el bucle está ejecutándose.
 */
bool event_loop_is_running(EventLoop *loop) { return loop ? loop->running : false; }

#endif // __linux__
