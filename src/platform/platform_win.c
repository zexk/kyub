#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "platform/platform_win.h"
#ifdef RENDERER_OPENGL
#include <GL/gl.h>
#endif
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

HINSTANCE g_win_instance;
HWND g_win_window;
HDC g_win_dc;
int g_win_width;
int g_win_height;

#ifdef RENDERER_OPENGL
static HGLRC g_win_gl_context;
#endif

static const char *WINDOW_CLASS_NAME = "KyubWindowClass";

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB  0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

#ifdef RENDERER_OPENGL
typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int *);
typedef BOOL (WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int);
#endif

static int vk_to_key(WPARAM vk) {
    switch (vk) {
        case 'W': return 'w';
        case 'A': return 'a';
        case 'S': return 's';
        case 'D': return 'd';
        case 'Q': return 'q';
        case 'E': return 'e';
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return 0x10;
        case VK_ESCAPE: return 0x1B;
        case VK_RETURN: return 0x0D;
        case VK_SPACE: return ' ';
        case VK_BACK: return 0x08;
        case VK_TAB: return 0x09;
        case VK_UP: return 0x26;
        case VK_DOWN: return 0x28;
        case VK_LEFT: return 0x25;
        case VK_RIGHT: return 0x27;
        default: return (int)(vk & 0xff);
    }
}

static LRESULT CALLBACK win_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

#ifdef RENDERER_OPENGL
static bool setup_pixel_format(HDC dc) {
    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int format = ChoosePixelFormat(dc, &pfd);
    if (format == 0) return false;
    return SetPixelFormat(dc, format, &pfd) != FALSE;
}

static bool create_gl_context(void) {
    if (!setup_pixel_format(g_win_dc)) {
        fprintf(stderr, "Failed to set Win32 pixel format\n");
        return false;
    }

    HGLRC legacy = wglCreateContext(g_win_dc);
    if (!legacy) {
        fprintf(stderr, "Failed to create legacy WGL context\n");
        return false;
    }
    if (!wglMakeCurrent(g_win_dc, legacy)) {
        fprintf(stderr, "Failed to make legacy WGL context current\n");
        wglDeleteContext(legacy);
        return false;
    }

    PFNWGLCREATECONTEXTATTRIBSARBPROC create_context_attribs =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    if (create_context_attribs) {
        int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
            WGL_CONTEXT_MINOR_VERSION_ARB, 5,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        HGLRC modern = create_context_attribs(g_win_dc, 0, attribs);
        if (modern) {
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(legacy);
            g_win_gl_context = modern;
            return wglMakeCurrent(g_win_dc, g_win_gl_context) != FALSE;
        }
    }

    g_win_gl_context = legacy;
    return true;
}
#endif

int platform_win_init(int width, int height) {
    g_win_width = width;
    g_win_height = height;
    g_win_instance = GetModuleHandleA(NULL);

    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = win_proc;
    wc.hInstance = g_win_instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        fprintf(stderr, "Failed to register window class: %lu\n", GetLastError());
        return -1;
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rect = {0, 0, width, height};
    AdjustWindowRect(&rect, style, FALSE);

    g_win_window = CreateWindowExA(
        0, WINDOW_CLASS_NAME, "Kyub", style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        NULL, NULL, g_win_instance, NULL);
    if (!g_win_window) return -1;

    g_win_dc = GetDC(g_win_window);
    if (!g_win_dc) return -1;

#ifdef RENDERER_OPENGL
    if (!create_gl_context()) return -1;
#endif

    ShowWindow(g_win_window, SW_SHOW);
    UpdateWindow(g_win_window);
    return 0;
}

void platform_win_shutdown(void) {
#ifdef RENDERER_OPENGL
    if (g_win_gl_context) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(g_win_gl_context);
        g_win_gl_context = NULL;
    }
#endif
    if (g_win_dc && g_win_window) {
        ReleaseDC(g_win_window, g_win_dc);
        g_win_dc = NULL;
    }
    if (g_win_window) {
        DestroyWindow(g_win_window);
        g_win_window = NULL;
    }
    UnregisterClassA(WINDOW_CLASS_NAME, g_win_instance);
}

