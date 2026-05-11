#define _POSIX_C_SOURCE 199309L
#define NK_IMPLEMENTATION
#include "common.h"
#include "renderer.h"
#include "voxel.h"
#include "mesh.h"
#include "math3d.h"
#include "camera.h"
#include "input.h"
#include "world.h"
#include "texture.h"
#include "ui.h"
#include "platform.h"
#include "logger.h"
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>

static void crash_handler(int sig) {
    void *addrs[32];
    int n = backtrace(addrs, 32);
    fprintf(stderr, "\n=== CRASH (signal %d) ===\n", sig);
    backtrace_symbols_fd(addrs, n, STDERR_FILENO);
    fprintf(stderr, "========================\n");
    logger_shutdown();
    _Exit(1);
}

static double get_time_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(void) {
    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGBUS, crash_handler);

    if (platform_init(800, 600) != 0) return 1;

#ifdef ENABLE_LOGGER
    logger_init("kyub.log");
#endif

    if (!renderer_init(800, 600)) {
        fprintf(stderr, "Failed to initialize renderer\n");
        platform_shutdown();
#ifdef ENABLE_LOGGER
        logger_shutdown();
#endif
        return 1;
    }

    renderer_enable(R_CAP_DEPTH_TEST);
    renderer_enable(R_CAP_CULL_FACE);
    renderer_enable(R_CAP_MULTISAMPLE);

    R_Program shader_program = renderer_create_program("build/shaders/basic", "build/shaders/basic");
    if (shader_program == R_INVALID_HANDLE) return 1;

    R_Program hud_program = renderer_create_program("build/shaders/hud", "build/shaders/hud");
    if (hud_program == R_INVALID_HANDLE) return 1;

    R_Program skybox_program = renderer_create_program("build/shaders/skybox", "build/shaders/skybox");
    if (skybox_program == R_INVALID_HANDLE) return 1;

    R_Program outline_program = renderer_create_program("build/shaders/outline", "build/shaders/outline");
    if (outline_program == R_INVALID_HANDLE) return 1;

    R_VAO skybox_vao = renderer_create_vao();
    R_Buffer skybox_vbo = renderer_create_buffer();
    float skybox_cube[] = {
        -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,
    };
    renderer_bind_vao(skybox_vao);
    renderer_bind_buffer(R_BUF_ARRAY, skybox_vbo);
    renderer_buffer_data(R_BUF_ARRAY, sizeof(skybox_cube), skybox_cube, R_USAGE_STATIC);
    renderer_attrib_pointer(0, 3, R_TYPE_FLOAT, false, 0, 0);
    renderer_enable_attrib(0);
    renderer_bind_vao(R_INVALID_HANDLE);

    R_VAO outline_vao = renderer_create_vao();
    R_Buffer outline_vbo = renderer_create_buffer();
    float outline_cube[] = {
        0,0,0, 1,0,0,   1,0,0, 1,0,1,   1,0,1, 0,0,1,   0,0,1, 0,0,0,
        0,1,0, 1,1,0,   1,1,0, 1,1,1,   1,1,1, 0,1,1,   0,1,1, 0,1,0,
        0,0,0, 0,1,0,   1,0,0, 1,1,0,   1,0,1, 1,1,1,   0,0,1, 0,1,1,
    };
    renderer_bind_vao(outline_vao);
    renderer_bind_buffer(R_BUF_ARRAY, outline_vbo);
    renderer_buffer_data(R_BUF_ARRAY, sizeof(outline_cube), outline_cube, R_USAGE_STATIC);
    renderer_attrib_pointer(0, 3, R_TYPE_FLOAT, false, 0, 0);
    renderer_enable_attrib(0);
    renderer_bind_vao(R_INVALID_HANDLE);

    R_VAO overlay_vao = renderer_create_vao();
    R_Buffer overlay_vbo = renderer_create_buffer();
    float overlay_tri[] = { -1.0f,-1.0f,  3.0f,-1.0f,  -1.0f,3.0f };
    renderer_bind_vao(overlay_vao);
    renderer_bind_buffer(R_BUF_ARRAY, overlay_vbo);
    renderer_buffer_data(R_BUF_ARRAY, sizeof(overlay_tri), overlay_tri, R_USAGE_STATIC);
    renderer_attrib_pointer(0, 2, R_TYPE_FLOAT, false, 0, 0);
    renderer_enable_attrib(0);
    renderer_bind_vao(R_INVALID_HANDLE);

    R_VAO button_vao = renderer_create_vao();
    R_Buffer button_vbo = renderer_create_buffer();
    float button_tri[] = {
        -0.12f,-0.12f,  0.12f,-0.12f,  0.12f, 0.12f,
        -0.12f,-0.12f,  0.12f, 0.12f, -0.12f, 0.12f
    };
    renderer_bind_vao(button_vao);
    renderer_bind_buffer(R_BUF_ARRAY, button_vbo);
    renderer_buffer_data(R_BUF_ARRAY, sizeof(button_tri), button_tri, R_USAGE_STATIC);
    renderer_attrib_pointer(0, 2, R_TYPE_FLOAT, false, 0, 0);
    renderer_enable_attrib(0);
    renderer_bind_vao(R_INVALID_HANDLE);

    R_VAO hud_vao = renderer_create_vao();
    R_Buffer hud_vbo = renderer_create_buffer();
    R_Buffer test_vbo = renderer_create_buffer();
    float crosshair[] = {
        -0.015f, 0.0f,  0.015f, 0.0f,
        0.0f, -0.02f,  0.0f, 0.02f
    };
    renderer_bind_vao(hud_vao);
    renderer_bind_buffer(R_BUF_ARRAY, hud_vbo);
    renderer_buffer_data(R_BUF_ARRAY, sizeof(crosshair), crosshair, R_USAGE_STATIC);
    renderer_attrib_pointer(0, 2, R_TYPE_FLOAT, false, 2 * sizeof(float), 0);
    renderer_enable_attrib(0);
    renderer_bind_vao(hud_vao);

    float test_tri[] = {0.0f, 0.2f,  -0.3f, -0.2f,  0.3f, -0.2f};
    renderer_bind_buffer(R_BUF_ARRAY, test_vbo);
    renderer_buffer_data(R_BUF_ARRAY, sizeof(test_tri), test_tri, R_USAGE_STATIC);

    R_Texture atlas = texture_load("assets/atlas.png");
    if (atlas == R_INVALID_HANDLE) {
        fprintf(stderr, "Failed to load atlas texture\n");
        return 1;
    }

    World world;
    world_init(&world, 2);

    Camera camera;
    camera_init(&camera);
    input_init();

    /* Find safe spawn position - move camera above terrain */
    /* First update world to load chunks at initial position */
    world_update(&world, camera.pos);
    /* Wait a few frames for chunks to load */
    for (int i = 0; i < 10; i++) {
        world_update(&world, camera.pos);
    }
    /* Find ground height at spawn position */
    int spawn_x = (int)floorf(camera.pos.x);
    int spawn_z = (int)floorf(camera.pos.z);
    int ground_y = 0;
    for (int y = CHUNK_SIZE - 1; y >= 0; y--) {
        if (world_is_solid(&world, spawn_x, y, spawn_z)) {
            ground_y = y;
            break;
        }
    }
    /* Place camera 2 blocks above ground */
    camera.pos.y = (float)(ground_y + 2) + PLAYER_EYES_HEIGHT;
    LOG_INFO(CAT_PLATFORM, "Spawn position set to y=%.2f (ground at y=%d)", camera.pos.y, ground_y);

    UI ui;
    ui.render_distance = world.render_distance;
    ui_init(&ui, 800, 600);

    platform_hide_cursor(true);
    platform_grab_mouse(true);

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

        Event event;
        while (platform_poll_event(&event)) {
            if (event.type == 1) {  // EVENT_KEY_DOWN
                input_set_key(event.key.key, true);
                if (event.key.key == 0x10) input_set_shift(true);
                if (event.key.key == 'p') ui.visible = !ui.visible;
                if (event.key.key == 't') {
                    paused = !paused;
                    platform_hide_cursor(!paused);
                    platform_grab_mouse(!paused);
                }
                if (event.key.key == 0x1B) {
                    paused = !paused;
                    platform_hide_cursor(!paused);
                    platform_grab_mouse(!paused);
                }
            } else if (event.type == 2) {  // EVENT_KEY_UP
                input_set_key(event.key.key, false);
                if (event.key.key == 0x10) input_set_shift(false);
            } else if (event.type == 3) {  // EVENT_MOUSE_BUTTON
                input_set_mouse_button(event.mouse_button.button, event.mouse_button.down);
            } else if (event.type == 4) {  // EVENT_MOUSE_MOTION
                input_set_mouse_pos(event.mouse_motion.x, event.mouse_motion.y);
                if (!paused) {
                    int dx = event.mouse_motion.x - win_width / 2;
                    int dy = event.mouse_motion.y - win_height / 2;
                    if (dx != 0 || dy != 0) {
                        input_set_mouse_delta((float)dx, (float)dy);
                        platform_warp_mouse(win_width / 2, win_height / 2);
                    }
                }
            } else if (event.type == 5) {  // EVENT_RESIZE
                win_width = event.resize.width;
                win_height = event.resize.height;
                renderer_viewport(0, 0, win_width, win_height);
            } else if (event.type == 6) {  // EVENT_QUIT
                running = false;
            }
        }

