#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <stdbool.h>
#include <stdint.h>

// Tipos de eventos (bitmask)
typedef enum {
	EVENT_READ  = 0x01,
	EVENT_WRITE = 0x02,
	EVENT_ERROR = 0x04
} EventType;

// Callback para FD
typedef void (*EventCallback)(int fd, EventType events, void *user_data);

// Callback para timer
typedef void (*TimerCallback)(void *user_data);

// Estructura opaca del event loop
typedef struct EventLoop EventLoop;

// API del event loop
EventLoop *event_loop_create(void);
void event_loop_destroy(EventLoop *loop);

int event_loop_add_fd(EventLoop *loop, int fd, EventType events,
					  EventCallback callback, void *user_data);
int event_loop_remove_fd(EventLoop *loop, int fd);
int event_loop_modify_fd(EventLoop *loop, int fd, EventType events);

int event_loop_add_timer(EventLoop *loop, uint64_t timeout_ms,
						  bool periodic, TimerCallback callback, void *user_data);
void event_loop_remove_timer(EventLoop *loop, int timer_id);

// Ejecuta el loop:
// - timeout_ms < 0 => corre hasta event_loop_stop()
// - timeout_ms >= 0 => procesa una iteración con ese timeout y retorna
int event_loop_run(EventLoop *loop, int timeout_ms);
void event_loop_stop(EventLoop *loop);
bool event_loop_is_running(EventLoop *loop);

#endif // EVENT_LOOP_H
