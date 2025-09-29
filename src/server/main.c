#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "server.h"
#include "platform.h"
#include "log.h"

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [--port N] [--verbose]\n", prog);
}

int main(int argc, char *argv[]) {
    uint16_t port = 5683;
    bool verbose = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            int p = atoi(argv[++i]);
            if (p < 0 || p > 65535) {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
            port = (uint16_t)p;
        } else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    platform_init();

    Server *srv = server_create(port, verbose);
    if (!srv) {
        fprintf(stderr, "Failed to create server on port %u\n", (unsigned)port);
        return EXIT_FAILURE;
    }

    if (verbose) {
        LOG_INFO("TeleServer running on UDP/%u\n", (unsigned)server_get_port(srv));
    }

    // Ejecutar hasta que el proceso sea terminado externamente
    (void)server_run(srv, -1);

    server_destroy(srv);
    platform_cleanup();
    return EXIT_SUCCESS;
}
