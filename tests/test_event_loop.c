#include "event_loop.h"
#include "platform.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static int read_count = 0;
static int timer_count = 0;

static void on_read(int fd, EventType events, void *user_data) {
	(void)user_data;
	if (events & EVENT_READ) {
		char buf[8];
		// leer y descartar
		read(fd, buf, sizeof(buf));
		read_count++;
	}
}

static void on_timer(void *user_data) {
	(void)user_data;
	timer_count++;
}

static void test_create_destroy(void) {
	EventLoop *loop = event_loop_create();
	assert(loop != NULL);
	event_loop_destroy(loop);
	printf("✓ test_create_destroy\n");
}

static void test_add_remove_fd(void) {
	EventLoop *loop = event_loop_create();
	assert(loop != NULL);

	int pipes[2];
	assert(pipe(pipes) == 0);

	int rc = event_loop_add_fd(loop, pipes[0], EVENT_READ, on_read, NULL);
	assert(rc == 0);
	rc = event_loop_remove_fd(loop, pipes[0]);
	assert(rc == 0);

	close(pipes[0]);
	close(pipes[1]);
	event_loop_destroy(loop);
	printf("✓ test_add_remove_fd\n");
}

static void test_timer(void) {
	EventLoop *loop = event_loop_create();
	assert(loop != NULL);

	timer_count = 0;
	int tid = event_loop_add_timer(loop, 10, false, on_timer, NULL);
	assert(tid > 0);

	// single iteration ~20ms
	event_loop_run(loop, 20);
	assert(timer_count == 1);
	event_loop_destroy(loop);
	printf("✓ test_timer\n");
}

static void test_read_event_once(void) {
	EventLoop *loop = event_loop_create();
	assert(loop != NULL);

	int pipes[2];
	assert(pipe(pipes) == 0);
	// non-block en read end
	int flags = fcntl(pipes[0], F_GETFL, 0);
	fcntl(pipes[0], F_SETFL, flags | O_NONBLOCK);

	read_count = 0;
	int rc = event_loop_add_fd(loop, pipes[0], EVENT_READ, on_read, NULL);
	assert(rc == 0);

	// Escribir algo para disparar el evento
	const char *msg = "x";
	write(pipes[1], msg, 1);

	event_loop_run(loop, 50);
	assert(read_count >= 1);

	event_loop_remove_fd(loop, pipes[0]);
	close(pipes[0]);
	close(pipes[1]);
	event_loop_destroy(loop);
	printf("✓ test_read_event_once\n");
}

int main(void) {
	printf("=== Tests de event loop ===\n");
	platform_init();

	test_create_destroy();
	test_add_remove_fd();
	test_timer();
	test_read_event_once();

	platform_cleanup();
	printf("✓ Todos los tests de event loop pasaron\n");
	return 0;
}
