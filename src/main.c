#include "renderer.h"
#include "platform.h"
#include "core.h"
#include "timer.h"
#include "noise.h"
#include "linalg.h"
#include "log.h"
#include "input.h"
#include "fps_camera.h"
#include "ui.h"
#include "ui_gl.h"
#include "physics.h"
#include "kv.h"
#include "voxel.h"
#include "inventory.h"
#include "components.h"
#include "systems.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_WIDTH         800
#define WINDOW_HEIGHT        600
#define RAYCAST_MAX_DISTANCE 8.0f
#define COOLDOWN_TIME        0.25f
#define FOV_DEFAULT          45.0f
#define NEAR_PLANE           0.1f
#define FAR_PLANE            800.0f
#define FOG_COLOR_R          0.53f
#define FOG_COLOR_G          0.81f
#define FOG_COLOR_B          0.92f
#define FOG_DENSITY          0.005f

typedef enum { PAUSE_MAIN = 0, PAUSE_OPTIONS } PauseScreen;

static bool ray_solid_cb(void *ctx, int x, int y, int z) {
    return kv_world_get_block((kv_world_t *)ctx, x, y, z) != KV_BLOCK_AIR;
}

static bool position_is_safe(kv_world_t *world, vec3_t pos) {
    float hw   = PLAYER_HALF_WIDTH;
    float miny = pos.y - PLAYER_EYES_HEIGHT + 0.01f;
    float maxy = pos.y + (PLAYER_HEIGHT - PLAYER_EYES_HEIGHT) - 0.01f;
    for (int y = (int)floorf(miny); y <= (int)floorf(maxy); y++) {
        if (kv_world_is_solid(world, (int)floorf(pos.x-hw), y, (int)floorf(pos.z-hw))) return false;
        if (kv_world_is_solid(world, (int)floorf(pos.x-hw), y, (int)floorf(pos.z+hw))) return false;
        if (kv_world_is_solid(world, (int)floorf(pos.x+hw), y, (int)floorf(pos.z-hw))) return false;
        if (kv_world_is_solid(world, (int)floorf(pos.x+hw), y, (int)floorf(pos.z+hw))) return false;
    }
    return true;
}

typedef struct { int x, y, z; } BlockPos;

static bool player_collides_block(vec3_t player_pos, BlockPos b) {
    float hw    = PLAYER_HALF_WIDTH;
    float p_minx=player_pos.x-hw,      p_maxx=player_pos.x+hw;
    float p_minz=player_pos.z-hw,      p_maxz=player_pos.z+hw;
    float p_miny=player_pos.y-PLAYER_EYES_HEIGHT;
    float p_maxy=player_pos.y+(PLAYER_HEIGHT-PLAYER_EYES_HEIGHT);
    return (p_minx<b.x+1&&p_maxx>b.x) &&
           (p_miny<b.y+1&&p_maxy>b.y) &&
           (p_minz<b.z+1&&p_maxz>b.z);
}

