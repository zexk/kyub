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
#include "platform.h"
#include "platform_x11.h"
#include <GL/glx.h>
#include <GL/glext.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>

static double get_time_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

typedef void (*PFNGLXSWAPINTERVALEXTPROC)(Display*, GLXDrawable, int);
static PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = NULL;

int main(void) {
    if (platform_init(800, 600) != 0) return 1;

    Display *display = platform_x11_get_display();
    Window window = platform_x11_get_window();

    int screen_id = DefaultScreen(display);
    int attributes[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, GLX_SAMPLE_BUFFERS, 1, GLX_SAMPLES, 4, None };
    XVisualInfo *visual_info = glXChooseVisual(display, screen_id, attributes);
    if (!visual_info) return 1;

    GLXContext gl_context = glXCreateContext(display, visual_info, NULL, GL_TRUE);
    glXMakeCurrent(display, window, gl_context);
    if (!gl_ext_init()) return 1;

    glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddress((const GLubyte*)"glXSwapIntervalEXT");
    if (!glXSwapIntervalEXT) {
        glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddress((const GLubyte*)"glXSwapIntervalMESA");
    }
    if (glXSwapIntervalEXT) {
        glXSwapIntervalEXT(display, window, 1);
    }

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

    UI ui;
    ui.render_distance = world.render_distance;
    ui_init(&ui, 800, 600);

    platform_hide_cursor(true);

    double last_time = get_time_s();
    double last_fps_update = 0.0;
    bool running = true;
    bool paused = false;
    int win_width = 800;
    int win_height = 600;

    platform_get_window_size(&win_width, &win_height);

    while (running) {
        platform_get_window_size(&win_width, &win_height);
        double now = get_time_s();

        double dt = now - last_time;
        last_time = now;

        nk_input_begin(&ui.ctx);
        Event event;
        while (platform_poll_event(&event)) {
            if (paused) {
                if (event.type == 1) {  // EVENT_KEY_DOWN
                    if (event.key.key == 'p') {
                        paused = false;
                        platform_hide_cursor(true);
                        platform_grab_mouse(true);
                    }
                    if (event.key.key == 0x1B) running = false;
                    ui_handle_key(&ui, event.key.key, true);
                } else if (event.type == 2) {  // EVENT_KEY_UP
                    ui_handle_key(&ui, event.key.key, false);
} else if (event.type == 3) {  // EVENT_MOUSE_BUTTON
                    ui_handle_mouse(&ui, event.mouse_button.x, event.mouse_button.y);
                    input_set_mouse_button(event.mouse_button.button, event.mouse_button.down);
                } else if (event.type == 4) {  // EVENT_MOUSE_MOTION
                    if (ui_is_visible(&ui)) {
                        ui_handle_mouse(&ui, event.mouse_motion.x, event.mouse_motion.y);
                    }
                } else if (event.type == 5) {  // EVENT_RESIZE
                    win_width = event.resize.width;
                    win_height = event.resize.height;
                    glViewport(0, 0, win_width, win_height);
                }
            } else {
                if (event.type == 1) {  // EVENT_KEY_DOWN
                    if (event.key.key == 'w') input_set_key('w', true);
                    if (event.key.key == 's') input_set_key('s', true);
                    if (event.key.key == 'a') input_set_key('a', true);
                    if (event.key.key == 'd') input_set_key('d', true);
                    if (event.key.key == 0x10) input_set_shift(true);
                    if (event.key.key == 'p') {
                        paused = true;
                        platform_grab_mouse(false);
                        platform_hide_cursor(false);
                    }
                    if (event.key.key == 0x1B) running = false;
                } else if (event.type == 2) {  // EVENT_KEY_UP
                    if (event.key.key == 'w') input_set_key('w', false);
                    if (event.key.key == 's') input_set_key('s', false);
                    if (event.key.key == 'a') input_set_key('a', false);
                    if (event.key.key == 'd') input_set_key('d', false);
                    if (event.key.key == 0x10) input_set_shift(false);
                } else if (event.type == 3) {  // EVENT_MOUSE_BUTTON
                    input_set_mouse_button(event.mouse_button.button, event.mouse_button.down);
                } else if (event.type == 4) {  // EVENT_MOUSE_MOTION
                    int dx = event.mouse_motion.x - win_width / 2;
                    int dy = event.mouse_motion.y - win_height / 2;
                    if (dx != 0 || dy != 0) {
                        input_set_mouse_delta((float)dx, (float)dy);
                        platform_warp_mouse(win_width / 2, win_height / 2);
                    }
                    if (ui_is_visible(&ui)) {
                        ui_handle_mouse(&ui, event.mouse_motion.x, event.mouse_motion.y);
                    }
                } else if (event.type == 5) {  // EVENT_RESIZE
                    win_width = event.resize.width;
                    win_height = event.resize.height;
                    glViewport(0, 0, win_width, win_height);
                } else if (event.type == 6) {  // EVENT_QUIT
                    running = false;
                }
            }
        }

        ui_poll_mouse(&ui);
        nk_input_end(&ui.ctx);

        camera_update(&camera, dt);
        world_update(&world, camera.pos);

        if (!paused) {
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
                            world_set_block(&world, prev_x, prev_y, prev_z, ui.selected_block);
                        break;
                }
                prev_x = bx; prev_y = by; prev_z = bz;
            }
        }
        prev_left = g_input.mouse_left;
        prev_right = g_input.mouse_right;
        }

        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader_program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atlas);
        glUniform1i(glGetUniformLocation(shader_program, "uTexture"), 0);

        mat4 model = mat4_identity();
        mat4 projection = mat4_perspective(45.0f * PI / 180.0f, (float)win_width / (float)win_height, 0.1f, 100.0f);
        mat4 view = camera_get_view_matrix(&camera);

        Frustum frustum;
        frustum_extract(&frustum, mat4_multiply(projection, view));

        glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, model.m);
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "view"), 1, GL_FALSE, view.m);
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "projection"), 1, GL_FALSE, projection.m);

        for (int i = 0; i < world.capacity; i++) {
            if (world.chunks[i].active && frustum_intersects_box(&frustum, world.chunks[i].chunk->min, world.chunks[i].chunk->max)) {
                glBindVertexArray(world.chunks[i].mesh->vao);
#ifdef ENABLE_COMPUTE
                glBindBuffer(GL_DRAW_INDIRECT_BUFFER, world.chunks[i].mesh->indirect_draw_buffer);
                glDrawArraysIndirect(GL_TRIANGLES, 0);
                glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
#else
                glDrawArrays(GL_TRIANGLES, 0, world.chunks[i].mesh->vertex_count);
#endif
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
        if (now - last_fps_update >= 0.5) {
            ui_set_stats(&ui, (int)(1.0f / dt + 0.5f), active_chunks, camera.pos, camera.front, camera.yaw, camera.pitch);
            last_fps_update = now;
        }

        ui_render(&ui, win_width, win_height);

        world.render_distance = ui.render_distance;

        glEnable(GL_DEPTH_TEST);

        glXSwapBuffers(display, window);

        if (glXSwapIntervalEXT) {
            static bool prev_fps_unlimited = false;
            if (ui.fps_unlimited != prev_fps_unlimited) {
                glXSwapIntervalEXT(display, window, ui.fps_unlimited ? 0 : 1);
                prev_fps_unlimited = ui.fps_unlimited;
            }
        }
    }

    ui_shutdown(&ui);
    world_free(&world);
    glDeleteProgram(shader_program);
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, gl_context);
    platform_shutdown();
    return 0;
}