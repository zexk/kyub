#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "kv.h"
#include "renderer.h"
#include "ui_gl.h"

/* ── Item type registry ──────────────────────────────────────────────────── */

#define ITEM_NONE     ((uint16_t)0)
#define ITEM_MAX      4096

typedef struct {
    const char *id;
    const char *name;
    uint16_t    max_stack;   /* 1 = non-stackable (tools); 256 = normal */
    uint16_t    block_type;  /* KV_BLOCK_AIR = not directly placeable   */
    int         tex_layer;   /* index into the block texture array; -1  */
} kyub_item_def_t;

uint16_t               kyub_item_register(const kyub_item_def_t *def);
const kyub_item_def_t *kyub_item_get(uint16_t id);
uint16_t               kyub_item_count(void);

/* Reverse lookup: which item does breaking this block drop? ITEM_NONE if none. */
uint16_t kyub_item_for_block(uint16_t block_id);

/* ── Item slot ───────────────────────────────────────────────────────────── */

typedef struct {
    uint16_t type;   /* ITEM_NONE = empty                              */
    uint16_t count;  /* 1–256                                          */
    uint16_t meta;   /* type-specific: durability, quality, variant…   */
    uint16_t _pad;
} kyub_item_t;

/* ── Inventory ───────────────────────────────────────────────────────────── */

#define INV_COLS     9
#define INV_ROWS     4
#define INV_SLOTS   (INV_COLS * INV_ROWS)   /* 36 */
#define HOTBAR_SIZE  9

typedef struct {
    kyub_item_t slots[INV_SLOTS];
    int         hotbar[HOTBAR_SIZE];   /* slot indices into slots[]; -1 = unpinned */
    int         active_hotbar;         /* 0–8 */
} kyub_inventory_t;

void        inv_init(kyub_inventory_t *inv);
bool        inv_add(kyub_inventory_t *inv, kyub_item_t item);
void        inv_remove_at(kyub_inventory_t *inv, int slot, uint16_t count);
kyub_item_t inv_selected(const kyub_inventory_t *inv);
int         inv_active_slot(const kyub_inventory_t *inv);
void        inv_hotbar_pin(kyub_inventory_t *inv, int hotbar_pos, int slot_idx);
void        inv_hotbar_cycle(kyub_inventory_t *inv, int delta);

/* Returns the grid slot index (0–35) the cursor is over, or -1. */
int inv_hovered_slot(float mouse_x, float mouse_y, float win_w, float win_h);

/* ── Rendering ───────────────────────────────────────────────────────────── */

typedef struct {
    R_Program shader;       /* inv_program: textured icons (VERTEX_FORMAT_ICON) */
    R_VAO     vao_icon;
    R_Buffer  vbo_icon;
    R_VAO     vao_bg;       /* slot backgrounds (VERTEX_FORMAT_HUD)              */
    R_Buffer  vbo_bg;
    int       loc_texture;
    int       loc_color;
    int       loc_alpha;
} inv_renderer_t;

bool inv_renderer_init(inv_renderer_t *r);
void inv_renderer_shutdown(inv_renderer_t *r);

/* Draw the 9-slot hotbar strip at the bottom of the screen. Always visible. */
void inv_draw_hotbar(inv_renderer_t *r, const kyub_inventory_t *inv,
                     R_Texture tex_array, R_Program hud_prog,
                     ui_gl_t *gui, float win_w, float win_h);

/* Draw the full inventory screen (9×4 grid + hotbar row). Call only when open. */
void inv_draw_screen(inv_renderer_t *r, const kyub_inventory_t *inv,
                     R_Texture tex_array, R_Program hud_prog,
                     ui_gl_t *gui, float win_w, float win_h, int hovered_slot);
