#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stddef.h>

#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_FONT_BAKING
#include "nuklear.h"

#include "math3d.h"
#include "voxel.h"

#define UI_MAX_VERTEX_BUFFER 65536
#define UI_MAX_ELEMENT_BUFFER 65536

typedef struct {
    BlockType selected_block;
    struct nk_context ctx;
    struct nk_font_atlas atlas;
    struct nk_buffer cmds;
    struct nk_buffer vertices;
    struct nk_buffer elements;
    unsigned int shader;
    unsigned int vao;
    unsigned int vbo;
    unsigned int ebo;
    unsigned int font_tex;
    bool initialized;
    bool visible;
    float fps;
    int chunk_count;
    vec3 player_pos;
    vec3 player_dir;
    float player_yaw;
    float player_pitch;
    int render_distance;
} UI;

void ui_init(UI *ui, int width, int height);
void ui_shutdown(UI *ui);
void ui_toggle(UI *ui);
bool ui_is_visible(UI *ui);
void ui_handle_mouse(UI *ui, int x, int y, int button, bool pressed);
void ui_handle_key(UI *ui, int key, bool pressed);
void ui_set_stats(UI *ui, float fps, int chunk_count, vec3 pos, vec3 dir, float yaw, float pitch);
void ui_render(UI *ui, int width, int height);

#endif // UI_H