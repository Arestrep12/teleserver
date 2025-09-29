#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN  = 1,
    LOG_LEVEL_INFO  = 2,
    LOG_LEVEL_DEBUG = 3
} LogLevel;

void log_set_level(LogLevel level);
void log_set_stream(FILE *stream);
void log_printf(LogLevel level, const char *fmt, ...);

#define LOG_ERROR(...) log_printf(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_WARN(...)  log_printf(LOG_LEVEL_WARN,  __VA_ARGS__)
#define LOG_INFO(...)  log_printf(LOG_LEVEL_INFO,  __VA_ARGS__)
#define LOG_DEBUG(...) log_printf(LOG_LEVEL_DEBUG, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // LOG_H