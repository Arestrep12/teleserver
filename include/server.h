#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>
#include <stdint.h>

// Tipo opaco del servidor
typedef struct Server Server;

// Crea el servidor y lo enlaza al puerto indicado (0 = efímero)
// Retorna NULL en error.
Server *server_create(uint16_t port, bool verbose);

// Libera recursos del servidor
void server_destroy(Server *srv);

// Ejecuta el event loop
// - run_timeout_ms < 0 => corre hasta server_stop()
// - run_timeout_ms >= 0 => procesa una iteración con ese timeout
int server_run(Server *srv, int run_timeout_ms);

// Señal para detener el loop (si está corriendo en modo infinito)
void server_stop(Server *srv);

// Puerto efectivamente enlazado por el socket del servidor
uint16_t server_get_port(const Server *srv);

#endif // SERVER_H