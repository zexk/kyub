#include "inventory.h"
#include <string.h>
#include <stdio.h>

/* ── Item registry ───────────────────────────────────────────────────────── */

static kyub_item_def_t s_items[ITEM_MAX];
static uint16_t        s_item_count = 1;  /* 0 = ITEM_NONE (implicit) */
static uint16_t        s_block_to_item[KV_MAX_BLOCK_TYPES]; /* block_id → item_id */

uint16_t kyub_item_register(const kyub_item_def_t *def) {
    if (!def || s_item_count >= ITEM_MAX) return ITEM_NONE;
    uint16_t id = s_item_count++;
    s_items[id] = *def;
    if (def->block_type != KV_BLOCK_AIR && def->block_type < KV_MAX_BLOCK_TYPES)
        s_block_to_item[def->block_type] = id;
    return id;
}

const kyub_item_def_t *kyub_item_get(uint16_t id) {
    if (id == ITEM_NONE || id >= s_item_count) return NULL;
    return &s_items[id];
}

uint16_t kyub_item_count(void)                        { return s_item_count; }
uint16_t kyub_item_for_block(uint16_t block_id)       { return (block_id < KV_MAX_BLOCK_TYPES) ? s_block_to_item[block_id] : ITEM_NONE; }

/* ── Inventory operations ────────────────────────────────────────────────── */

void inv_init(kyub_inventory_t *inv) {
    memset(inv, 0, sizeof(*inv));
    for (int i = 0; i < HOTBAR_SIZE; i++) inv->hotbar[i] = -1;
}

bool inv_add(kyub_inventory_t *inv, kyub_item_t item) {
    if (item.type == ITEM_NONE || item.count == 0) return true;
    const kyub_item_def_t *def = kyub_item_get(item.type);
    uint16_t max_stack = def ? def->max_stack : 256;

    /* Try stacking onto existing slots first */
    if (max_stack > 1) {
        for (int i = 0; i < INV_SLOTS && item.count > 0; i++) {
            kyub_item_t *sl = &inv->slots[i];
            if (sl->type != item.type || sl->meta != item.meta) continue;
            uint16_t space = (uint16_t)(max_stack - sl->count);
            if (space == 0) continue;
            uint16_t take = item.count < space ? item.count : space;
            sl->count   += take;
            item.count  -= take;
        }
    }

    /* Fill empty slots with remainder */
    for (int i = 0; i < INV_SLOTS && item.count > 0; i++) {
        if (inv->slots[i].type != ITEM_NONE) continue;
        uint16_t take = item.count < max_stack ? item.count : max_stack;
        inv->slots[i] = (kyub_item_t){item.type, take, item.meta, 0};
        item.count   -= take;
    }
    return item.count == 0;
}

void inv_remove_at(kyub_inventory_t *inv, int slot, uint16_t count) {
    if (slot < 0 || slot >= INV_SLOTS) return;
    kyub_item_t *sl = &inv->slots[slot];
    if (sl->count <= count) {
        *sl = (kyub_item_t){0};
        /* Unpin any hotbar slot pointing here */
        for (int i = 0; i < HOTBAR_SIZE; i++)
            if (inv->hotbar[i] == slot) inv->hotbar[i] = -1;
    } else {
        sl->count -= count;
    }
}

int inv_active_slot(const kyub_inventory_t *inv) {
    return inv->hotbar[inv->active_hotbar];
}

kyub_item_t inv_selected(const kyub_inventory_t *inv) {
    int s = inv_active_slot(inv);
    return (s >= 0) ? inv->slots[s] : (kyub_item_t){0};
}

void inv_hotbar_pin(kyub_inventory_t *inv, int hotbar_pos, int slot_idx) {
    if (hotbar_pos < 0 || hotbar_pos >= HOTBAR_SIZE) return;
    if (slot_idx < 0 || slot_idx >= INV_SLOTS) { inv->hotbar[hotbar_pos] = -1; return; }
    inv->hotbar[hotbar_pos] = slot_idx;
}

