#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>

#define PLATFORM_MAX_KEYS 256

typedef enum {
    EVENT_NONE,
    EVENT_KEY_DOWN,
    EVENT_KEY_UP,
    EVENT_MOUSE_BUTTON,
    EVENT_MOUSE_MOTION,
    EVENT_SCROLL,
    EVENT_TEXT,
    EVENT_RESIZE,
    EVENT_QUIT
} EventType;

typedef enum {
    MOUSE_BUTTON_LEFT = 1,
    MOUSE_BUTTON_MIDDLE = 2,
    MOUSE_BUTTON_RIGHT = 3
} MouseButton;

typedef struct Event {
    EventType type;
    union {
        struct { int key; unsigned long keysym; bool down; } key;
        struct { int x; int y; MouseButton button; bool down; } mouse_button;
        struct { int x; int y; } mouse_motion;
        struct { int dx; int dy; } scroll;
        struct { char c; } text;
        struct { int width; int height; } resize;
    };
} Event;

int  platform_init(int width, int height);
void platform_shutdown(void);
bool platform_poll_event(Event *event);
void platform_get_window_size(int *width, int *height);

void platform_hide_cursor(bool hidden);
void platform_grab_mouse(bool grabbed);
void platform_warp_mouse(int x, int y);

#endif // PLATFORM_H