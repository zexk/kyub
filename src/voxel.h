#pragma once

#include "kv.h"

/* Block IDs — set by kyub_blocks_register(), valid after that call. */
extern uint16_t BLOCK_DIRT;
extern uint16_t BLOCK_GRASS;
extern uint16_t BLOCK_STONE;
extern uint16_t BLOCK_SAND;
extern uint16_t BLOCK_GRAVEL;
extern uint16_t BLOCK_WOOD;
extern uint16_t BLOCK_LEAVES;
extern uint16_t BLOCK_WATER;

/* Register all kyub block types with kiln-voxel. Call before kv_world_create(). */
void kyub_blocks_register(void);

/* Register all kyub item types. Call after kv_build_texture_array(). */
void kyub_items_register(void);

/* Terrain generator (matches kv_gen_fn). Pass to kv_world_create(). */
void kyub_terrain_gen(
    uint16_t blocks[KV_CHUNK_SIZE][KV_CHUNK_SIZE][KV_CHUNK_SIZE],
    uint16_t meta[KV_CHUNK_SIZE][KV_CHUNK_SIZE][KV_CHUNK_SIZE],
    int32_t cx, int32_t cy, int32_t cz,
    void *ctx);
