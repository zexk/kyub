#include "ui.h"
#include "input.h"
#include "gl_ext.h"
#include "shader.h"
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

    ui->shader = shader_create_program("shaders/ui.vert", "shaders/ui.frag");
    if (!ui->shader) {
        fprintf(stderr, "Failed to create UI shader\n");
        return;
    }

    if (glCreateVertexArrays) {
        glCreateVertexArrays(1, &ui->vao);
    } else {
        glGenVertexArrays(1, &ui->vao);
    }
    if (glCreateBuffers) {
        glCreateBuffers(1, &ui->vbo);
        glCreateBuffers(1, &ui->ebo);
    } else {
        glGenBuffers(1, &ui->vbo);
        glGenBuffers(1, &ui->ebo);
    }

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
    nk_font_atlas_end(&ui->atlas, (nk_handle){0}, NULL);

    glGenTextures(1, &ui->font_tex);
    glBindTexture(GL_TEXTURE_2D, ui->font_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (font) {
        nk_style_set_font(&ui->ctx, &font->handle);
    }

    ui->initialized = true;
}

void ui_shutdown(UI *ui) {
    if (!ui->initialized) return;
    if (ui->font_tex) glDeleteTextures(1, &ui->font_tex);
    nk_buffer_free(&ui->cmds);
    nk_buffer_free(&ui->vertices);
    nk_buffer_free(&ui->elements);
    if (ui->shader) glDeleteProgram(ui->shader);
    if (ui->vao) glDeleteVertexArrays(1, &ui->vao);
    if (ui->vbo) glDeleteBuffers(1, &ui->vbo);
    if (ui->ebo) glDeleteBuffers(1, &ui->ebo);
    ui->initialized = false;
}

void ui_toggle(UI *ui) {
    ui->visible = !ui->visible;
}

bool ui_is_visible(UI *ui) {
    return ui->visible;
}

static int nk_key_from_key(int key) {
    switch (key) {
        case 0x26: return NK_KEY_UP;
        case 0x28: return NK_KEY_DOWN;
        case 0x25: return NK_KEY_LEFT;
        case 0x27: return NK_KEY_RIGHT;
        case 0x0D: return NK_KEY_ENTER;
        case 0x09: return NK_KEY_TAB;
        case 0x08: return NK_KEY_BACKSPACE;
        default: return key;
    }
}

void ui_handle_mouse(UI *ui, int x, int y) {
    if (!ui->visible || !ui->initialized) return;
    nk_input_motion(&ui->ctx, x, y);
}

void ui_handle_key(UI *ui, int key, bool pressed) {
    if (!ui->visible || !ui->initialized) return;
    int nk_key = nk_key_from_key(key);
    nk_input_key(&ui->ctx, nk_key, pressed);
}

void ui_poll_mouse(UI *ui) {
    if (!ui->visible || !ui->initialized) return;
    nk_input_button(&ui->ctx, NK_BUTTON_LEFT, ui->ctx.input.mouse.pos.x, ui->ctx.input.mouse.pos.y, g_input.mouse_left);
    nk_input_button(&ui->ctx, NK_BUTTON_RIGHT, ui->ctx.input.mouse.pos.x, ui->ctx.input.mouse.pos.y, g_input.mouse_right);
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

    glPushAttrib(GL_ENABLE_BIT | GL_BLEND);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);

    glUseProgram(ui->shader);
    glUniform2f(glGetUniformLocation(ui->shader, "uScreenSize"), (float)width, (float)height);

    nk_buffer_clear(&ui->cmds);
    nk_buffer_clear(&ui->vertices);
    nk_buffer_clear(&ui->elements);

    if (nk_begin(&ui->ctx, "Debug", nk_rect(10, 10, 250, 200), NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(&ui->ctx, 20, 1);

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
    config.tex_null.texture = (nk_handle){0};
    config.tex_null.uv = (struct nk_vec2){0.0f, 0.0f};
    config.vertex_layout = vertex_layout;
    config.vertex_size = sizeof(struct ui_vertex);
    config.vertex_alignment = NK_ALIGNOF(struct ui_vertex);

    nk_convert(&ui->ctx, &ui->cmds, &ui->vertices, &ui->elements, &config);

    if (glVertexArrayVertexBuffer && glVertexArrayElementBuffer && glNamedBufferData) {
        glNamedBufferData(ui->vbo, ui->vertices.needed, ui->vertices.memory.ptr, GL_DYNAMIC_DRAW);
        glVertexArrayVertexBuffer(ui->vao, 0, ui->vbo, 0, 20);
        glVertexArrayAttribFormat(ui->vao, 0, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(ui->vao, 0, 0);
        glEnableVertexArrayAttrib(ui->vao, 0);

        glVertexArrayAttribFormat(ui->vao, 1, 2, GL_FLOAT, GL_FALSE, 8);
        glVertexArrayAttribBinding(ui->vao, 1, 0);
        glEnableVertexArrayAttrib(ui->vao, 1);

        glVertexArrayAttribFormat(ui->vao, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16);
        glVertexArrayAttribBinding(ui->vao, 2, 0);
        glEnableVertexArrayAttrib(ui->vao, 2);

        glNamedBufferData(ui->ebo, ui->elements.needed, ui->elements.memory.ptr, GL_DYNAMIC_DRAW);
        glVertexArrayElementBuffer(ui->vao, ui->ebo);
        glBindVertexArray(ui->vao);
    } else {
        glBindVertexArray(ui->vao);
        glBindBuffer(GL_ARRAY_BUFFER, ui->vbo);
        glBufferData(GL_ARRAY_BUFFER, ui->vertices.needed, ui->vertices.memory.ptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 20, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 20, (void*)8);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, 20, (void*)16);
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ui->ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, ui->elements.needed, ui->elements.memory.ptr, GL_DYNAMIC_DRAW);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ui->font_tex);

    glDrawElements(GL_TRIANGLES, ui->elements.needed / sizeof(nk_ushort), GL_UNSIGNED_SHORT, 0);

    nk_clear(&ui->ctx);
    nk_buffer_clear(&ui->cmds);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    glPopAttrib();
}