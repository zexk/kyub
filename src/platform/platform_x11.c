#define _GNU_SOURCE
#include "platform/platform_x11.h"
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef RENDERER_OPENGL
#include <GL/glx.h>
#endif

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
    XRaiseWindow(g_x11_display, g_x11_window);

    g_x11_wm_delete_window = XInternAtom(g_x11_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_x11_display, g_x11_window, &g_x11_wm_delete_window, 1);

#ifdef RENDERER_OPENGL
    /* Create OpenGL 4.5 core profile context */
    int glx_attribs[] = {
        GLX_X_RENDERABLE    , True,
        GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
        GLX_RENDER_TYPE     , GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
        GLX_RED_SIZE        , 8,
        GLX_GREEN_SIZE      , 8,
        GLX_BLUE_SIZE       , 8,
        GLX_ALPHA_SIZE      , 0,
        GLX_DEPTH_SIZE      , 24,
        GLX_STENCIL_SIZE    , 8,
        GLX_DOUBLEBUFFER    , True,
        GLX_SAMPLE_BUFFERS  , 0,
        GLX_SAMPLES         , 0,
        None
    };

    int fbcount;
    GLXFBConfig *fbc = glXChooseFBConfig(g_x11_display, g_screen, glx_attribs, &fbcount);
    if (!fbc || fbcount < 1) {
        fprintf(stderr, "Failed to choose FB config\n");
        return -1;
    }

    /* Find a FB config with alpha=0; fall back to first if none found */
    GLXFBConfig best_fbc = fbc[0];
    int best_alpha = -1;
    for (int i = 0; i < fbcount; i++) {
        int alpha;
        glXGetFBConfigAttrib(g_x11_display, fbc[i], GLX_ALPHA_SIZE, &alpha);
        if (alpha == 0) {
            best_fbc = fbc[i];
            best_alpha = 0;
            break;
        }
    }
    XFree(fbc);

    /* Get visual from FB config */
    XVisualInfo *vi = glXGetVisualFromFBConfig(g_x11_display, best_fbc);
    if (!vi) {
        fprintf(stderr, "Failed to get visual from FB config\n");
        return -1;
    }
    /* Create colormap with GLX visual */
    Colormap glx_colormap = XCreateColormap(g_x11_display, g_root, vi->visual, AllocNone);
    wa.colormap = glx_colormap;
    wa.background_pixel = BlackPixel(g_x11_display, g_screen);

    /* Recreate window with OpenGL-compatible visual */
    XDestroyWindow(g_x11_display, g_x11_window);
    if (g_colormap) XFreeColormap(g_x11_display, g_colormap);
    g_colormap = glx_colormap;

    g_x11_window = XCreateWindow(
        g_x11_display, g_root,
        0, 0, width, height, 0,
        vi->depth, InputOutput,
        vi->visual,
        CWColormap | CWEventMask | CWBackPixel, &wa
    );

    XStoreName(g_x11_display, g_x11_window, "Kyub");

    /* Hint to compositor: fully opaque window (set before mapping) */
    Atom opacity_atom = XInternAtom(g_x11_display, "_NET_WM_WINDOW_OPACITY", False);
    unsigned long opacity = 0xFFFFFFFF;
    XChangeProperty(g_x11_display, g_x11_window, opacity_atom,
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char*)&opacity, 1);

    XMapWindow(g_x11_display, g_x11_window);
    XRaiseWindow(g_x11_display, g_x11_window);

    g_x11_wm_delete_window = XInternAtom(g_x11_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_x11_display, g_x11_window, &g_x11_wm_delete_window, 1);

    XFree(vi);

    /* Create OpenGL context */
    PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB =
        (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");

    if (!glXCreateContextAttribsARB) {
        fprintf(stderr, "glXCreateContextAttribsARB not available\n");
        return -1;
    }

    int context_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 5,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None
    };

    GLXContext glx_context = glXCreateContextAttribsARB(g_x11_display, best_fbc, 0, True, context_attribs);
    if (!glx_context) {
        fprintf(stderr, "Failed to create OpenGL 4.5 context\n");
        return -1;
    }

    if (!glXMakeCurrent(g_x11_display, g_x11_window, glx_context)) {
        fprintf(stderr, "Failed to make GLX context current\n");
        return -1;
    }
#endif

    char cursor_bits[1] = {0};
    Pixmap blank = XCreateBitmapFromData(g_x11_display, g_x11_window, cursor_bits, 1, 1);
    XColor dummy;
    g_blank_pixmap_cursor = XCreatePixmapCursor(g_x11_display, blank, blank, &dummy, &dummy, 0, 0);

    return 0;
}

void platform_x11_shutdown(void) {
#ifdef RENDERER_OPENGL
    GLXContext ctx = glXGetCurrentContext();
    if (ctx) {
        glXMakeCurrent(g_x11_display, None, NULL);
        glXDestroyContext(g_x11_display, ctx);
    }
#endif
    if (g_blank_pixmap_cursor) XFreeCursor(g_x11_display, g_blank_pixmap_cursor);
    if (g_x11_window) XDestroyWindow(g_x11_display, g_x11_window);
    if (g_colormap) XFreeColormap(g_x11_display, g_colormap);
    if (g_x11_display) XCloseDisplay(g_x11_display);
}