void inv_hotbar_cycle(kyub_inventory_t *inv, int delta) {
    /* Cycle only through pinned, non-empty hotbar positions */
    int first = -1, best = -1;
    int cur = inv->active_hotbar;

    /* Collect valid positions in order */
    int valid[HOTBAR_SIZE]; int nv = 0;
    for (int i = 0; i < HOTBAR_SIZE; i++) {
        int s = inv->hotbar[i];
        if (s < 0 || inv->slots[s].type == ITEM_NONE) continue;
        valid[nv++] = i;
    }
    if (nv == 0) return;

    /* Find current position in valid list, move by delta */
    int ci = 0;
    for (int i = 0; i < nv; i++) if (valid[i] == cur) { ci = i; break; }
    ci = ((ci + delta) % nv + nv) % nv;
    inv->active_hotbar = valid[ci];

    (void)first; (void)best;
}

/* ── Slot geometry ───────────────────────────────────────────────────────── */

#define SLT_SZ    0.07f
#define SLT_GAP   0.015f
#define SLT_STEP  (SLT_SZ + SLT_GAP)

/* Grid is centred on screen */
#define GRID_W    (INV_COLS * SLT_SZ + (INV_COLS - 1) * SLT_GAP)
#define GRID_H    (INV_ROWS * SLT_SZ + (INV_ROWS - 1) * SLT_GAP)
#define GRID_X0   (-(GRID_W * 0.5f))
#define GRID_Y1   ( (GRID_H * 0.5f))   /* NDC top of first row */

/* Hotbar is centred below the grid with a small gap */
#define HBAR_GAP_FROM_GRID  0.04f
#define HBAR_Y1   (GRID_Y1 - GRID_H - HBAR_GAP_FROM_GRID)
#define HBAR_Y0   (HBAR_Y1 - SLT_SZ)
#define HBAR_X0   GRID_X0

/* Persistent hotbar (always visible) sits at the bottom of the screen */
#define PERSIST_Y0  (-0.93f)
#define PERSIST_Y1  (PERSIST_Y0 + SLT_SZ)

typedef struct { float x0, y0, x1, y1; } SlotRect;

static SlotRect grid_rect(int slot) {
    int col = slot % INV_COLS, row = slot / INV_COLS;
    float x0 = GRID_X0 + col * SLT_STEP;
    float y1 = GRID_Y1  - row * SLT_STEP;
    return (SlotRect){x0, y1 - SLT_SZ, x0 + SLT_SZ, y1};
}

static SlotRect hbar_rect(int pos) {
    float x0 = HBAR_X0 + pos * SLT_STEP;
    return (SlotRect){x0, HBAR_Y0, x0 + SLT_SZ, HBAR_Y1};
}

static SlotRect persist_rect(int pos) {
    float x0 = HBAR_X0 + pos * SLT_STEP;
    return (SlotRect){x0, PERSIST_Y0, x0 + SLT_SZ, PERSIST_Y1};
}

int inv_hovered_slot(float mx, float my, float win_w, float win_h) {
    float nx = (mx / win_w) * 2.0f - 1.0f;
    float ny = 1.0f - (my / win_h) * 2.0f;
    for (int i = 0; i < INV_SLOTS; i++) {
        SlotRect r = grid_rect(i);
        if (nx >= r.x0 && nx <= r.x1 && ny >= r.y0 && ny <= r.y1) return i;
    }
    return -1;
}

/* ── Vertex helpers ──────────────────────────────────────────────────────── */

/* Appends a 2-float-per-vertex quad (for hud_program backgrounds) */
static void push_bg(float *buf, int *n, SlotRect r) {
    float v[12] = {
        r.x0,r.y0,  r.x1,r.y0,  r.x1,r.y1,
        r.x0,r.y0,  r.x1,r.y1,  r.x0,r.y1,
    };
    memcpy(buf + *n, v, sizeof(v));
    *n += 12;
}

