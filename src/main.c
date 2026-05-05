#define _POSIX_C_SOURCE 199309L
#define NK_IMPLEMENTATION
#include "common.h"
#include "gl_ext.h"
#include "shader.h"
#include "voxel.h"
#include "mesh.h"
#include "math3d.h"
#include "camera.h"
#include "input.h"
#include "world.h"
#include "texture.h"
#include "ui.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <time.h>
#include <math.h>
#include <stdio.h>

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
    window_attributes.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;
    int width = 800, height = 600;
    Window window = XCreateWindow(display, RootWindow(display, screen_id), 0, 0, width, height, 0, visual_info->depth, InputOutput, visual_info->visual, CWColormap | CWEventMask, &window_attributes);
    XMapWindow(display, window);
    
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

    unsigned int hud_program = shader_create_program("shaders/hud.vert", "shaders/hud.frag");
    if (!hud_program) return 1;

    GLuint hud_vao, hud_vbo, test_vbo;
    glGenVertexArrays(1, &hud_vao);
    glGenBuffers(1, &hud_vbo);
    glGenBuffers(1, &test_vbo);
    float crosshair[] = {
        -0.015f, 0.0f,  0.015f, 0.0f,
        0.0f, -0.02f,  0.0f, 0.02f
    };
    glBindVertexArray(hud_vao);
    glBindBuffer(GL_ARRAY_BUFFER, hud_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(crosshair), crosshair, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(hud_vao);

    float test_tri[] = {0.0f, 0.2f,  -0.3f, -0.2f,  0.3f, -0.2f};
    glBindBuffer(GL_ARRAY_BUFFER, test_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(test_tri), test_tri, GL_STATIC_DRAW);

    GLuint atlas = texture_load("assets/atlas.png");

    World world;
    world_init(&world, 2);

    Camera camera;
    camera_init(&camera);
    input_init();

    XGrabPointer(display, window, True, PointerMotionMask, GrabModeAsync, GrabModeAsync, window, None, CurrentTime);

    UI ui;
    ui_init(&ui, width, height);

    double last_time = get_time_s();
    bool running = true;

    while (running) {
        double now = get_time_s();
        double dt = now - last_time;
        last_time = now;

        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == KeyPress) {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                if (keysym == XK_w) input_set_key('w', true);
                if (keysym == XK_s) input_set_key('s', true);
                if (keysym == XK_a) input_set_key('a', true);
                if (keysym == XK_d) input_set_key('d', true);
                if (keysym == XK_Shift_L || keysym == XK_Shift_R) input_set_shift(true);
                if (keysym == XK_F3) { g_input.key_f3 = true; ui_toggle(&ui); }
                if (keysym == XK_Escape) running = false;
            } else if (event.type == KeyRelease) {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                if (keysym == XK_w) input_set_key('w', false);
                if (keysym == XK_s) input_set_key('s', false);
                if (keysym == XK_a) input_set_key('a', false);
                if (keysym == XK_d) input_set_key('d', false);
                if (keysym == XK_Shift_L || keysym == XK_Shift_R) input_set_shift(false);
                if (keysym == XK_F3) g_input.key_f3 = false;
            } else if (event.type == ButtonPress) {
                input_set_mouse_button(event.xbutton.button, true);
                ui_handle_mouse(&ui, event.xbutton.x, event.xbutton.y, NK_BUTTON_LEFT, true);
            } else if (event.type == ButtonRelease) {
                input_set_mouse_button(event.xbutton.button, false);
                ui_handle_mouse(&ui, event.xbutton.x, event.xbutton.y, NK_BUTTON_LEFT, false);
            } else if (event.type == MotionNotify) {
                if (!ui_is_visible(&ui)) {
                    int dx = event.xmotion.x - width / 2;
                    int dy = event.xmotion.y - height / 2;
                    if (dx != 0 || dy != 0) {
                        input_set_mouse_delta((float)dx, (float)dy);
                        XWarpPointer(display, None, window, 0, 0, 0, 0, width / 2, height / 2);
                    }
                }
                if (ui_is_visible(&ui)) {
                    ui_handle_mouse(&ui, event.xmotion.x, event.xmotion.y, 0, false);
                }
            } else if (event.type == ClientMessage) {
                if ((Atom)event.xclient.data.l[0] == wm_delete_window) running = false;
            } else if (event.type == ConfigureNotify) {
                width = event.xconfigure.width;
                height = event.xconfigure.height;
                glViewport(0, 0, width, height);
            }
        }

        nk_input_end(&ui.ctx);

        camera_update(&camera, dt);
        world_update(&world, camera.pos);

        static bool prev_left = false, prev_right = false;
        if (g_input.mouse_left && !prev_left) {
            vec3 dir = camera.front;
            vec3 pos = camera.pos;
            int hit_x = -1, hit_y = -1, hit_z = -1;
            for (float t = 0; t < 8.0f; t += 0.05f) {
                vec3 p = vec3_add(pos, vec3_mul(dir, t));
                int bx = (int)floorf(p.x);
                int by = (int)floorf(p.y);
                int bz = (int)floorf(p.z);
                BlockType b = world_get_block(&world, bx, by, bz);
                if (b != BLOCK_AIR) {
                    hit_x = bx; hit_y = by; hit_z = bz;
                    break;
                }
            }
            if (hit_x >= 0) world_set_block(&world, hit_x, hit_y, hit_z, BLOCK_AIR);
        }
        if (g_input.mouse_right && !prev_right) {
            vec3 dir = camera.front;
            vec3 pos = camera.pos;
            int prev_x = -1, prev_y = -1, prev_z = -1;
            for (float t = 0; t < 8.0f; t += 0.05f) {
                vec3 p = vec3_add(pos, vec3_mul(dir, t));
                int bx = (int)floorf(p.x);
                int by = (int)floorf(p.y);
                int bz = (int)floorf(p.z);
                BlockType b = world_get_block(&world, bx, by, bz);
                if (b != BLOCK_AIR) {
                    if (prev_x >= 0 && world_get_block(&world, prev_x, prev_y, prev_z) == BLOCK_AIR)
                        world_set_block(&world, prev_x, prev_y, prev_z, BLOCK_STONE);
                    break;
                }
                prev_x = bx; prev_y = by; prev_z = bz;
            }
        }
        prev_left = g_input.mouse_left;
        prev_right = g_input.mouse_right;

        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(shader_program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atlas);
        glUniform1i(glGetUniformLocation(shader_program, "uTexture"), 0);

        mat4 model = mat4_identity();
        mat4 projection = mat4_perspective(45.0f * PI / 180.0f, (float)width / (float)height, 0.1f, 100.0f);
        mat4 view = camera_get_view_matrix(&camera);
        
        Frustum frustum;
        frustum_extract(&frustum, mat4_multiply(projection, view));

        glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, model.m);
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "view"), 1, GL_FALSE, view.m);
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "projection"), 1, GL_FALSE, projection.m);

        for (int i = 0; i < world.capacity; i++) {
            if (world.chunks[i].active && frustum_intersects_box(&frustum, world.chunks[i].chunk->min, world.chunks[i].chunk->max)) {
                glBindVertexArray(world.chunks[i].mesh->vao);
                glDrawArrays(GL_TRIANGLES, 0, world.chunks[i].mesh->vertex_count);
            }
        }

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(hud_program);
        glBindVertexArray(hud_vao);
        glBindBuffer(GL_ARRAY_BUFFER, hud_vbo);
        glUniform3f(glGetUniformLocation(hud_program, "uColor"), 0.7f, 0.7f, 0.7f);
        glUniform1f(glGetUniformLocation(hud_program, "uAlpha"), 1.0f);
        glDrawArrays(GL_LINES, 0, 4);
        glUseProgram(0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        int active_chunks = 0;
        for (int i = 0; i < world.capacity; i++) {
            if (world.chunks[i].active) active_chunks++;
        }
        ui_set_stats(&ui, 1.0f / dt, active_chunks, camera.pos, camera.front, camera.yaw, camera.pitch);

        ui_render(&ui, width, height);

        glEnable(GL_DEPTH_TEST);

        glXSwapBuffers(display, window);
    }

    ui_shutdown(&ui);
    world_free(&world);
    glDeleteProgram(shader_program);
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, gl_context);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}