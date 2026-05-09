#include "ui.h"
#include "input.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

struct ui_vertex {
    float pos[2];
    float uv[2];
    unsigned char col[4];
};

static const struct nk_draw_vertex_layout_element vertex_layout[] = {
    {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct ui_vertex, pos)},
    {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct ui_vertex, uv)},
    {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(struct ui_vertex, col)},
    {NK_VERTEX_LAYOUT_END}
};

static void *nk_alloc_fn(nk_handle arg, void *old, nk_size size) {
    (void)arg;
    (void)old;
    if (old == NULL) {
        return malloc(size);
    }
    return realloc(old, size);
}

static void nk_free_fn(nk_handle arg, void *old) {
    (void)arg;
    free(old);
}

void ui_init(UI *ui, int width, int height) {
    (void)width;
    (void)height;
    ui->initialized = false;
    ui->selected_block = BLOCK_STONE;
    ui->visible = true;

    ui->shader = renderer_create_program("shaders/ui.vert", "shaders/ui.frag");
    if (ui->shader == R_INVALID_HANDLE) {
        LOG_ERROR(CAT_UI, "Failed to create UI shader");
        return;
    }

    LOG_INFO(CAT_UI, "UI initialized successfully");

    ui->vao = renderer_create_vao();
    ui->vbo = renderer_create_buffer();
    ui->ebo = renderer_create_buffer();

    struct nk_allocator alloc;
    alloc.userdata.ptr = NULL;
    alloc.alloc = nk_alloc_fn;
    alloc.free = nk_free_fn;
    nk_buffer_init(&ui->cmds, &alloc, UI_MAX_VERTEX_BUFFER);
    nk_buffer_init(&ui->vertices, &alloc, UI_MAX_VERTEX_BUFFER);
    nk_buffer_init(&ui->elements, &alloc, UI_MAX_ELEMENT_BUFFER);

    nk_init_default(&ui->ctx, NULL);

    nk_font_atlas_init_default(&ui->atlas);
    nk_font_atlas_begin(&ui->atlas);
    struct nk_font *font = nk_font_atlas_add_default(&ui->atlas, 18.0f, NULL);
    int w, h;
    const void *image = nk_font_atlas_bake(&ui->atlas, &w, &h, NK_FONT_ATLAS_RGBA32);

    /* Create font texture before ending atlas so we can pass the handle */
    ui->font_tex = renderer_create_texture();
    renderer_bind_texture(R_TEX_2D, ui->font_tex);
    renderer_tex_image_2d(w, h, image);
    renderer_tex_param(R_TEX_2D, R_TEX_MIN_FILTER, R_TEX_LINEAR);
    renderer_tex_param(R_TEX_2D, R_TEX_MAG_FILTER, R_TEX_LINEAR);
    renderer_bind_texture(R_TEX_2D, R_INVALID_HANDLE);

    nk_font_atlas_end(&ui->atlas, nk_handle_id((int)ui->font_tex), NULL);

    /* Create 1x1 white texture for null/fill draws */
    unsigned char white_pixel[4] = {255, 255, 255, 255};
    ui->null_tex = renderer_create_texture();
    renderer_bind_texture(R_TEX_2D, ui->null_tex);
    renderer_tex_image_2d(1, 1, white_pixel);
    renderer_bind_texture(R_TEX_2D, R_INVALID_HANDLE);

    if (font) {
        nk_style_set_font(&ui->ctx, &font->handle);
    }

    ui->initialized = true;
}

void ui_shutdown(UI *ui) {
    if (!ui->initialized) return;
    if (ui->font_tex != R_INVALID_HANDLE) renderer_destroy_texture(ui->font_tex);
    if (ui->null_tex != R_INVALID_HANDLE) renderer_destroy_texture(ui->null_tex);
    nk_buffer_free(&ui->cmds);
    nk_buffer_free(&ui->vertices);
    nk_buffer_free(&ui->elements);
    if (ui->shader != R_INVALID_HANDLE) renderer_destroy_program(ui->shader);
    if (ui->vao != R_INVALID_HANDLE) renderer_destroy_vao(ui->vao);
    if (ui->vbo != R_INVALID_HANDLE) renderer_destroy_buffer(ui->vbo);
    if (ui->ebo != R_INVALID_HANDLE) renderer_destroy_buffer(ui->ebo);
    ui->initialized = false;
}

void ui_toggle(UI *ui) {
    ui->visible = !ui->visible;
}

bool ui_is_visible(UI *ui) {
    return ui->visible;
}

void ui_set_stats(UI *ui, float fps, int chunk_count, vec3 pos, vec3 dir, float yaw, float pitch) {
    ui->fps = fps;
    ui->chunk_count = chunk_count;
    ui->player_pos = pos;
    ui->player_dir = dir;
    ui->player_yaw = yaw;
    ui->player_pitch = pitch;
}

