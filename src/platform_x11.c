#include "platform_x11.h"
#include <X11/Xutil.h>
#include <stdio.h>
#include <string.h>

Display *g_x11_display;
Window g_x11_window;
int g_x11_width;
int g_x11_height;
Atom g_x11_wm_delete_window;

static int g_screen;
static Colormap g_colormap;
static Window g_root;
static Cursor g_blank_pixmap_cursor;

#define PLATFORM_EVENT_KEY_DOWN 1
#define PLATFORM_EVENT_KEY_UP 2
#define PLATFORM_EVENT_MOUSE_BUTTON 3
#define PLATFORM_EVENT_MOUSE_MOTION 4
#define PLATFORM_EVENT_RESIZE 5
#define PLATFORM_EVENT_QUIT 6

static int keysym_to_key(int keysym) {
    switch (keysym) {
        case XK_w: case XK_W: return 'w';
        case XK_s: case XK_S: return 's';
        case XK_a: case XK_A: return 'a';
        case XK_d: case XK_D: return 'd';
        case XK_q: case XK_Q: return 'q';
        case XK_e: case XK_E: return 'e';
        case XK_Shift_L: case XK_Shift_R: return 0x10;
        case XK_Escape: return 0x1B;
        case XK_Return: case XK_KP_Enter: return 0x0D;
        case XK_space: return ' ';
        case XK_BackSpace: return 0x08;
        case XK_Tab: return 0x09;
        case XK_Up: return 0x26;
        case XK_Down: return 0x28;
        case XK_Left: return 0x25;
        case XK_Right: return 0x27;
        default: return keysym & 0xFF;
    }
}

int platform_x11_init(int width, int height) {
    g_x11_width = width;
    g_x11_height = height;

    g_x11_display = XOpenDisplay(NULL);
    if (!g_x11_display) return -1;

    g_screen = DefaultScreen(g_x11_display);
    g_root = RootWindow(g_x11_display, g_screen);

    int screen = DefaultScreen(g_x11_display);
    Visual *visual = DefaultVisual(g_x11_display, screen);
    g_colormap = XCreateColormap(g_x11_display, g_root, visual, AllocNone);

    XSetWindowAttributes wa;
    wa.colormap = g_colormap;
    wa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;

    g_x11_window = XCreateWindow(
        g_x11_display, g_root,
        0, 0, width, height, 0,
        DefaultDepth(g_x11_display, screen), InputOutput,
        visual,
        CWColormap | CWEventMask, &wa
    );

    XStoreName(g_x11_display, g_x11_window, "Kyub");
    XMapWindow(g_x11_display, g_x11_window);

    g_x11_wm_delete_window = XInternAtom(g_x11_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_x11_display, g_x11_window, &g_x11_wm_delete_window, 1);

    char cursor_bits[1] = {0};
    Pixmap blank = XCreateBitmapFromData(g_x11_display, g_x11_window, cursor_bits, 1, 1);
    XColor dummy;
    g_blank_pixmap_cursor = XCreatePixmapCursor(g_x11_display, blank, blank, &dummy, &dummy, 0, 0);

    return 0;
}

void platform_x11_shutdown(void) {
    if (g_blank_pixmap_cursor) XFreeCursor(g_x11_display, g_blank_pixmap_cursor);
    if (g_x11_window) XDestroyWindow(g_x11_display, g_x11_window);
    if (g_colormap) XFreeColormap(g_x11_display, g_colormap);
    if (g_x11_display) XCloseDisplay(g_x11_display);
}

bool platform_x11_poll_event(Event *event) {
    if (!XPending(g_x11_display)) return false;

    XEvent ev;
    XNextEvent(g_x11_display, &ev);

    memset(event, 0, sizeof(Event));
    event->type = EVENT_NONE;

if (ev.type == KeyPress) {
        KeySym keysym = XLookupKeysym(&ev.xkey, 0);
        int key = keysym_to_key(keysym);
        event->type = PLATFORM_EVENT_KEY_DOWN;
        event->key.key = key;
        event->key.down = true;
    } else if (ev.type == KeyRelease) {
        KeySym keysym = XLookupKeysym(&ev.xkey, 0);
        int key = keysym_to_key(keysym);
        event->type = PLATFORM_EVENT_KEY_UP;
        event->key.key = key;
        event->key.down = false;
    } else if (ev.type == ButtonPress) {
        MouseButton btn = (ev.xbutton.button == 1) ? MOUSE_BUTTON_LEFT :
                      (ev.xbutton.button == 2) ? MOUSE_BUTTON_MIDDLE : MOUSE_BUTTON_RIGHT;
        event->type = PLATFORM_EVENT_MOUSE_BUTTON;
        event->mouse_button.x = ev.xbutton.x;
        event->mouse_button.y = ev.xbutton.y;
        event->mouse_button.button = btn;
        event->mouse_button.down = true;
    } else if (ev.type == ButtonRelease) {
        MouseButton btn = (ev.xbutton.button == 1) ? MOUSE_BUTTON_LEFT :
                      (ev.xbutton.button == 2) ? MOUSE_BUTTON_MIDDLE : MOUSE_BUTTON_RIGHT;
        event->type = PLATFORM_EVENT_MOUSE_BUTTON;
        event->mouse_button.x = ev.xbutton.x;
        event->mouse_button.y = ev.xbutton.y;
        event->mouse_button.button = btn;
        event->mouse_button.down = false;
    } else if (ev.type == MotionNotify) {
        event->type = PLATFORM_EVENT_MOUSE_MOTION;
        event->mouse_motion.x = ev.xmotion.x;
        event->mouse_motion.y = ev.xmotion.y;
    } else if (ev.type == ClientMessage) {
        if ((Atom)ev.xclient.data.l[0] == g_x11_wm_delete_window) {
            event->type = PLATFORM_EVENT_QUIT;
        }
    } else if (ev.type == ConfigureNotify) {
        event->type = PLATFORM_EVENT_RESIZE;
        event->resize.width = ev.xconfigure.width;
        event->resize.height = ev.xconfigure.height;
        g_x11_width = event->resize.width;
        g_x11_height = event->resize.height;
    }

    return true;
}

void platform_x11_get_window_size(int *width, int *height) {
    if (width) *width = g_x11_width;
    if (height) *height = g_x11_height;
}

void platform_x11_hide_cursor(bool hidden) {
    if (hidden) {
        XDefineCursor(g_x11_display, g_x11_window, g_blank_pixmap_cursor);
    } else {
        XDefineCursor(g_x11_display, g_x11_window, None);
    }
    XFlush(g_x11_display);
}

void platform_x11_grab_mouse(bool grabbed) {
    if (grabbed) {
        XGrabPointer(g_x11_display, g_x11_window, True, PointerMotionMask, GrabModeAsync, GrabModeAsync, g_x11_window, None, CurrentTime);
    } else {
        XUngrabPointer(g_x11_display, CurrentTime);
    }
}

void platform_x11_warp_mouse(int x, int y) {
    XWarpPointer(g_x11_display, None, g_x11_window, 0, 0, 0, 0, x, y);
}

Display* platform_x11_get_display(void) {
    return g_x11_display;
}

Window platform_x11_get_window(void) {
    return g_x11_window;
}