#define _POSIX_C_SOURCE 199309L
#include "common.h"
#include "gl_ext.h"
#include "shader.h"
#include "voxel.h"
#include "mesh.h"
#include "math3d.h"
#include "camera.h"
#include "input.h"
#include "world.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <time.h>

static double get_time_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(void) {
    Display *display = XOpenDisplay(NULL);
    if (!display) return 1;
    int screen_id = DefaultScreen(display);
    int attributes[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, GLX_SAMPLE_BUFFERS, 1, GLX_SAMPLES, 4, None };
    XVisualInfo *visual_info = glXChooseVisual(display, screen_id, attributes);
    if (!visual_info) return 1;
    Colormap colormap = XCreateColormap(display, RootWindow(display, screen_id), visual_info->visual, AllocNone);
    XSetWindowAttributes window_attributes;
    window_attributes.colormap = colormap;
    window_attributes.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | PointerMotionMask | StructureNotifyMask;
    int width = 800, height = 600;
    Window window = XCreateWindow(display, RootWindow(display, screen_id), 0, 0, width, height, 0, visual_info->depth, InputOutput, visual_info->visual, CWColormap | CWEventMask, &window_attributes);
    XMapWindow(display, window);
    
    // Hide cursor
    Pixmap blank = XCreateBitmapFromData(display, window, (char[]){0}, 1, 1);
    XColor dummy;
    Cursor cursor = XCreatePixmapCursor(display, blank, blank, &dummy, &dummy, 0, 0);
    XDefineCursor(display, window, cursor);
    
    Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wm_delete_window, 1);

    GLXContext gl_context = glXCreateContext(display, visual_info, NULL, GL_TRUE);
    glXMakeCurrent(display, window, gl_context);
    if (!gl_ext_init()) return 1;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_MULTISAMPLE);

    unsigned int shader_program = shader_create_program("shaders/basic.vert", "shaders/basic.frag");
    if (!shader_program) return 1;

    World world;
    world_init(&world, 2); // 2 chunk render distance

    Camera camera;
    camera_init(&camera);
    input_init();

    double last_time = get_time_s();
    bool running = true;
    XGrabPointer(display, window, True, PointerMotionMask, GrabModeAsync, GrabModeAsync, window, None, CurrentTime);

    while (running) {
        double current_time = get_time_s();
        float dt = (float)(current_time - last_time);
        last_time = current_time;

        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == KeyPress) {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                if (keysym == XK_Escape) running = false;
                if (keysym == XK_w) input_set_key('w', true);
                if (keysym == XK_s) input_set_key('s', true);
                if (keysym == XK_a) input_set_key('a', true);
                if (keysym == XK_d) input_set_key('d', true);
                if (keysym == XK_Shift_L || keysym == XK_Shift_R) input_set_shift(true);
            } else if (event.type == KeyRelease) {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                if (keysym == XK_w) input_set_key('w', false);
                if (keysym == XK_s) input_set_key('s', false);
                if (keysym == XK_a) input_set_key('a', false);
                if (keysym == XK_d) input_set_key('d', false);
                if (keysym == XK_Shift_L || keysym == XK_Shift_R) input_set_shift(false);
            } else if (event.type == MotionNotify) {
                int dx = event.xmotion.x - width / 2;
                int dy = event.xmotion.y - height / 2;
                if (dx != 0 || dy != 0) {
                    input_set_mouse_delta((float)dx, (float)dy);
                    XWarpPointer(display, None, window, 0, 0, 0, 0, width / 2, height / 2);
                }
            } else if (event.type == ClientMessage) {
                if ((Atom)event.xclient.data.l[0] == wm_delete_window) running = false;
            } else if (event.type == ConfigureNotify) {
                width = event.xconfigure.width;
                height = event.xconfigure.height;
                glViewport(0, 0, width, height);
            }
        }

        camera_update(&camera, dt);
        world_update(&world, camera.pos);

        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(shader_program);

        mat4 model = mat4_identity();
        mat4 projection = mat4_perspective(45.0f * PI / 180.0f, (float)width / (float)height, 0.1f, 100.0f);
        mat4 view = camera_get_view_matrix(&camera);
        mat4 view_proj = mat4_multiply(projection, view);
        
        Frustum frustum;
        frustum_extract(&frustum, view_proj);

        glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, model.m);
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "view"), 1, GL_FALSE, view.m);
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "projection"), 1, GL_FALSE, projection.m);

        for (int i = 0; i < world.capacity; i++) {
            if (world.chunks[i].active && frustum_intersects_box(&frustum, world.chunks[i].chunk->min, world.chunks[i].chunk->max)) {
                glBindVertexArray(world.chunks[i].mesh->vao);
                glDrawArrays(GL_TRIANGLES, 0, world.chunks[i].mesh->vertex_count);
            }
        }

        glXSwapBuffers(display, window);
    }

    world_free(&world);
    glDeleteProgram(shader_program);
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, gl_context);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}
