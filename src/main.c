#define _POSIX_C_SOURCE 199309L
#include "common.h"
#include "gl_ext.h"
#include "shader.h"
#include "voxel.h"
#include "mesh.h"
#include "math3d.h"
#include "camera.h"
#include "input.h"
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
    Display *display;
    Window window;
    GLXContext gl_context;
    XVisualInfo *visual_info;
    Colormap colormap;
    XSetWindowAttributes window_attributes;
    int screen_id;

    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    screen_id = DefaultScreen(display);

    int attributes[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        GLX_SAMPLE_BUFFERS, 1,
        GLX_SAMPLES, 4,
        None
    };
    visual_info = glXChooseVisual(display, screen_id, attributes);
    if (!visual_info) {
        // Fallback to no MSAA if 4x is not supported
        int fallback_attributes[] = {
            GLX_RGBA,
            GLX_DEPTH_SIZE, 24,
            GLX_DOUBLEBUFFER,
            None
        };
        visual_info = glXChooseVisual(display, screen_id, fallback_attributes);
        if (!visual_info) {
            fprintf(stderr, "No appropriate visual found\n");
            XCloseDisplay(display);
            return 1;
        }
    }

    colormap = XCreateColormap(display, RootWindow(display, screen_id), visual_info->visual, AllocNone);
    window_attributes.colormap = colormap;
    window_attributes.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | PointerMotionMask | StructureNotifyMask;

    int width = 800;
    int height = 600;
    window = XCreateWindow(display, RootWindow(display, screen_id), 0, 0, width, height, 0,
                           visual_info->depth, InputOutput, visual_info->visual,
                           CWColormap | CWEventMask, &window_attributes);

    XMapWindow(display, window);
    XStoreName(display, window, "kyub - Voxel Engine");

    // Hide cursor
    Pixmap blank;
    XColor dummy;
    char data[1] = {0};
    blank = XCreateBitmapFromData(display, window, data, 1, 1);
    Cursor cursor = XCreatePixmapCursor(display, blank, blank, &dummy, &dummy, 0, 0);
    XDefineCursor(display, window, cursor);
    XFreePixmap(display, blank);

    Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wm_delete_window, 1);

    gl_context = glXCreateContext(display, visual_info, NULL, GL_TRUE);
    if (!gl_context) {
        fprintf(stderr, "Could not create GLX context\n");
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        return 1;
    }
    glXMakeCurrent(display, window, gl_context);

    if (!gl_ext_init()) {
        fprintf(stderr, "Failed to initialize OpenGL extensions\n");
        return 1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_MULTISAMPLE);

    unsigned int shader_program = shader_create_program("shaders/basic.vert", "shaders/basic.frag");
    if (!shader_program) return 1;

    // Initialize world data and meshes
    int grid_size = 3;
    World world;
    world_init(&world, grid_size);
    
    int num_chunks = grid_size * grid_size;
    Mesh *meshes = malloc(sizeof(Mesh) * num_chunks);
    for (int i = 0; i < num_chunks; i++) {
        mesh_init(&meshes[i]);
        mesh_generate_greedy(&meshes[i], &world.chunks[i]);
        mesh_upload(&meshes[i]);
    }

    Camera camera;
    camera_init(&camera);
    input_init();

    double last_time = get_time_s();
    bool running = true;
    
    // Grab the pointer
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

        int rendered_chunks = 0;
        for (int i = 0; i < num_chunks; i++) {
            if (frustum_intersects_box(&frustum, world.chunks[i].min, world.chunks[i].max)) {
                glBindVertexArray(meshes[i].vao);
                glDrawArrays(GL_TRIANGLES, 0, meshes[i].vertex_count);
                rendered_chunks++;
            }
        }
        // printf("Rendered chunks: %d\n", rendered_chunks);

        glXSwapBuffers(display, window);
    }

    XUngrabPointer(display, CurrentTime);
    for (int i = 0; i < num_chunks; i++) mesh_free(&meshes[i]);
    free(meshes);
    world_free(&world);
    glDeleteProgram(shader_program);
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, gl_context);
    XDestroyWindow(display, window);
    XCloseDisplay(display);

    return 0;
}
