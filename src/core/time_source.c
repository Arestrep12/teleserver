#include "time_source.h"
#include "platform.h"

static TimeSource *g_ts = 0;

static uint64_t default_now_ms(void) {
    return platform_get_time_ms();
}

uint64_t time_source_now_ms(void) {
    if (g_ts && g_ts->now_ms) return g_ts->now_ms();
    return default_now_ms();
}

void time_source_set(TimeSource *ts) {
    g_ts = ts;
}