/* Appends a 5-float-per-vertex textured icon quad (for inv_program) */
static void push_icon(float *buf, int *n, SlotRect r, float layer) {
    /* UV convention matches the voxel mesh: low-y vertex → v near 0,
       high-y vertex → v near 1.  Padding avoids edge-texel sampling. */
    float pad = 0.001f;
    float u0 = pad, v0 = pad, u1 = 1.0f - pad, v1 = 1.0f - pad;
    float v[30] = {
        r.x0,r.y0, u0,v0,layer,   /* BL screen → UV bottom-left  */
        r.x1,r.y0, u1,v0,layer,   /* BR screen → UV bottom-right */
        r.x1,r.y1, u1,v1,layer,   /* TR screen → UV top-right    */
        r.x0,r.y0, u0,v0,layer,
        r.x1,r.y1, u1,v1,layer,
        r.x0,r.y1, u0,v1,layer,   /* TL screen → UV top-left     */
    };
    memcpy(buf + *n, v, sizeof(v));
    *n += 30;
}

/* ── Renderer lifecycle ──────────────────────────────────────────────────── */

bool inv_renderer_init(inv_renderer_t *r) {
    memset(r, 0, sizeof(*r));
    r->shader = renderer_create_program("shaders/inv.vert", "shaders/inv.frag");
    if (r->shader == R_INVALID_HANDLE) return false;

    r->loc_texture = renderer_uniform_location(r->shader, "uTexture");
    r->loc_color   = renderer_uniform_location(r->shader, "uColor");
    r->loc_alpha   = renderer_uniform_location(r->shader, "uAlpha");

    /* Icon VAO: vec2 pos + vec2 uv + float layer (stride 20) */
    r->vao_icon = renderer_create_vao();
    r->vbo_icon = renderer_create_buffer();
    renderer_bind_vao(r->vao_icon);
    renderer_bind_buffer(R_BUF_ARRAY, r->vbo_icon);
    renderer_buffer_data(R_BUF_ARRAY, 8192, NULL, R_USAGE_DYNAMIC);
    renderer_attrib_pointer(0, 2, R_TYPE_FLOAT, false, 20, 0);
    renderer_enable_attrib(0);
    renderer_attrib_pointer(1, 2, R_TYPE_FLOAT, false, 20, 8);
    renderer_enable_attrib(1);
    renderer_attrib_pointer(2, 1, R_TYPE_FLOAT, false, 20, 16);
    renderer_enable_attrib(2);
    renderer_bind_vao(R_INVALID_HANDLE);

    /* Background VAO: vec2 pos (stride 8), used with hud_program */
    r->vao_bg = renderer_create_vao();
    r->vbo_bg = renderer_create_buffer();
    renderer_bind_vao(r->vao_bg);
    renderer_bind_buffer(R_BUF_ARRAY, r->vbo_bg);
    renderer_buffer_data(R_BUF_ARRAY, 4096, NULL, R_USAGE_DYNAMIC);
    renderer_attrib_pointer(0, 2, R_TYPE_FLOAT, false, 8, 0);
    renderer_enable_attrib(0);
    renderer_bind_vao(R_INVALID_HANDLE);

    return true;
}

void inv_renderer_shutdown(inv_renderer_t *r) {
    if (r->vao_icon != R_INVALID_HANDLE) renderer_destroy_vao(r->vao_icon);
    if (r->vbo_icon != R_INVALID_HANDLE) renderer_destroy_buffer(r->vbo_icon);
    if (r->vao_bg   != R_INVALID_HANDLE) renderer_destroy_vao(r->vao_bg);
    if (r->vbo_bg   != R_INVALID_HANDLE) renderer_destroy_buffer(r->vbo_bg);
}

/* ── Internal draw helpers ───────────────────────────────────────────────── */

/* NDC slot bottom-left → pixel top-left (for hud_text, which uses pixel coords) */
static void slot_text_pos(SlotRect r, float win_w, float win_h, float *px, float *py) {
    *px = (r.x0 * 0.5f + 0.5f) * win_w + 2.0f;
    *py = (0.5f - r.y0 * 0.5f) * win_h - 10.0f;
}

