#define _POSIX_C_SOURCE 199309L
#include "logger.h"

#ifdef ENABLE_LOGGER

#include "common.h"
#include <string.h>
#include <time.h>

#define LOG_MAX_FILE_SIZE (10 * 1024 * 1024)  // 10MB generous limit
#define LOG_CATEGORY_NAMES {"WORLD", "GL", "UI", "INPUT", "PLATFORM"}

static FILE *g_log_file = NULL;
static LogLevel g_log_level = LOG_INFO;
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

void logger_init(const char *filename) {
    if (!filename) return;
    
    strncpy(g_filename, filename, sizeof(g_filename) - 1);
    g_filename[sizeof(g_filename) - 1] = '\0';
    
    g_log_file = fopen(g_filename, "a");
    if (!g_log_file) {
        fprintf(stderr, "[logger] Failed to open log file: %s\n", filename);
        return;
    }
    
    // Write startup marker
    time_t now = time(NULL);
    fprintf(g_log_file, "\n=== Logger initialized at %s", ctime(&now));
    fflush(g_log_file);
}

void logger_shutdown(void) {
    if (g_log_file) {
        time_t now = time(NULL);
        fprintf(g_log_file, "=== Logger shutdown at %s\n", ctime(&now));
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
    // Check if we should log this level
    if (level > g_log_level || level < 0 || level >= LOG_LEVEL_COUNT) return;
    if (cat < 0 || cat >= CAT_COUNT) cat = CAT_COUNT - 1;
    
    // Get current time
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm *tm_info = localtime(&ts.tv_sec);
    
    // Format timestamp
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Add milliseconds
    int ms = (int)(ts.tv_nsec / 1000000);
    
    // Build the message
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    
    // Format: [timestamp.mmm] [LEVEL] [CAT] message
    char log_line[1152];
    int len = snprintf(log_line, sizeof(log_line), "[%s.%03d] [%-5s] [%-6s] %s\n",
                     timestamp, ms, level_names[level], category_names[cat], message);
    
    // Check file rotation
    if (g_log_file) {
        long file_pos = ftell(g_log_file);
        if (file_pos > LOG_MAX_FILE_SIZE) {
            // Rotate: close current, rename to .old, open new
            fclose(g_log_file);
            
            char old_name[280];  // Max original + ".old" + null
            snprintf(old_name, sizeof(old_name), "%s.old", g_filename);
            remove(old_name);
            rename(g_filename, old_name);
            
            g_log_file = fopen(g_filename, "a");
            if (g_log_file) {
                fprintf(g_log_file, "=== Log rotated (size exceeded) ===\n");
            }
        }
    }
    
    // Output to stderr
    fprintf(stderr, "%s", log_line);
    
    // Output to file
    if (g_log_file) {
        fwrite(log_line, 1, len, g_log_file);
        fflush(g_log_file);
    }
}

#else  // ENABLE_LOGGER stubs

void logger_init(const char *filename) { (void)filename; }
void logger_shutdown(void) {}
void logger_set_level(LogLevel level) { (void)level; }
void logger_log(LogLevel level, LogCategory cat, const char *fmt, ...) {
    (void)level; (void)cat; (void)fmt;
}

#endif // ENABLE_LOGGER