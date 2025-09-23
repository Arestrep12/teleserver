#if defined(__APPLE__) && defined(__MACH__)

#include "event_loop.h"
#include "platform.h"
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

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
	int kqueue_fd;
	bool running;
	FdHandler handlers[MAX_FDS];
	Timer timers[MAX_TIMERS];
	int next_timer_id;
};

static void compute_timespec_for_wait(EventLoop *loop, int run_timeout_ms,
                                     struct timespec *out_ts, struct timespec **out_pts) {
	// Calcula el timeout a usar considerando timers pendientes
	uint64_t now = platform_get_time_ms();
	int64_t ms_to_timer = -1; // -1 => no timers activos
	for (int i = 0; i < MAX_TIMERS; i++) {
		if (!loop->timers[i].active) continue;
		int64_t delta = (int64_t)loop->timers[i].next_fire - (int64_t)now;
		if (delta < 0) delta = 0;
		if (ms_to_timer < 0 || delta < ms_to_timer) {
			ms_to_timer = delta;
		}
	}

	int use_ms;
	if (run_timeout_ms >= 0 && ms_to_timer >= 0)
		use_ms = (run_timeout_ms < ms_to_timer) ? run_timeout_ms : (int)ms_to_timer;
	else if (run_timeout_ms >= 0)
		use_ms = run_timeout_ms;
	else if (ms_to_timer >= 0)
		use_ms = (int)ms_to_timer;
	else
		use_ms = 1000; // default 1s si no hay timers ni timeout de corrida

	out_ts->tv_sec = use_ms / 1000;
	out_ts->tv_nsec = (use_ms % 1000) * 1000000L;
	*out_pts = out_ts;
}

EventLoop *event_loop_create(void) {
	EventLoop *loop = (EventLoop *)calloc(1, sizeof(EventLoop));
	if (!loop) return NULL;

	loop->kqueue_fd = kqueue();
	if (loop->kqueue_fd < 0) {
		free(loop);
		return NULL;
	}
	loop->next_timer_id = 1;
	return loop;
}

void event_loop_destroy(EventLoop *loop) {
	if (!loop) return;
	if (loop->kqueue_fd >= 0) close(loop->kqueue_fd);
	free(loop);
}

int event_loop_add_fd(EventLoop *loop, int fd, EventType events,
                      EventCallback callback, void *user_data) {
	if (!loop || fd < 0 || fd >= MAX_FDS || !callback) return PLATFORM_EINVAL;

	FdHandler *h = &loop->handlers[fd];
	h->fd = fd;
	h->callback = callback;
	h->user_data = user_data;
	h->events = events;
	h->active = true;

	struct kevent changes[2];
	int n = 0;
	if (events & EVENT_READ)  EV_SET(&changes[n++], fd, EVFILT_READ,  EV_ADD | EV_ENABLE, 0, 0, h);
	if (events & EVENT_WRITE) EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, h);
	if (kevent(loop->kqueue_fd, changes, n, NULL, 0, NULL) < 0) {
		h->active = false;
		return PLATFORM_ERROR;
	}
	return PLATFORM_OK;
}

int event_loop_remove_fd(EventLoop *loop, int fd) {
	if (!loop || fd < 0 || fd >= MAX_FDS) return PLATFORM_EINVAL;
	FdHandler *h = &loop->handlers[fd];
	if (!h->active) return PLATFORM_OK;

	struct kevent changes[2];
	int n = 0;
	if (h->events & EVENT_READ)  EV_SET(&changes[n++], fd, EVFILT_READ,  EV_DELETE, 0, 0, NULL);
	if (h->events & EVENT_WRITE) EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	kevent(loop->kqueue_fd, changes, n, NULL, 0, NULL);
	h->active = false;
	return PLATFORM_OK;
}

int event_loop_modify_fd(EventLoop *loop, int fd, EventType events) {
	if (!loop || fd < 0 || fd >= MAX_FDS) return PLATFORM_EINVAL;
	FdHandler *h = &loop->handlers[fd];
	if (!h->active) return PLATFORM_EINVAL;
	// El enfoque simple: eliminar y volver a agregar
	int rc = event_loop_remove_fd(loop, fd);
	if (rc != PLATFORM_OK) return rc;
	return event_loop_add_fd(loop, fd, events, h->callback, h->user_data);
}

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

void event_loop_remove_timer(EventLoop *loop, int timer_id) {
	if (!loop) return;
	for (int i = 0; i < MAX_TIMERS; i++) {
		if (loop->timers[i].active && loop->timers[i].id == timer_id) {
			loop->timers[i].active = false;
			break;
		}
	}
}

static void process_timers(EventLoop *loop) {
	uint64_t now = platform_get_time_ms();
	for (int i = 0; i < MAX_TIMERS; i++) {
		Timer *t = &loop->timers[i];
		if (!t->active) continue;
		if (t->next_fire <= now) {
			t->callback(t->user_data);
			if (t->periodic) {
				t->next_fire = now + t->timeout_ms;
			} else {
				t->active = false;
			}
		}
	}
}

int event_loop_run(EventLoop *loop, int timeout_ms) {
	if (!loop) return PLATFORM_EINVAL;
	loop->running = true;

	do {
		struct timespec ts; struct timespec *pts = NULL;
		compute_timespec_for_wait(loop, timeout_ms, &ts, &pts);

		struct kevent events[MAX_EVENTS];
		int n = kevent(loop->kqueue_fd, NULL, 0, events, MAX_EVENTS, pts);
		if (n < 0) {
			if (errno == EINTR) { process_timers(loop); if (timeout_ms >= 0) break; else continue; }
			return PLATFORM_ERROR;
		}

		for (int i = 0; i < n; i++) {
			struct kevent *ev = &events[i];
			FdHandler *h = (FdHandler *)ev->udata;
			if (!h || !h->active) continue;

			EventType et = 0;
			if (ev->filter == EVFILT_READ)  et |= EVENT_READ;
			if (ev->filter == EVFILT_WRITE) et |= EVENT_WRITE;
			if (ev->flags & EV_ERROR)       et |= EVENT_ERROR;
			h->callback(h->fd, et, h->user_data);
		}

		process_timers(loop);
		if (timeout_ms >= 0) break; // single-iteration mode
	} while (loop->running);

	return PLATFORM_OK;
}

void event_loop_stop(EventLoop *loop) {
	if (loop) loop->running = false;
}

bool event_loop_is_running(EventLoop *loop) {
	return loop ? loop->running : false;
}

#endif // __APPLE__ && __MACH__
