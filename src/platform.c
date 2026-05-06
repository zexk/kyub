#include "platform.h"
#include "platform_x11.h"

int platform_init(int width, int height) {
    return platform_x11_init(width, height);
}

void platform_shutdown(void) {
    platform_x11_shutdown();
}

bool platform_poll_event(Event *event) {
    return platform_x11_poll_event(event);
}

void platform_get_window_size(int *width, int *height) {
    platform_x11_get_window_size(width, height);
}

void platform_hide_cursor(bool hidden) {
    platform_x11_hide_cursor(hidden);
}

void platform_grab_mouse(bool grabbed) {
    platform_x11_grab_mouse(grabbed);
}

void platform_warp_mouse(int x, int y) {
    platform_x11_warp_mouse(x, y);
}