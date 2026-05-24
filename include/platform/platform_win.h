#ifndef PLATFORM_WIN_H
#define PLATFORM_WIN_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "platform/platform.h"

int  platform_win_init(int width, int height);
void platform_win_shutdown(void);
bool platform_win_poll_event(Event *event);
void platform_win_get_window_size(int *width, int *height);
void platform_win_hide_cursor(bool hidden);
void platform_win_grab_mouse(bool grabbed);
void platform_win_warp_mouse(int x, int y);

HINSTANCE platform_win_get_instance(void);
HWND platform_win_get_window(void);
HDC platform_win_get_dc(void);
void* platform_win_get_gl_proc_address(const char *name);
void platform_win_swap_buffers(void);
void platform_win_swap_interval(int interval);

#endif /* PLATFORM_WIN_H */
