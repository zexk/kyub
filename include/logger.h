#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>

typedef enum {
    LOG_ERROR = 0,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
    LOG_LEVEL_COUNT
} LogLevel;

typedef enum {
    CAT_WORLD = 0,
    CAT_GL,
    CAT_UI,
    CAT_INPUT,
    CAT_PLATFORM,
    CAT_COUNT
} LogCategory;

void logger_init(const char *filename);
void logger_shutdown(void);
void logger_set_level(LogLevel level);
void logger_log(LogLevel level, LogCategory cat, const char *fmt, ...);

#ifdef ENABLE_LOGGER
#define LOG_ERROR(cat, ...)   logger_log(LOG_ERROR, cat, __VA_ARGS__)
#define LOG_WARN(cat, ...)    logger_log(LOG_WARN, cat, __VA_ARGS__)
#define LOG_INFO(cat, ...)    logger_log(LOG_INFO, cat, __VA_ARGS__)
#define LOG_DEBUG(cat, ...)   logger_log(LOG_DEBUG, cat, __VA_ARGS__)
#else
#define LOG_ERROR(cat, ...)    do {} while(0)
#define LOG_WARN(cat, ...)     do {} while(0)
#define LOG_INFO(cat, ...)     do {} while(0)
#define LOG_DEBUG(cat, ...)    do {} while(0)
#endif

#endif // LOGGER_H
