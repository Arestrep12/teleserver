#ifndef TIME_SOURCE_H
#define TIME_SOURCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t (*NowMsFn)(void);

typedef struct {
    NowMsFn now_ms;
} TimeSource;

// Establece una fuente de tiempo. Pasar NULL restaura la fuente por defecto.
void time_source_set(TimeSource *ts);

// Obtiene el tiempo actual en ms usando la fuente configurada (o la por defecto).
uint64_t time_source_now_ms(void);

#ifdef __cplusplus
}
#endif

#endif // TIME_SOURCE_H