#ifdef ENABLE_LOGGER
        if (g_input.keys['w'] || g_input.keys['a'] || g_input.keys['s'] || g_input.keys['d']) {
            LOG_DEBUG(CAT_INPUT, "main post-events: w=%d a=%d s=%d d=%d",
                g_input.keys['w'], g_input.keys['a'], g_input.keys['s'], g_input.keys['d']);
        }
        double timing_start = get_time_s();
        double frame_start = timing_start;
#define LOG_TIMING(name) do { \
    double _t = get_time_s(); \
    double _dt = _t - timing_start; \
    if (_dt > 0.001) LOG_DEBUG(CAT_PLATFORM, "TIMING %-20s %.3f ms", name, _dt * 1000.0); \
    timing_start = _t; \
} while(0)
#else
#define LOG_TIMING(name) ((void)0)
        double timing_start = 0;
        double frame_start = 0;
#endif

        nk_input_sync(&ui.ctx, g_input.mouse_x, g_input.mouse_y,
            g_input.mouse_left, g_input.mouse_right, 0,
            g_input.shift, g_input.keys[0x11], g_input.keys[0x12],
            g_input.keys[0x0D], g_input.keys[0x09], g_input.keys[0x08], g_input.keys[0x7F],
            g_input.keys[0x25], g_input.keys[0x27], g_input.keys[0x26], g_input.keys[0x28]);
        LOG_TIMING("nk_input_sync");

        if (paused) {
            static bool prev_pause_click = false;
            if (g_input.mouse_left && !prev_pause_click) {
                float mx = 2.0f * ui.ctx.input.mouse.pos.x / win_width - 1.0f;
                float my = 1.0f - 2.0f * ui.ctx.input.mouse.pos.y / win_height;
                if (mx >= -0.12f && mx <= 0.12f && my >= -0.12f && my <= 0.12f)
                    running = false;
            }
            prev_pause_click = g_input.mouse_left;
        }

        camera_update(&camera, dt, &world);
        LOG_TIMING("camera_update");

        static float break_cooldown = 0.0f;
        static float place_cooldown = 0.0f;
        break_cooldown -= (float)dt;
        place_cooldown -= (float)dt;

        if (!paused) {
            if (g_input.mouse_left && break_cooldown <= 0.0f) {
                vec3 dir = camera.front;
                vec3 pos = camera.pos;
                bool hit_found = false;
                int hit_x = 0, hit_y = 0, hit_z = 0;
                LOG_DEBUG(CAT_WORLD, "Break raycast from pos=%.2f,%.2f,%.2f dir=%.2f,%.2f,%.2f", pos.x, pos.y, pos.z, dir.x, dir.y, dir.z);
                for (float t = 0; t < 8.0f; t += 0.05f) {
                    vec3 p = vec3_add(pos, vec3_mul(dir, t));
                    int bx = (int)floorf(p.x);
                    int by = (int)floorf(p.y);
                    int bz = (int)floorf(p.z);
                    BlockType b = world_get_block(&world, bx, by, bz);
                    if (b != BLOCK_AIR) {
                        hit_found = true;
                        hit_x = bx; hit_y = by; hit_z = bz;
                        LOG_DEBUG(CAT_WORLD, "Break hit block at %d,%d,%d type=%d", bx, by, bz, b);
                        break;
                    }
                }
                if (hit_found) {
                    LOG_DEBUG(CAT_WORLD, "Break setting block %d,%d,%d to AIR", hit_x, hit_y, hit_z);
                    world_set_block(&world, hit_x, hit_y, hit_z, BLOCK_AIR);
                } else {
                    LOG_DEBUG(CAT_WORLD, "Break no block hit in range");
                }
                break_cooldown = 0.25f;
            }
            if (g_input.mouse_right && place_cooldown <= 0.0f) {
                vec3 dir = camera.front;
                vec3 pos = camera.pos;
                bool prev_found = false;
                int prev_x = 0, prev_y = 0, prev_z = 0;
                LOG_DEBUG(CAT_WORLD, "Place raycast from pos=%.2f,%.2f,%.2f dir=%.2f,%.2f,%.2f", pos.x, pos.y, pos.z, dir.x, dir.y, dir.z);
                for (float t = 0; t < 8.0f; t += 0.05f) {
                    vec3 p = vec3_add(pos, vec3_mul(dir, t));
                    int bx = (int)floorf(p.x);
                    int by = (int)floorf(p.y);
                    int bz = (int)floorf(p.z);
                    BlockType b = world_get_block(&world, bx, by, bz);
                    if (b != BLOCK_AIR) {
                        if (prev_found) {
                            BlockType prev_b = world_get_block(&world, prev_x, prev_y, prev_z);
                            LOG_DEBUG(CAT_WORLD, "Place hit block at %d,%d,%d type=%d, prev=%d,%d,%d type=%d", bx, by, bz, b, prev_x, prev_y, prev_z, prev_b);
                            if (prev_b == BLOCK_AIR) {
                                LOG_DEBUG(CAT_WORLD, "Place setting block %d,%d,%d to type=%d", prev_x, prev_y, prev_z, ui.selected_block);
                                world_set_block(&world, prev_x, prev_y, prev_z, ui.selected_block);
                            }
                        } else {
                            LOG_DEBUG(CAT_WORLD, "Place hit block at %d,%d,%d but no previous position", bx, by, bz);
                        }
                        break;
                    }
                    prev_found = true;
                    prev_x = bx; prev_y = by; prev_z = bz;
                }
                if (!prev_found) {
                    LOG_DEBUG(CAT_WORLD, "Place no block hit in range");
                }
                place_cooldown = 0.25f;
            }
        }

        LOG_TIMING("raycasts");

        // Block highlight raycast
        bool hl_found = false;
        int hl_x = 0, hl_y = 0, hl_z = 0;
        {
            vec3 dir = camera.front;
            vec3 pos = camera.pos;
            for (float t = 0; t < 8.0f; t += 0.05f) {
                vec3 p = vec3_add(pos, vec3_mul(dir, t));
                int bx = (int)floorf(p.x);
                int by = (int)floorf(p.y);
                int bz = (int)floorf(p.z);
                if (world_get_block(&world, bx, by, bz) != BLOCK_AIR) {
                    hl_found = true;
                    hl_x = bx; hl_y = by; hl_z = bz;
                    break;
                }
            }
        }

        renderer_clear(0.1f, 0.1f, 0.12f, 1.0f);
        LOG_TIMING("renderer_clear");

        /* Update world after fence wait to avoid GPU read/write race on buffers */
        world_update(&world, camera.pos);
        LOG_TIMING("world_update");

        renderer_use_program(shader_program);
        renderer_active_texture(0);
        renderer_bind_texture(R_TEX_2D, atlas);
        int tex_loc = renderer_uniform_location(shader_program, "uTexture");
        renderer_uniform_int(tex_loc, 0);
        int fog_color_loc = renderer_uniform_location(shader_program, "uFogColor");
        renderer_uniform_vec3(fog_color_loc, 0.53f, 0.81f, 0.92f);
        int fog_density_loc = renderer_uniform_location(shader_program, "uFogDensity");
        renderer_uniform_float(fog_density_loc, 0.015f);

        mat4 projection = mat4_perspective(45.0f * PI / 180.0f, (float)win_width / (float)win_height, 0.1f, 100.0f);
        mat4 view = camera_get_view_matrix(&camera);

        Frustum frustum;
        frustum_extract(&frustum, mat4_multiply(projection, view));

        int model_loc = renderer_uniform_location(shader_program, "model");
        int view_loc = renderer_uniform_location(shader_program, "view");
        renderer_uniform_mat4(view_loc, view.m);
        int proj_loc = renderer_uniform_location(shader_program, "projection");
        renderer_uniform_mat4(proj_loc, projection.m);

        for (int i = 0; i < world.capacity; i++) {
            if (world.chunks[i].active && frustum_intersects_box(&frustum, world.chunks[i].chunk->min, world.chunks[i].chunk->max)) {
                /* Model matrix is identity - vertices already contain world position */
                mat4 model = mat4_identity();
                renderer_uniform_mat4(model_loc, model.m);
                renderer_bind_vao(world.chunks[i].mesh->vao);
                renderer_draw_arrays(R_PRIM_TRIANGLES, 0, world.chunks[i].mesh->vertex_count);
            }
        }

        // Render block highlight outline
        if (hl_found) {
            renderer_use_program(outline_program);
            mat4 hl_model = mat4_translate((vec3){(float)hl_x, (float)hl_y, (float)hl_z});
            int ol_model_loc = renderer_uniform_location(outline_program, "model");
            renderer_uniform_mat4(ol_model_loc, hl_model.m);
            int ol_view_loc = renderer_uniform_location(outline_program, "view");
            renderer_uniform_mat4(ol_view_loc, view.m);
            int ol_proj_loc = renderer_uniform_location(outline_program, "projection");
            renderer_uniform_mat4(ol_proj_loc, projection.m);
            int ol_color_loc = renderer_uniform_location(outline_program, "uColor");
            renderer_uniform_vec3(ol_color_loc, 0.6f, 0.6f, 0.6f);

            renderer_depth_mask(false);
            renderer_polygon_offset(-1.0f, -1.0f);
            renderer_enable(R_CAP_POLYGON_OFFSET_LINE);
            renderer_line_width(3.0f);

            renderer_bind_vao(outline_vao);
            renderer_draw_arrays(R_PRIM_LINES, 0, 24);
            renderer_bind_vao(R_INVALID_HANDLE);

            renderer_line_width(1.0f);
            renderer_disable(R_CAP_POLYGON_OFFSET_LINE);
            renderer_depth_mask(true);
        }

        renderer_depth_mask(false);
        renderer_depth_func(R_FUNC_LEQUAL);
        renderer_disable(R_CAP_CULL_FACE);
        renderer_use_program(skybox_program);
        mat4 skybox_projection = mat4_perspective(45.0f * PI / 180.0f, (float)win_width / (float)win_height, 0.1f, 100.0f);
        int sb_proj_loc = renderer_uniform_location(skybox_program, "projection");
        renderer_uniform_mat4(sb_proj_loc, skybox_projection.m);
        int sb_view_loc = renderer_uniform_location(skybox_program, "view");
        renderer_uniform_mat4(sb_view_loc, view.m);
        renderer_bind_vao(skybox_vao);
        renderer_draw_arrays(R_PRIM_TRIANGLES, 0, 36);
        renderer_bind_vao(R_INVALID_HANDLE);
        renderer_enable(R_CAP_CULL_FACE);
        renderer_depth_func(R_FUNC_LESS);
        renderer_depth_mask(true);

        renderer_disable(R_CAP_DEPTH_TEST);
        renderer_enable(R_CAP_BLEND);
        renderer_blend_func(R_BLEND_SRC_ALPHA, R_BLEND_ONE_MINUS_SRC_ALPHA);
        renderer_use_program(hud_program);
        renderer_bind_vao(hud_vao);
        renderer_bind_buffer(R_BUF_ARRAY, hud_vbo);
        int hud_color_loc = renderer_uniform_location(hud_program, "uColor");
        renderer_uniform_vec3(hud_color_loc, 0.7f, 0.7f, 0.7f);
        int hud_alpha_loc = renderer_uniform_location(hud_program, "uAlpha");
        renderer_uniform_float(hud_alpha_loc, 1.0f);
        renderer_draw_arrays(R_PRIM_LINES, 0, 4);
        renderer_use_program(R_INVALID_HANDLE);
        renderer_bind_vao(R_INVALID_HANDLE);
        renderer_bind_buffer(R_BUF_ARRAY, R_INVALID_HANDLE);

        if (paused) {
            renderer_use_program(hud_program);
            int p_color_loc = renderer_uniform_location(hud_program, "uColor");
            renderer_uniform_vec3(p_color_loc, 0.0f, 0.0f, 0.0f);
            int p_alpha_loc = renderer_uniform_location(hud_program, "uAlpha");
            renderer_uniform_float(p_alpha_loc, 0.5f);
            renderer_bind_vao(overlay_vao);
            renderer_draw_arrays(R_PRIM_TRIANGLES, 0, 3);
            renderer_uniform_vec3(p_color_loc, 1.0f, 1.0f, 1.0f);
            renderer_uniform_float(p_alpha_loc, 1.0f);
            renderer_bind_vao(button_vao);
            renderer_draw_arrays(R_PRIM_TRIANGLES, 0, 6);
            renderer_bind_vao(R_INVALID_HANDLE);
            renderer_use_program(R_INVALID_HANDLE);
        }

        int active_chunks = 0;
        for (int i = 0; i < world.capacity; i++) {
            if (world.chunks[i].active) active_chunks++;
        }
        if (now - last_fps_update >= 0.5) {
            ui_set_stats(&ui, (int)(1.0f / dt + 0.5f), active_chunks, camera.pos, camera.front, camera.yaw, camera.pitch);
            last_fps_update = now;
        }

        world.render_distance = ui.render_distance;

        renderer_enable(R_CAP_DEPTH_TEST);
        LOG_TIMING("rendering");

        renderer_swap();
        LOG_TIMING("renderer_swap");

        ui_render(&ui, win_width, win_height);
        LOG_TIMING("ui_render");

        renderer_swap_interval(ui.fps_unlimited ? 0 : 1);
        LOG_TIMING("swap_interval");

