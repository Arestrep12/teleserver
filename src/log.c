#include "log.h"

#include <time.h>
#include <string.h>

static LogLevel g_level = LOG_LEVEL_INFO; // por defecto: INFO
static FILE *g_stream = NULL;             // NULL => usar stderr

void log_set_level(LogLevel level) {
    g_level = level;
}

void log_set_stream(FILE *stream) {
    g_stream = stream;
}

static const char *level_to_str(LogLevel lvl) {
    switch (lvl) {
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        default: return "LOG";
    }
}

void log_printf(LogLevel level, const char *fmt, ...) {
    if (level > g_level) return;
    FILE *out = g_stream ? g_stream : stderr;

    // Prefijo simple con nivel
    fprintf(out, "[%s] ", level_to_str(level));

    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
}