static Event g_pending_event;
static bool g_has_pending = false;

bool platform_x11_poll_event(Event *event) {
    if (g_has_pending) {
        *event = g_pending_event;
        g_has_pending = false;
        return true;
    }

    if (!XPending(g_x11_display)) return false;

    XEvent ev;
    XNextEvent(g_x11_display, &ev);

    memset(event, 0, sizeof(Event));
    event->type = EVENT_NONE;

    if (ev.type == KeyPress) {
        KeySym keysym = XLookupKeysym(&ev.xkey, 0);
        int key = keysym_to_key(keysym);
        event->type = EVENT_KEY_DOWN;
        event->key.key = key;
        event->key.keysym = keysym;
        event->key.down = true;

        char buf[32];
        int len = XLookupString(&ev.xkey, buf, sizeof(buf), NULL, NULL);
        if (len == 1 && (unsigned char)buf[0] >= 32 && (unsigned char)buf[0] < 127) {
            g_pending_event.type = EVENT_TEXT;
            g_pending_event.text.c = buf[0];
            g_has_pending = true;
        }
    } else if (ev.type == KeyRelease) {
        KeySym keysym = XLookupKeysym(&ev.xkey, 0);
        int key = keysym_to_key(keysym);
        event->type = EVENT_KEY_UP;
        event->key.key = key;
        event->key.keysym = keysym;
        event->key.down = false;
    } else if (ev.type == ButtonPress) {
        if (ev.xbutton.button == 4) {
            event->type = EVENT_SCROLL;
            event->scroll.dx = 0;
            event->scroll.dy = 1;
        } else if (ev.xbutton.button == 5) {
            event->type = EVENT_SCROLL;
            event->scroll.dx = 0;
            event->scroll.dy = -1;
        } else if (ev.xbutton.button == 6) {
            event->type = EVENT_SCROLL;
            event->scroll.dx = 1;
            event->scroll.dy = 0;
        } else if (ev.xbutton.button == 7) {
            event->type = EVENT_SCROLL;
            event->scroll.dx = -1;
            event->scroll.dy = 0;
        } else {
            MouseButton btn = (ev.xbutton.button == 1) ? MOUSE_BUTTON_LEFT :
                          (ev.xbutton.button == 2) ? MOUSE_BUTTON_MIDDLE : MOUSE_BUTTON_RIGHT;
            event->type = EVENT_MOUSE_BUTTON;
            event->mouse_button.x = ev.xbutton.x;
            event->mouse_button.y = ev.xbutton.y;
            event->mouse_button.button = btn;
            event->mouse_button.down = true;
        }
    } else if (ev.type == ButtonRelease) {
        if (ev.xbutton.button >= 4 && ev.xbutton.button <= 7) {
            event->type = EVENT_NONE;
        } else {
            MouseButton btn = (ev.xbutton.button == 1) ? MOUSE_BUTTON_LEFT :
                          (ev.xbutton.button == 2) ? MOUSE_BUTTON_MIDDLE : MOUSE_BUTTON_RIGHT;
            event->type = EVENT_MOUSE_BUTTON;
            event->mouse_button.x = ev.xbutton.x;
            event->mouse_button.y = ev.xbutton.y;
            event->mouse_button.button = btn;
            event->mouse_button.down = false;
        }
    } else if (ev.type == MotionNotify) {
        event->type = EVENT_MOUSE_MOTION;
        event->mouse_motion.x = ev.xmotion.x;
        event->mouse_motion.y = ev.xmotion.y;
    } else if (ev.type == ClientMessage) {
        if ((Atom)ev.xclient.data.l[0] == g_x11_wm_delete_window) {
            event->type = EVENT_QUIT;
        }
    } else if (ev.type == ConfigureNotify) {
        event->type = EVENT_RESIZE;
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
        XGrabPointer(g_x11_display, g_x11_window, True, PointerMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, g_x11_window, None, CurrentTime);
    } else {
        XUngrabPointer(g_x11_display, CurrentTime);
    }
}

void platform_x11_warp_mouse(int x, int y) {
    XWarpPointer(g_x11_display, None, g_x11_window, 0, 0, 0, 0, x, y);
    XFlush(g_x11_display);
}

Display* platform_x11_get_display(void) {
    return g_x11_display;
}

Window platform_x11_get_window(void) {
    return g_x11_window;
}

static char* dup_str(const char *s) {
    size_t len = strlen(s) + 1;
    char *d = malloc(len);
    if (d) memcpy(d, s, len);
    return d;
}

char* platform_resolve_path(const char *path) {
    if (!path) return NULL;

    if (access(path, F_OK) == 0) {
        return dup_str(path);
    }

    char exe_path[4096];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        return dup_str(path);
    }
    exe_path[len] = '\0';

    char *last_slash = strrchr(exe_path, '/');
    if (last_slash) {
        *last_slash = '\0';
    }

    size_t result_size = strlen(exe_path) + 1 + strlen(path) + 1;
    char *result = malloc(result_size);
    if (!result) {
        return dup_str(path);
    }

    snprintf(result, result_size, "%s/%s", exe_path, path);
    return result;
}