static void draw_slot_backgrounds(inv_renderer_t *r, R_Program hud_prog,
                                   const SlotRect *rects, int count,
                                   int active_pos, int hovered_slot_local,
                                   const int *pinned_positions, int n_pinned) {
    /* Build ALL geometry first, upload once, then N draw calls varying only
       the push-constant color — avoids repeated staging-buffer clobbers. */
    float buf[45 * 12];
    int   n = 0;
    for (int i = 0; i < count; i++) push_bg(buf, &n, rects[i]);

    renderer_bind_vao(r->vao_bg);
    renderer_bind_buffer(R_BUF_ARRAY, r->vbo_bg);
    renderer_buffer_sub_data(R_BUF_ARRAY, 0, (size_t)(n * 4), buf);

    renderer_use_program(hud_prog);
    int color_loc = renderer_uniform_location(hud_prog, "uColor");
    int alpha_loc = renderer_uniform_location(hud_prog, "uAlpha");
    renderer_uniform_float(alpha_loc, 1.0f);

    for (int i = 0; i < count; i++) {
        bool is_active  = (i == active_pos);
        bool is_hovered = (i == hovered_slot_local);
        bool is_pinned  = false;
        for (int p = 0; p < n_pinned; p++) if (pinned_positions[p] == i) { is_pinned = true; break; }

        if      (is_active)  renderer_uniform_vec3(color_loc, 1.00f, 1.00f, 1.00f);
        else if (is_hovered) renderer_uniform_vec3(color_loc, 0.55f, 0.55f, 0.55f);
        else if (is_pinned)  renderer_uniform_vec3(color_loc, 0.40f, 0.40f, 0.40f);
        else                 renderer_uniform_vec3(color_loc, 0.20f, 0.20f, 0.20f);

        renderer_draw_arrays(R_PRIM_TRIANGLES, i * 6, 6);
    }
    renderer_bind_vao(R_INVALID_HANDLE);
    renderer_bind_buffer(R_BUF_ARRAY, R_INVALID_HANDLE); /* clear bound_vbo so VAO buffers take over */
}

static void draw_slot_icons(inv_renderer_t *r, R_Texture tex_array,
                             R_Program hud_prog,
                             const SlotRect *rects, int count,
                             const kyub_item_t *items) {
    /* Build geometry before touching any renderer state so the early-return
       never leaves the inv pipeline active.  hud_text does not call
       renderer_use_program itself, so leaving the inv pipeline set causes it
       to render HUD-format quads through the icon pipeline → texture garbage. */
    float buf[45 * 30];
    int   n = 0;

    for (int i = 0; i < count; i++) {
        const kyub_item_t *it = &items[i];
        if (it->type == ITEM_NONE) continue;
        const kyub_item_def_t *def = kyub_item_get(it->type);
        if (!def || def->tex_layer < 0) continue;
        push_icon(buf, &n, rects[i], (float)def->tex_layer);
    }

    if (n > 0) {
        renderer_use_program(r->shader);
        renderer_active_texture(0);
        renderer_bind_texture(R_TEX_2D, tex_array);
        renderer_uniform_int(r->loc_texture, 0);
        renderer_uniform_vec3(r->loc_color, 1.0f, 1.0f, 1.0f);
        renderer_uniform_float(r->loc_alpha, 1.0f);
        renderer_bind_vao(r->vao_icon);
        renderer_bind_buffer(R_BUF_ARRAY, r->vbo_icon);
        renderer_buffer_sub_data(R_BUF_ARRAY, 0, (size_t)(n * 4), buf);
        renderer_draw_arrays(R_PRIM_TRIANGLES, 0, n / 5);
        renderer_bind_vao(R_INVALID_HANDLE);
        renderer_bind_buffer(R_BUF_ARRAY, R_INVALID_HANDLE);
    }

    /* Always restore hud pipeline so subsequent hud_text calls use the
       correct vertex format (HUD: stride 8, not icon: stride 20). */
    renderer_use_program(hud_prog);
}