bool platform_win_poll_event(Event *event) {
    MSG msg;
    memset(event, 0, sizeof(Event));

    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        event->type = EVENT_NONE;

        switch (msg.message) {
            case WM_QUIT:
                event->type = EVENT_QUIT;
                return true;
            case WM_SIZE:
                g_win_width = LOWORD(msg.lParam);
                g_win_height = HIWORD(msg.lParam);
                event->type = EVENT_RESIZE;
                event->resize.width = g_win_width;
                event->resize.height = g_win_height;
                return true;
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                TranslateMessage(&msg);
                event->type = EVENT_KEY_DOWN;
                event->key.key = vk_to_key(msg.wParam);
                event->key.keysym = (unsigned long)msg.wParam;
                event->key.down = true;
                return true;
            case WM_KEYUP:
            case WM_SYSKEYUP:
                event->type = EVENT_KEY_UP;
                event->key.key = vk_to_key(msg.wParam);
                event->key.keysym = (unsigned long)msg.wParam;
                event->key.down = false;
                return true;
            case WM_CHAR:
                if (msg.wParam >= 32 && msg.wParam < 127) {
                    event->type = EVENT_TEXT;
                    event->text.c = (char)msg.wParam;
                    return true;
                }
                break;
            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP: {
                bool down = msg.message == WM_LBUTTONDOWN || msg.message == WM_MBUTTONDOWN || msg.message == WM_RBUTTONDOWN;
                event->type = EVENT_MOUSE_BUTTON;
                event->mouse_button.x = GET_X_LPARAM(msg.lParam);
                event->mouse_button.y = GET_Y_LPARAM(msg.lParam);
                event->mouse_button.down = down;
                event->mouse_button.button =
                    (msg.message == WM_LBUTTONDOWN || msg.message == WM_LBUTTONUP) ? MOUSE_BUTTON_LEFT :
                    (msg.message == WM_MBUTTONDOWN || msg.message == WM_MBUTTONUP) ? MOUSE_BUTTON_MIDDLE :
                    MOUSE_BUTTON_RIGHT;
                return true;
            }
            case WM_MOUSEMOVE:
                event->type = EVENT_MOUSE_MOTION;
                event->mouse_motion.x = GET_X_LPARAM(msg.lParam);
                event->mouse_motion.y = GET_Y_LPARAM(msg.lParam);
                return true;
            case WM_MOUSEWHEEL:
                event->type = EVENT_SCROLL;
                event->scroll.dx = 0;
                event->scroll.dy = GET_WHEEL_DELTA_WPARAM(msg.wParam) / WHEEL_DELTA;
                return true;
            case WM_MOUSEHWHEEL:
                event->type = EVENT_SCROLL;
                event->scroll.dx = GET_WHEEL_DELTA_WPARAM(msg.wParam) / WHEEL_DELTA;
                event->scroll.dy = 0;
                return true;
            default:
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
                break;
        }
    }
    return false;
}

void platform_win_get_window_size(int *width, int *height) {
    if (width) *width = g_win_width;
    if (height) *height = g_win_height;
}

static bool s_cursor_hidden = false;

void platform_win_hide_cursor(bool hidden) {
    if (hidden == s_cursor_hidden) return;
    ShowCursor(hidden ? FALSE : TRUE);
    s_cursor_hidden = hidden;
}

void platform_win_grab_mouse(bool grabbed) {
    if (grabbed) {
        RECT rect;
        GetClientRect(g_win_window, &rect);
        MapWindowPoints(g_win_window, NULL, (POINT *)&rect, 2);
        ClipCursor(&rect);
    } else {
        ClipCursor(NULL);
    }
}

void platform_win_warp_mouse(int x, int y) {
    POINT pt = {x, y};
    ClientToScreen(g_win_window, &pt);
    SetCursorPos(pt.x, pt.y);
}

HINSTANCE platform_win_get_instance(void) {
    return g_win_instance;
}

HWND platform_win_get_window(void) {
    return g_win_window;
}

HDC platform_win_get_dc(void) {
    return g_win_dc;
}

void* platform_win_get_gl_proc_address(const char *name) {
#ifdef RENDERER_OPENGL
    void *proc = (void *)wglGetProcAddress(name);
    if (proc == NULL || proc == (void *)0x1 || proc == (void *)0x2 ||
        proc == (void *)0x3 || proc == (void *)-1) {
        static HMODULE s_opengl32 = NULL;
        if (!s_opengl32) s_opengl32 = LoadLibraryA("opengl32.dll");
        proc = s_opengl32 ? (void *)GetProcAddress(s_opengl32, name) : NULL;
    }
    return proc;
#else
    (void)name;
    return NULL;
#endif
}

void platform_win_swap_buffers(void) {
#ifdef RENDERER_OPENGL
    SwapBuffers(g_win_dc);
#endif
}

void platform_win_swap_interval(int interval) {
#ifdef RENDERER_OPENGL
    PFNWGLSWAPINTERVALEXTPROC swap_interval =
        (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    if (swap_interval) swap_interval(interval);
#else
    (void)interval;
#endif
}

static char* dup_str(const char *s) {
    size_t len = strlen(s) + 1;
    char *d = malloc(len);
    if (d) memcpy(d, s, len);
    return d;
}

char* platform_resolve_path(const char *path) {
    if (!path) return NULL;

    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
        return dup_str(path);
    }

    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if (len == 0 || len >= sizeof(exe_path)) {
        return dup_str(path);
    }

    char *last_slash = strrchr(exe_path, '\\');
    char *last_fwd = strrchr(exe_path, '/');
    char *last = last_slash > last_fwd ? last_slash : last_fwd;
    if (last) *last = '\0';

    size_t result_size = strlen(exe_path) + 1 + strlen(path) + 1;
    char *result = malloc(result_size);
    if (!result) return dup_str(path);

    snprintf(result, result_size, "%s/%s", exe_path, path);
    return result;
}