void ui_render(UI *ui, int width, int height) {
    if (!ui->visible || !ui->initialized) return;

    renderer_push_attrib();
    renderer_enable(R_CAP_BLEND);
    renderer_blend_func(R_BLEND_SRC_ALPHA, R_BLEND_ONE_MINUS_SRC_ALPHA);
    renderer_disable(R_CAP_CULL_FACE);
    renderer_disable(R_CAP_DEPTH_TEST);
    renderer_disable(R_CAP_SCISSOR_TEST);

    renderer_use_program(ui->shader);
    int loc = renderer_uniform_location(ui->shader, "uScreenSize");
    renderer_uniform_vec2(loc, (float)width, (float)height);

    nk_buffer_clear(&ui->cmds);
    nk_buffer_clear(&ui->vertices);
    nk_buffer_clear(&ui->elements);

    ui->ctx.style.window.background = nk_rgba(40, 40, 40, 255);
    ui->ctx.style.window.group_padding = nk_vec2(0, 0);
    ui->ctx.style.window.spacing = nk_vec2(0, 0);

    if (nk_begin(&ui->ctx, "Debug", nk_rect(10, 10, 350, 400), NK_WINDOW_BORDER | NK_WINDOW_BACKGROUND)) {
        nk_layout_row_dynamic(&ui->ctx, 20, 1);

        nk_bool fps_unlimited = ui->fps_unlimited ? 1 : 0;
        nk_checkbox_label(&ui->ctx, "Unlimited FPS", &fps_unlimited);
        ui->fps_unlimited = fps_unlimited ? true : false;

        char buf[64];
        snprintf(buf, sizeof(buf), "FPS: %.1f", ui->fps);
        nk_label(&ui->ctx, buf, NK_TEXT_LEFT);

        snprintf(buf, sizeof(buf), "Chunks: %d", ui->chunk_count);
        nk_label(&ui->ctx, buf, NK_TEXT_LEFT);

        snprintf(buf, sizeof(buf), "Pos: %.1f %.1f %.1f",
                 ui->player_pos.x, ui->player_pos.y, ui->player_pos.z);
        nk_label(&ui->ctx, buf, NK_TEXT_LEFT);

        snprintf(buf, sizeof(buf), "Yaw: %.1f", ui->player_yaw);
        nk_label(&ui->ctx, buf, NK_TEXT_LEFT);

        snprintf(buf, sizeof(buf), "Pitch: %.1f", ui->player_pitch);
        nk_label(&ui->ctx, buf, NK_TEXT_LEFT);

        nk_layout_row_dynamic(&ui->ctx, 20, 2);
        nk_label(&ui->ctx, "Render Dist:", NK_TEXT_LEFT);
        nk_slider_int(&ui->ctx, 1, &ui->render_distance, 8, 1);

        /* Block palette */
        nk_layout_row_dynamic(&ui->ctx, 30, 4);
        if (nk_button_label(&ui->ctx, "Air"))   ui->selected_block = BLOCK_AIR;
        if (nk_button_label(&ui->ctx, "Dirt"))  ui->selected_block = BLOCK_DIRT;
        if (nk_button_label(&ui->ctx, "Grass")) ui->selected_block = BLOCK_GRASS;
        if (nk_button_label(&ui->ctx, "Stone")) ui->selected_block = BLOCK_STONE;
    }
    nk_end(&ui->ctx);

    struct nk_convert_config config;
    config.global_alpha = 1.0f;
    config.line_AA = NK_ANTI_ALIASING_ON;
    config.shape_AA = NK_ANTI_ALIASING_ON;
    config.circle_segment_count = 22;
    config.arc_segment_count = 22;
    config.curve_segment_count = 22;
    config.tex_null.texture = nk_handle_id((int)ui->null_tex);
    config.tex_null.uv = (struct nk_vec2){0.0f, 0.0f};
    config.vertex_layout = vertex_layout;
    config.vertex_size = sizeof(struct ui_vertex);
    config.vertex_alignment = NK_ALIGNOF(struct ui_vertex);

    nk_convert(&ui->ctx, &ui->cmds, &ui->vertices, &ui->elements, &config);

    renderer_bind_vao(ui->vao);
    renderer_bind_buffer(R_BUF_ARRAY, ui->vbo);
    renderer_buffer_data(R_BUF_ARRAY, ui->vertices.needed, ui->vertices.memory.ptr, R_USAGE_DYNAMIC);
    renderer_attrib_pointer(0, 2, R_TYPE_FLOAT, false, 20, 0);
    renderer_enable_attrib(0);
    renderer_attrib_pointer(1, 2, R_TYPE_FLOAT, false, 20, 8);
    renderer_enable_attrib(1);
    renderer_attrib_pointer(2, 4, R_TYPE_UBYTE, true, 20, 16);
    renderer_enable_attrib(2);

    renderer_bind_buffer(R_BUF_ELEMENT, ui->ebo);
    renderer_buffer_data(R_BUF_ELEMENT, ui->elements.needed, ui->elements.memory.ptr, R_USAGE_DYNAMIC);

    renderer_active_texture(0);

    /* Iterate draw commands to bind correct texture per command */
    const nk_ushort *base_offset = (const nk_ushort *)nk_buffer_memory_const(&ui->elements);
    const nk_ushort *offset = base_offset;
    const struct nk_draw_command *cmd;
    nk_draw_foreach(cmd, &ui->ctx, &ui->cmds) {
        if (!cmd->elem_count) continue;
        R_Texture tex = ui->null_tex;
        if (cmd->texture.id == (int)ui->font_tex)
            tex = ui->font_tex;
        renderer_bind_texture(R_TEX_2D, tex);
        renderer_draw_elements(R_PRIM_TRIANGLES, cmd->elem_count,
                               (int)(offset - base_offset));
        offset += cmd->elem_count;
    }

    nk_clear(&ui->ctx);
    nk_buffer_clear(&ui->cmds);

    renderer_bind_buffer(R_BUF_ARRAY, R_INVALID_HANDLE);
    renderer_bind_buffer(R_BUF_ELEMENT, R_INVALID_HANDLE);
    renderer_bind_texture(R_TEX_2D, R_INVALID_HANDLE);
    renderer_use_program(R_INVALID_HANDLE);

    renderer_pop_attrib();
}