static void draw_slot_counts(const SlotRect *rects, int count,
                              const kyub_item_t *items,
                              hud_t *gui, float win_w, float win_h) {
    char text[8];
    for (int i = 0; i < count; i++) {
        const kyub_item_t *it = &items[i];
        if (it->type == ITEM_NONE || it->count <= 1) continue;
        float px, py;
        slot_text_pos(rects[i], win_w, win_h, &px, &py);
        snprintf(text, sizeof(text), "%d", (int)it->count);
        float tw = hud_text_width(text, 1.0f);
        /* right-align within the slot */
        float slot_px1 = (rects[i].x1 * 0.5f + 0.5f) * win_w - 2.0f;
        hud_text(gui, slot_px1 - tw, py, text, 1.0f, 0.9f, 0.9f, 0.9f);
    }
}

/* ── Public draw functions ───────────────────────────────────────────────── */

void inv_draw_hotbar(inv_renderer_t *r, const kyub_inventory_t *inv,
                     R_Texture tex_array, R_Program hud_prog,
                     hud_t *gui, float win_w, float win_h) {
    /* Build rect array and item snapshot for the 9 hotbar positions */
    SlotRect    rects[HOTBAR_SIZE];
    kyub_item_t items[HOTBAR_SIZE];

    for (int i = 0; i < HOTBAR_SIZE; i++) {
        rects[i] = persist_rect(i);
        int s     = inv->hotbar[i];
        items[i]  = (s >= 0) ? inv->slots[s] : (kyub_item_t){0};
    }

    /* Active hotbar pos maps to active_hotbar index */
    draw_slot_backgrounds(r, hud_prog, rects, HOTBAR_SIZE,
                          inv->active_hotbar, -1, NULL, 0);
    draw_slot_icons(r, tex_array, hud_prog, rects, HOTBAR_SIZE, items);
    draw_slot_counts(rects, HOTBAR_SIZE, items, gui, win_w, win_h);
}

void inv_draw_screen(inv_renderer_t *r, const kyub_inventory_t *inv,
                     R_Texture tex_array, R_Program hud_prog,
                     hud_t *gui, float win_w, float win_h, int hovered_slot) {
    /* ── Grid ── */
    SlotRect grid_rects[INV_SLOTS];
    for (int i = 0; i < INV_SLOTS; i++) grid_rects[i] = grid_rect(i);

    /* Collect pinned slot indices for highlight */
    int pinned[HOTBAR_SIZE]; int np = 0;
    for (int i = 0; i < HOTBAR_SIZE; i++)
        if (inv->hotbar[i] >= 0) pinned[np++] = inv->hotbar[i];

    draw_slot_backgrounds(r, hud_prog, grid_rects, INV_SLOTS,
                          -1, hovered_slot, pinned, np);
    draw_slot_icons(r, tex_array, hud_prog, grid_rects, INV_SLOTS, inv->slots);
    draw_slot_counts(grid_rects, INV_SLOTS, inv->slots, gui, win_w, win_h);

    /* ── Hotbar strip inside inventory screen ── */
    SlotRect    hb_rects[HOTBAR_SIZE];
    kyub_item_t hb_items[HOTBAR_SIZE];
    for (int i = 0; i < HOTBAR_SIZE; i++) {
        hb_rects[i] = hbar_rect(i);
        int s        = inv->hotbar[i];
        hb_items[i]  = (s >= 0) ? inv->slots[s] : (kyub_item_t){0};
    }
    draw_slot_backgrounds(r, hud_prog, hb_rects, HOTBAR_SIZE,
                          inv->active_hotbar, -1, NULL, 0);
    draw_slot_icons(r, tex_array, hud_prog, hb_rects, HOTBAR_SIZE, hb_items);

    /* ── Pin hint ── */
    if (hovered_slot >= 0) {
        float px, py;
        slot_text_pos(grid_rects[hovered_slot], win_w, win_h, &px, &py);
        hud_text(gui, px, py - 12.0f, "1-9 pin", 1.0f, 0.7f, 0.7f, 0.7f);
    }
}
