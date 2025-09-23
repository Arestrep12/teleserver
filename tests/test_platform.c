#include "platform.h"
#include <assert.h>
#include <stdio.h>

static void test_socket_creation(void) {
	int sock = platform_socket_create_udp();
	assert(sock > 0);
	platform_socket_close(sock);
	printf("✓ test_socket_creation\n");
}

static void test_socket_bind(void) {
	int sock = platform_socket_create_udp();
	assert(sock > 0);

	int result = platform_socket_bind(sock, 15683); // Puerto de prueba
	assert(result == PLATFORM_OK);

	platform_socket_close(sock);
	printf("✓ test_socket_bind\n");
}

static void test_nonblocking(void) {
	int sock = platform_socket_create_udp();
	assert(sock > 0);

	int result = platform_socket_set_nonblocking(sock);
	assert(result == PLATFORM_OK);

	platform_socket_close(sock);
	printf("✓ test_nonblocking\n");
}

static void test_time(void) {
	uint64_t t1 = platform_get_time_ms();
	uint64_t t2 = platform_get_time_ms();
	assert(t2 >= t1);
	printf("✓ test_time\n");
}

int main(void) {
	printf("=== Tests de plataforma ===\n");
	platform_init();

	test_socket_creation();
	test_socket_bind();
	test_nonblocking();
	test_time();

	platform_cleanup();
	printf("✓ Todos los tests de plataforma pasaron\n");
	return 0;
}