int main(void) {
    core_install_crash_handler();
    noise_init(42);

#ifdef ENABLE_LOGGER
    log_init("kyub.log");
#endif

    window_t *win = window_create("Kyub", WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!win) { fprintf(stderr, "Failed to create window\n"); return 1; }

    uint32_t init_w, init_h;
    window_size(win, &init_w, &init_h);

    platform_native_handles_t nh = window_get_native_handles(win);
    if (!renderer_init((int)init_w, (int)init_h, &nh)) {
        fprintf(stderr, "Failed to initialize renderer\n");
        window_destroy(win); return 1;
    }

    renderer_viewport(0, 0, (int)init_w, (int)init_h);
    renderer_swap_interval(0);
    renderer_enable(R_CAP_DEPTH_TEST);
    renderer_enable(R_CAP_CULL_FACE);
    renderer_enable(R_CAP_MULTISAMPLE);

    /* ── ECS ─────────────────────────────────────────────────────────────── */
    g_ecs = world_create();
    components_init(g_ecs);

    /* ── Blocks + Items ─────────────────────────────────────────────────── */
    kyub_blocks_register();
    R_Texture tex_array = kv_build_texture_array(16);
    if (tex_array == R_INVALID_HANDLE) { fprintf(stderr, "Failed to build texture array\n"); return 1; }
    kyub_items_register();

    /* ── Skybox ──────────────────────────────────────────────────────────── */
    R_Program skybox_program = renderer_create_program("shaders/skybox.vert", "shaders/skybox.frag");
    if (skybox_program == R_INVALID_HANDLE) { fprintf(stderr, "Failed to load skybox shader\n"); return 1; }
    R_VAO    skybox_vao = renderer_create_vao();
    R_Buffer skybox_vbo = renderer_create_buffer();
    float skybox_tri[] = { -1.0f,-1.0f,  3.0f,-1.0f,  -1.0f,3.0f };
    renderer_bind_vao(skybox_vao);
    renderer_bind_buffer(R_BUF_ARRAY, skybox_vbo);
    renderer_buffer_data(R_BUF_ARRAY, sizeof(skybox_tri), skybox_tri, R_USAGE_STATIC);
    renderer_attrib_pointer(0, 2, R_TYPE_FLOAT, false, 8, 0);
    renderer_enable_attrib(0);
    renderer_bind_vao(R_INVALID_HANDLE);

    /* ── Block outline ───────────────────────────────────────────────────── */
    R_Program outline_program = renderer_create_program("shaders/outline.vert", "shaders/outline.frag");
    if (outline_program == R_INVALID_HANDLE) { fprintf(stderr, "Failed to load outline shader\n"); return 1; }
    R_VAO    outline_vao = renderer_create_vao();
    R_Buffer outline_vbo = renderer_create_buffer();
    float outline_cube[] = {
        0,0,0, 1,0,0,   1,0,0, 1,0,1,   1,0,1, 0,0,1,   0,0,1, 0,0,0,
        0,1,0, 1,1,0,   1,1,0, 1,1,1,   1,1,1, 0,1,1,   0,1,1, 0,1,0,
        0,0,0, 0,1,0,   1,0,0, 1,1,0,   1,0,1, 1,1,1,   0,0,1, 0,1,1,
    };
    renderer_bind_vao(outline_vao);
    renderer_bind_buffer(R_BUF_ARRAY, outline_vbo);
    renderer_buffer_data(R_BUF_ARRAY, sizeof(outline_cube), outline_cube, R_USAGE_STATIC);
    renderer_attrib_pointer(0, 3, R_TYPE_FLOAT, false, 12, 0);
    renderer_enable_attrib(0);
    renderer_bind_vao(R_INVALID_HANDLE);

    /* ── Dim overlay ─────────────────────────────────────────────────────── */
    R_Program hud_program = renderer_create_program("shaders/hud.vert", "shaders/hud.frag");
    if (hud_program == R_INVALID_HANDLE) { fprintf(stderr, "Failed to load HUD shader\n"); return 1; }
    R_VAO    overlay_vao = renderer_create_vao();
    R_Buffer overlay_vbo = renderer_create_buffer();
    float overlay_tri[] = { -1.0f,-1.0f,  3.0f,-1.0f,  -1.0f,3.0f };
    renderer_bind_vao(overlay_vao);
    renderer_bind_buffer(R_BUF_ARRAY, overlay_vbo);
    renderer_buffer_data(R_BUF_ARRAY, sizeof(overlay_tri), overlay_tri, R_USAGE_STATIC);
    renderer_attrib_pointer(0, 2, R_TYPE_FLOAT, false, 8, 0);
    renderer_enable_attrib(0);
    renderer_bind_vao(R_INVALID_HANDLE);

    /* ── Crosshair ───────────────────────────────────────────────────────── */
    R_VAO    hud_vao = renderer_create_vao();
    R_Buffer hud_vbo = renderer_create_buffer();
    float hw2=0.015f, hh2=0.0015f, vw=0.0015f, vh=0.020f;
    float crosshair[] = {
        -hw2,-hh2,  hw2,-hh2,  hw2, hh2,  -hw2,-hh2,  hw2, hh2,  -hw2, hh2,
        -vw, -vh,   vw, -vh,   vw,  vh,   -vw, -vh,   vw,  vh,   -vw,  vh,
    };
    renderer_bind_vao(hud_vao);
    renderer_bind_buffer(R_BUF_ARRAY, hud_vbo);
    renderer_buffer_data(R_BUF_ARRAY, sizeof(crosshair), crosshair, R_USAGE_STATIC);
    renderer_attrib_pointer(0, 2, R_TYPE_FLOAT, false, 2*sizeof(float), 0);
    renderer_enable_attrib(0);
    renderer_bind_vao(R_INVALID_HANDLE);

    /* ── Inventory ───────────────────────────────────────────────────────── */
    inv_renderer_t inv_r;
    if (!inv_renderer_init(&inv_r)) { fprintf(stderr, "Failed to init inventory renderer\n"); return 1; }
    kyub_inventory_t player_inv;
    inv_init(&player_inv);

    ui_gl_t gui;
    if (!ui_gl_init(&gui)) { fprintf(stderr, "Failed to init UI renderer\n"); return 1; }
    ui_t debug_ui;
    ui_init(&debug_ui);
    const ui_draw_t game_draw = ui_gl_draw(&gui);

    /* ── World ───────────────────────────────────────────────────────────── */
    kv_world_t *world = kv_world_create(6, 2, kyub_terrain_gen, NULL, "saves/default");

    fps_camera_t camera;
    fps_camera_init(&camera);

    entity_t player = entity_create(g_ecs);
    C_Transform *pt = entity_add_component(g_ecs, player, COMP_TRANSFORM);
    pt->position = (vec3_t){8.0f, 20.0f, 8.0f};
    pt->yaw = -90.0f; pt->pitch = -45.0f;
    C_Movement *pm = entity_add_component(g_ecs, player, COMP_MOVEMENT);
    pm->velocity = (vec3_t){0}; pm->speed = 5.0f; pm->grounded = false;
    C_Health *ph = entity_add_component(g_ecs, player, COMP_HEALTH);
    ph->current = ph->max = 20.0f;

    /* Initial chunk load + spawn point */
    vec3_t player_pos = pt->position;
    for (int i=0;i<10;i++) kv_world_update(world, player_pos);

    /* Find ground below spawn */
    {
        int sx=(int)floorf(player_pos.x), sz=(int)floorf(player_pos.z);
        int ground_y=0;
        for (int y=64;y>=-64;y--) {
            uint16_t b=kv_world_get_block(world,sx,y,sz);
            if (b!=KV_BLOCK_AIR&&b!=BLOCK_WATER) { ground_y=y; break; }
        }
        vec3_t spawn={(float)player_pos.x, (float)(ground_y+2)+PLAYER_EYES_HEIGHT, (float)player_pos.z};
        while (!position_is_safe(world,spawn) && spawn.y < 80.0f) spawn.y += 1.0f;
        pt->position = spawn;
        player_pos   = spawn;
        LOG_INFO(LOG_CAT_PLATFORM,"Spawn at y=%.2f (ground at y=%d)", (double)spawn.y, ground_y);
    }

    int         render_distance = 6;
    float       fov_degrees = FOV_DEFAULT;
    PauseScreen pause_screen = PAUSE_MAIN;
    bool        inv_open = false;

    window_set_cursor_mode(win, CURSOR_DISABLED);

    double last_time       = kln_timer_now();
    double last_save_flush = 0.0;
    bool   running  = true;
    bool   paused   = false;
    bool   show_debug = false;
    float  fps      = 0.0f;
    double fps_acc  = 0.0;
    int    fps_frames = 0;
    uint32_t win_width=WINDOW_WIDTH, win_height=WINDOW_HEIGHT;
    window_size(win, &win_width, &win_height);

    game_input_t game_input; game_input_init(&game_input);

    while (running) {
        window_size(win, &win_width, &win_height);
        double now = kln_timer_now();
        double dt  = now - last_time;
        last_time  = now;

        fps_acc += dt; fps_frames++;
        if (fps_acc >= 0.5) { fps=(float)(fps_frames/fps_acc); fps_acc=0; fps_frames=0; }

        event_t event;
        while (window_poll_event(win, &event)) {
            game_input_handle_event(&game_input, &event);

            switch (event.type) {
            case EVENT_KEY_DOWN:
                if (event.key.keysym == 'e' || event.key.keysym == 'E') {
                    if (!paused) {
                        inv_open = !inv_open;
                        window_set_cursor_mode(win, inv_open ? CURSOR_NORMAL : CURSOR_DISABLED);
                    }
                }
                if (event.key.keysym=='t' || event.key.code==KEY_ESCAPE) {
                    if (inv_open) {
                        inv_open = false;
                        window_set_cursor_mode(win, CURSOR_DISABLED);
                    } else if (paused && pause_screen==PAUSE_OPTIONS) {
                        pause_screen = PAUSE_MAIN;
                    } else {
                        paused = !paused;
                        if (!paused) pause_screen = PAUSE_MAIN;
                        window_set_cursor_mode(win, paused ? CURSOR_NORMAL : CURSOR_DISABLED);
                    }
                }
                if (event.key.keysym==0xFFC0) show_debug=!show_debug; /* F3 */
                /* Hotbar pin while inventory open: 1–9 keys */
                if (inv_open && event.key.keysym>='1' && event.key.keysym<='9') {
                    int hpos = event.key.keysym - '1';
                    int hsl  = inv_hovered_slot((float)game_input.mouse_x, (float)game_input.mouse_y,
                                                (float)win_width, (float)win_height);
                    if (hsl >= 0) inv_hotbar_pin(&player_inv, hpos, hsl);
                }
                break;
            case EVENT_SCROLL:
                if (!paused && !inv_open && event.scroll.delta!=0.0f)
                    inv_hotbar_cycle(&player_inv, event.scroll.delta > 0.0f ? 1 : -1);
                break;
            case EVENT_RESIZE:
                win_width=event.resize.width; win_height=event.resize.height;
                renderer_viewport(0,0,(int)win_width,(int)win_height);
                break;
            case EVENT_QUIT:
                running=false;
                break;
            default:
                break;
            }
        }

        ui_gl_begin(&gui,(int)win_width,(int)win_height,(int)game_input.mouse_x,(int)game_input.mouse_y,game_input.mouse_left);

        if (paused || inv_open) {
            sys_movement(g_ecs, world, 0.0f);
        } else {
            fps_camera_rotate(&camera, game_input.mouse_dx, game_input.mouse_dy);
            game_input.mouse_dx = 0; game_input.mouse_dy = 0;

            C_Transform *ct = entity_get_component(g_ecs, player, COMP_TRANSFORM);
            C_Movement  *cm = entity_get_component(g_ecs, player, COMP_MOVEMENT);
            if (ct && cm) {
                float speed = cm->speed;
                if (game_input.keys[0xe1]) speed *= 6.0f;
                vec3_t right = vec3_normalize(vec3_cross(camera.front, camera.up));
                vec3_t move  = {0};
                if (game_input.keys['w']) move = vec3_add(move, camera.front);
                if (game_input.keys['s']) move = vec3_sub(move, camera.front);
                if (game_input.keys['a']) move = vec3_sub(move, right);
                if (game_input.keys['d']) move = vec3_add(move, right);
                move.y = 0.0f;
                if (move.x!=0.0f || move.z!=0.0f) {
                    move = vec3_normalize(move);
                    cm->velocity.x = move.x * speed;
                    cm->velocity.z = move.z * speed;
                } else {
                    cm->velocity.x = cm->velocity.z = 0.0f;
                }
                if (game_input.keys[' '] && cm->grounded) {
                    cm->velocity.y = JUMP_VELOCITY;
                    cm->grounded   = false;
                }
            }
            sys_movement(g_ecs, world, (float)dt);
        }

        static float break_cd = 0.0f, place_cd = 0.0f;
        break_cd -= (float)dt; place_cd -= (float)dt;

        C_Transform *player_transform = entity_get_component(g_ecs, player, COMP_TRANSFORM);
        vec3_t cam_pos = player_transform ? player_transform->position : (vec3_t){0};

        if (!paused && !inv_open) {
            if (game_input.mouse_left && break_cd<=0.0f) {
                phys_raycast_hit_t h = phys_raycast_voxel(cam_pos, camera.front, RAYCAST_MAX_DISTANCE, ray_solid_cb, world);
                if (h.hit) {
                    uint16_t broken = kv_world_get_block(world, h.x, h.y, h.z);
                    kv_world_set_block(world, h.x, h.y, h.z, KV_BLOCK_AIR);
                    uint16_t drop = kyub_item_for_block(broken);
                    if (drop != ITEM_NONE) inv_add(&player_inv, (kyub_item_t){drop, 1, 0, 0});
                }
                break_cd = COOLDOWN_TIME;
            }
            if (game_input.mouse_right && place_cd<=0.0f) {
                kyub_item_t sel = inv_selected(&player_inv);
                if (sel.type != ITEM_NONE) {
                    const kyub_item_def_t *def = kyub_item_get(sel.type);
                    if (def && def->block_type != KV_BLOCK_AIR) {
                        phys_raycast_hit_t h = phys_raycast_voxel(cam_pos, camera.front, RAYCAST_MAX_DISTANCE, ray_solid_cb, world);
                        if (h.hit) {
                            BlockPos prev = {h.x+h.nx, h.y+h.ny, h.z+h.nz};
                            if (kv_world_get_block(world,prev.x,prev.y,prev.z)==KV_BLOCK_AIR &&
                                !player_collides_block(cam_pos, prev)) {
                                kv_world_set_block(world, prev.x, prev.y, prev.z, def->block_type);
                                inv_remove_at(&player_inv, inv_active_slot(&player_inv), 1);
                            }
                        }
                    }
                }
                place_cd = COOLDOWN_TIME;
            }
        }

        phys_raycast_hit_t hl_hit = phys_raycast_voxel(cam_pos, camera.front, RAYCAST_MAX_DISTANCE, ray_solid_cb, world);
        bool hl_found = hl_hit.hit;

        renderer_enable(R_CAP_DEPTH_TEST);
        renderer_enable(R_CAP_CULL_FACE);
        renderer_disable(R_CAP_BLEND);
        renderer_clear(0.1f,0.1f,0.12f,1.0f);

        kv_world_update(world, cam_pos);

        float aspect = (win_height>0) ? (float)win_width/(float)win_height : 1.0f;
        mat4_t projection = mat4_perspective(fov_degrees*KLN_PI/180.0f, aspect, NEAR_PLANE, FAR_PLANE);
        mat4_t view       = fps_camera_view(&camera, cam_pos);

        /* ── Voxel world ─────────────────────────────────────────────────── */
        kv_world_draw(world, tex_array, view, projection,
                      (vec3_t){FOG_COLOR_R, FOG_COLOR_G, FOG_COLOR_B}, FOG_DENSITY);

        /* ── Block highlight ─────────────────────────────────────────────── */
        if (hl_found) {
            renderer_use_program(outline_program);
            mat4_t hl_model = mat4_translation((vec3_t){(float)hl_hit.x,(float)hl_hit.y,(float)hl_hit.z});
            renderer_uniform_mat4(renderer_uniform_location(outline_program,"model"), hl_model.m);
            renderer_uniform_mat4(renderer_uniform_location(outline_program,"view"),  view.m);
            renderer_uniform_mat4(renderer_uniform_location(outline_program,"projection"), projection.m);
            renderer_uniform_vec3(renderer_uniform_location(outline_program,"uColor"), 0.6f,0.6f,0.6f);
            renderer_depth_mask(false);
            renderer_polygon_offset(-1.0f,-1.0f);
            renderer_enable(R_CAP_POLYGON_OFFSET_LINE);
            renderer_line_width(3.0f);
            renderer_bind_vao(outline_vao);
            renderer_draw_arrays(R_PRIM_LINES, 0, 24);
            renderer_bind_vao(R_INVALID_HANDLE);
            renderer_line_width(1.0f);
            renderer_disable(R_CAP_POLYGON_OFFSET_LINE);
            renderer_depth_mask(true);
        }

        /* ── Skybox ──────────────────────────────────────────────────────── */
        renderer_depth_mask(false);
        renderer_depth_func(R_FUNC_LEQUAL);
        renderer_disable(R_CAP_CULL_FACE);
        renderer_use_program(skybox_program);
        mat4_t inv_proj     = mat4_inverse(projection);
        mat4_t view_rot     = view; view_rot.m[12]=view_rot.m[13]=view_rot.m[14]=0;
        mat4_t inv_view_rot = mat4_transpose(view_rot);
        renderer_uniform_mat4(renderer_uniform_location(skybox_program,"inv_projection"),    inv_proj.m);
        renderer_uniform_mat4(renderer_uniform_location(skybox_program,"inv_view_rotation"), inv_view_rot.m);
        renderer_bind_vao(skybox_vao);
        renderer_draw_arrays(R_PRIM_TRIANGLES, 0, 3);
        renderer_bind_vao(R_INVALID_HANDLE);
        renderer_enable(R_CAP_CULL_FACE);
        renderer_depth_func(R_FUNC_LESS);
        renderer_depth_mask(true);

        /* ── HUD ─────────────────────────────────────────────────────────── */
        renderer_disable(R_CAP_DEPTH_TEST);
        renderer_enable(R_CAP_BLEND);
        renderer_blend_func(R_BLEND_SRC_ALPHA, R_BLEND_ONE_MINUS_SRC_ALPHA);
        renderer_use_program(hud_program);
        renderer_bind_vao(hud_vao);
        renderer_uniform_vec3(renderer_uniform_location(hud_program,"uColor"), 0.7f,0.7f,0.7f);
        renderer_uniform_float(renderer_uniform_location(hud_program,"uAlpha"), 1.0f);
        renderer_draw_arrays(R_PRIM_TRIANGLES, 0, 12);

        inv_draw_hotbar(&inv_r, &player_inv, tex_array, hud_program,
                        &gui, (float)win_width, (float)win_height);

        /* ── Pause ───────────────────────────────────────────────────────── */
        if (paused) {
            renderer_use_program(hud_program);
            renderer_uniform_vec3(renderer_uniform_location(hud_program,"uColor"),0.0f,0.0f,0.0f);
            renderer_uniform_float(renderer_uniform_location(hud_program,"uAlpha"),0.45f);
            renderer_bind_vao(overlay_vao);
            renderer_draw_arrays(R_PRIM_TRIANGLES,0,3);
            renderer_bind_vao(R_INVALID_HANDLE);

#define BTN_W 240.0f
#define BTN_H 48.0f
#define BTN_GAP 14.0f
            float cx2=(float)win_width*0.5f, cy2=(float)win_height*0.5f;
            float bx=cx2-BTN_W*0.5f;

            if (pause_screen==PAUSE_MAIN) {
                float total_h=3.0f*BTN_H+2.0f*BTN_GAP, by=cy2-total_h*0.5f;
                float title_scale=4.0f, tw=ui_gl_text_width("PAUSED",title_scale), th=7.0f*title_scale;
                ui_gl_text(&gui,cx2-tw*0.5f,by-th-18.0f,"PAUSED",title_scale,0.95f,0.95f,0.95f);
                if (ui_gl_button(&gui,bx,by,BTN_W,BTN_H,"resume")) { paused=false; pause_screen=PAUSE_MAIN; window_set_cursor_mode(win,CURSOR_DISABLED); }
                by+=BTN_H+BTN_GAP;
                if (ui_gl_button(&gui,bx,by,BTN_W,BTN_H,"options")) pause_screen=PAUSE_OPTIONS;
                by+=BTN_H+BTN_GAP;
                if (ui_gl_button(&gui,bx,by,BTN_W,BTN_H,"quit game")) running=false;
            } else {
                float title_scale=4.0f, tw=ui_gl_text_width("OPTIONS",title_scale), th=7.0f*title_scale;
                float panel_w=280.0f, panel_x=cx2-panel_w*0.5f, panel_y=cy2-130.0f;
                ui_gl_text(&gui,cx2-tw*0.5f,panel_y-th-12.0f,"OPTIONS",title_scale,0.95f,0.95f,0.95f);
                ui_input_t ui_in={.mouse_x=game_input.mouse_x,.mouse_y=game_input.mouse_y,.mouse_down=game_input.mouse_left,.pointer_valid=true};
                ui_begin(&debug_ui,&ui_in,(float)win_width,(float)win_height,&game_draw);
                ui_panel_begin(&debug_ui,panel_x,panel_y,panel_w);
                ui_slider_int(&debug_ui,"render dist",&render_distance,1,16);
                ui_slider_float(&debug_ui,"sensitivity",&camera.sensitivity,0.0005f,0.005f);
                ui_slider_float(&debug_ui,"FOV",&fov_degrees,30.0f,110.0f);
                ui_panel_end(&debug_ui);
                ui_end(&debug_ui);
                float back_w=140.0f;
                if (ui_gl_button(&gui,cx2-back_w*0.5f,panel_y+200.0f,back_w,BTN_H,"back"))
                    pause_screen=PAUSE_MAIN;
            }
#undef BTN_W
#undef BTN_H
#undef BTN_GAP
            renderer_use_program(R_INVALID_HANDLE);
        }

        /* ── Inventory screen ────────────────────────────────────────────── */
        if (inv_open) {
            int hsl = inv_hovered_slot((float)game_input.mouse_x, (float)game_input.mouse_y,
                                       (float)win_width, (float)win_height);
            inv_draw_screen(&inv_r, &player_inv, tex_array, hud_program,
                            &gui, (float)win_width, (float)win_height, hsl);
        }

        /* ── Debug (F3) ──────────────────────────────────────────────────── */
        if (show_debug && !paused) {
            renderer_disable(R_CAP_DEPTH_TEST);
            renderer_enable(R_CAP_BLEND);
            renderer_blend_func(R_BLEND_SRC_ALPHA, R_BLEND_ONE_MINUS_SRC_ALPHA);
            renderer_use_program(hud_program);
            ui_input_t ui_in={.mouse_x=game_input.mouse_x,.mouse_y=game_input.mouse_y,.mouse_down=game_input.mouse_left,.pointer_valid=paused};
            ui_begin(&debug_ui,&ui_in,(float)win_width,(float)win_height,&game_draw);
            ui_panel_begin(&debug_ui,10.0f,10.0f,240.0f);
            ui_text(&debug_ui,"%.0f fps",(double)fps);
            ui_text(&debug_ui,"pos  %.1f %.1f %.1f",(double)cam_pos.x,(double)cam_pos.y,(double)cam_pos.z);
            ui_separator(&debug_ui);
            ui_slider_int(&debug_ui,"render dist",&render_distance,1,16);
            ui_panel_end(&debug_ui);
            ui_end(&debug_ui);
            renderer_enable(R_CAP_DEPTH_TEST);
        }

        if (now-last_save_flush>=5.0) { kv_world_flush_saves(world); last_save_flush=now; }

        renderer_enable(R_CAP_DEPTH_TEST);
        renderer_swap();
        game_input_end_frame(&game_input);
    }

    kv_world_flush_saves(world);
    kv_world_destroy(world);

    renderer_destroy_program(skybox_program);
    renderer_destroy_program(outline_program);
    renderer_destroy_program(hud_program);
    renderer_destroy_vao(skybox_vao);   renderer_destroy_buffer(skybox_vbo);
    renderer_destroy_vao(outline_vao);  renderer_destroy_buffer(outline_vbo);
    renderer_destroy_vao(overlay_vao);  renderer_destroy_buffer(overlay_vbo);
    renderer_destroy_vao(hud_vao);      renderer_destroy_buffer(hud_vbo);
    inv_renderer_shutdown(&inv_r);
    renderer_destroy_texture(tex_array);
    ui_gl_shutdown(&gui);
    world_destroy(g_ecs);
    renderer_shutdown();

#ifdef ENABLE_LOGGER
    log_shutdown();
#endif

    window_destroy(win);
    return 0;
}