#ifdef ENABLE_LOGGER
        double frame_end = get_time_s();
        double frame_dt = frame_end - frame_start;
        if (frame_dt > 0.1) {
            LOG_WARN(CAT_PLATFORM, "SLOW FRAME: %.3f ms", frame_dt * 1000.0);
        }
#endif
    }

    ui_shutdown(&ui);
    world_free(&world);
    renderer_destroy_program(shader_program);
    renderer_destroy_program(hud_program);
    renderer_destroy_program(skybox_program);
    renderer_destroy_program(outline_program);
    renderer_destroy_vao(skybox_vao);
    renderer_destroy_buffer(skybox_vbo);
    renderer_destroy_vao(outline_vao);
    renderer_destroy_buffer(outline_vbo);
    renderer_destroy_vao(overlay_vao);
    renderer_destroy_buffer(overlay_vbo);
    renderer_destroy_vao(button_vao);
    renderer_destroy_buffer(button_vbo);
    renderer_destroy_vao(hud_vao);
    renderer_destroy_buffer(hud_vbo);
    renderer_destroy_buffer(test_vbo);
    renderer_destroy_texture(atlas);
    renderer_shutdown();

#ifdef ENABLE_LOGGER
    logger_shutdown();
#endif

    platform_shutdown();
    return 0;
}