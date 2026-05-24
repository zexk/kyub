#include "platform/platform.h"

#if defined(PLATFORM_WIN32)
#include "platform/platform_win.h"
#else
#include "platform/platform_x11.h"
#endif

int platform_init(int width, int height) {
#if defined(PLATFORM_WIN32)
    return platform_win_init(width, height);
#else
    return platform_x11_init(width, height);
#endif
}

void platform_shutdown(void) {
#if defined(PLATFORM_WIN32)
    platform_win_shutdown();
#else
    platform_x11_shutdown();
#endif
}

bool platform_poll_event(Event *event) {
#if defined(PLATFORM_WIN32)
    return platform_win_poll_event(event);
#else
    return platform_x11_poll_event(event);
#endif
}

void platform_get_window_size(int *width, int *height) {
#if defined(PLATFORM_WIN32)
    platform_win_get_window_size(width, height);
#else
    platform_x11_get_window_size(width, height);
#endif
}

void platform_hide_cursor(bool hidden) {
#if defined(PLATFORM_WIN32)
    platform_win_hide_cursor(hidden);
#else
    platform_x11_hide_cursor(hidden);
#endif
}

void platform_grab_mouse(bool grabbed) {
#if defined(PLATFORM_WIN32)
    platform_win_grab_mouse(grabbed);
#else
    platform_x11_grab_mouse(grabbed);
#endif
}

void platform_warp_mouse(int x, int y) {
#if defined(PLATFORM_WIN32)
    platform_win_warp_mouse(x, y);
#else
    platform_x11_warp_mouse(x, y);
#endif
}
