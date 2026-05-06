#ifndef PLATFORM_X11_H
#define PLATFORM_X11_H

#include <X11/Xlib.h>
#include "platform.h"

int  platform_x11_init(int width, int height);
void platform_x11_shutdown(void);
bool platform_x11_poll_event(Event *event);
void platform_x11_get_window_size(int *width, int *height);
void platform_x11_hide_cursor(bool hidden);
void platform_x11_grab_mouse(bool grabbed);
void platform_x11_warp_mouse(int x, int y);

extern Display *g_x11_display;
extern Window g_x11_window;
extern int g_x11_width;
extern int g_x11_height;
extern Atom g_x11_wm_delete_window;

Display* platform_x11_get_display(void);
Window platform_x11_get_window(void);

#endif // PLATFORM_X11_H