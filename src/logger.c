#define _POSIX_C_SOURCE 199309L
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include "logger.h"
#include "common.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define LOG_MAX_FILE_SIZE (10 * 1024 * 1024)

static FILE *g_log_file = NULL;
static LogLevel g_log_level = LOG_WARN;
static char g_filename[256] = {0};

static const char* category_names[CAT_COUNT] = {
    "WORLD",
    "GL",
    "UI",
    "INPUT",
    "PLATFORM"
};

static const char* level_names[LOG_LEVEL_COUNT] = {
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG"
};

static LogLevel parse_log_level(const char *val) {
    if (!val) return LOG_WARN;
    if (strcmp(val, "debug") == 0 || strcmp(val, "DEBUG") == 0) return LOG_DEBUG;
    if (strcmp(val, "info") == 0 || strcmp(val, "INFO") == 0) return LOG_INFO;
    if (strcmp(val, "warn") == 0 || strcmp(val, "WARN") == 0) return LOG_WARN;
    if (strcmp(val, "error") == 0 || strcmp(val, "ERROR") == 0) return LOG_ERROR;
    return LOG_WARN;
}

void logger_init(const char *filename) {
    const char *env = getenv("KYUB_LOG");
    g_log_level = parse_log_level(env);

    if (!filename) return;

    strncpy(g_filename, filename, sizeof(g_filename) - 1);
    g_filename[sizeof(g_filename) - 1] = '\0';

    g_log_file = fopen(g_filename, "a");
    if (!g_log_file) {
        fprintf(stderr, "[logger] Failed to open log file: %s\n", filename);
        return;
    }

    time_t now = time(NULL);
    fprintf(g_log_file, "\n=== Logger initialized at %s", ctime(&now));
    fprintf(g_log_file, "    KYUB_LOG=%s  level=%s\n",
            env ? env : "(unset)", level_names[g_log_level]);
    fflush(g_log_file);
}

void logger_shutdown(void) {
    if (g_log_file) {
        time_t now = time(NULL);
        fprintf(g_log_file, "=== Logger shutdown at %s", ctime(&now));
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

void logger_set_level(LogLevel level) {
    if (level >= 0 && level < LOG_LEVEL_COUNT) {
        g_log_level = level;
    }
}

void logger_log(LogLevel level, LogCategory cat, const char *fmt, ...) {
    if (level > g_log_level || level < 0 || level >= LOG_LEVEL_COUNT) return;
    if (cat < 0 || cat >= CAT_COUNT) cat = CAT_COUNT - 1;

    time_t sec;
    int ms;
#if defined(_WIN32)
    SYSTEMTIME st;
    GetLocalTime(&st);
    sec = time(NULL);
    ms = (int)st.wMilliseconds;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    sec = ts.tv_sec;
    ms = (int)(ts.tv_nsec / 1000000);
#endif
    struct tm *tm_info = localtime(&sec);

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    char log_line[1152];
    int len = snprintf(log_line, sizeof(log_line), "[%s.%03d] [%-5s] [%-6s] %s\n",
                     timestamp, ms, level_names[level], category_names[cat], message);

    if (g_log_file) {
        long file_pos = ftell(g_log_file);
        if (file_pos > LOG_MAX_FILE_SIZE) {
            fclose(g_log_file);

            char old_name[280];
            snprintf(old_name, sizeof(old_name), "%s.old", g_filename);
            remove(old_name);
            rename(g_filename, old_name);

            g_log_file = fopen(g_filename, "a");
            if (g_log_file) {
                fprintf(g_log_file, "=== Log rotated (size exceeded) ===\n");
            }
        }
    }

    fprintf(stderr, "%s", log_line);

    if (g_log_file) {
        fwrite(log_line, 1, len, g_log_file);
        fflush(g_log_file);
    }
}
