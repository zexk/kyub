#include "voxel.h"
#include "inventory.h"
#include "noise.h"
#include <string.h>
#include <math.h>

uint16_t BLOCK_DIRT;
uint16_t BLOCK_GRASS;
uint16_t BLOCK_STONE;
uint16_t BLOCK_SAND;
uint16_t BLOCK_GRAVEL;
uint16_t BLOCK_WOOD;
uint16_t BLOCK_LEAVES;
uint16_t BLOCK_WATER;

void kyub_blocks_register(void) {
    BLOCK_DIRT   = kv_block_register(&(kv_block_def_t){ .id="kyub:dirt",   .name="Dirt",   .solid=true, .opaque=true, .hardness=1.0f, .tex_path="assets/textures/dirt.png" });
    BLOCK_GRASS  = kv_block_register(&(kv_block_def_t){ .id="kyub:grass",  .name="Grass",  .solid=true, .opaque=true, .hardness=1.0f, .tex_path="assets/textures/dirt.png", .tex_top="assets/textures/grass_top.png", .tex_side="assets/textures/grass_side.png" });
    BLOCK_STONE  = kv_block_register(&(kv_block_def_t){ .id="kyub:stone",  .name="Stone",  .solid=true, .opaque=true, .hardness=2.0f, .tex_path="assets/textures/stone.png" });
    BLOCK_SAND   = kv_block_register(&(kv_block_def_t){ .id="kyub:sand",   .name="Sand",   .solid=true, .opaque=true, .hardness=1.0f, .tex_path="assets/textures/sand.png" });
    BLOCK_GRAVEL = kv_block_register(&(kv_block_def_t){ .id="kyub:gravel", .name="Gravel", .solid=true, .opaque=true, .hardness=1.0f, .tex_path="assets/textures/gravel.png" });
    BLOCK_WOOD   = kv_block_register(&(kv_block_def_t){ .id="kyub:wood",   .name="Wood",   .solid=true, .opaque=true, .hardness=2.0f, .tex_path="assets/textures/wood.png" });
    BLOCK_LEAVES = kv_block_register(&(kv_block_def_t){ .id="kyub:leaves", .name="Leaves", .solid=true, .opaque=false,.hardness=0.5f, .tex_path="assets/textures/leaves.png" });
    BLOCK_WATER  = kv_block_register(&(kv_block_def_t){ .id="kyub:water",  .name="Water",  .solid=true, .opaque=true, .hardness=0.0f, .tex_path="assets/textures/gravel.png", .r=0.15f, .g=0.50f, .b=1.00f });
}

void kyub_items_register(void) {
    /* One item per block, 1:1 mapping — expand as non-block items are added. */
    kyub_item_register(&(kyub_item_def_t){ .id="kyub:dirt",   .name="Dirt",   .max_stack=256, .block_type=BLOCK_DIRT,   .tex_layer=kv_block_tex_layer_top(BLOCK_DIRT)   });
    kyub_item_register(&(kyub_item_def_t){ .id="kyub:grass",  .name="Grass",  .max_stack=256, .block_type=BLOCK_GRASS,  .tex_layer=kv_block_tex_layer_top(BLOCK_GRASS)  });
    kyub_item_register(&(kyub_item_def_t){ .id="kyub:stone",  .name="Stone",  .max_stack=256, .block_type=BLOCK_STONE,  .tex_layer=kv_block_tex_layer_top(BLOCK_STONE)  });
    kyub_item_register(&(kyub_item_def_t){ .id="kyub:sand",   .name="Sand",   .max_stack=256, .block_type=BLOCK_SAND,   .tex_layer=kv_block_tex_layer_top(BLOCK_SAND)   });
    kyub_item_register(&(kyub_item_def_t){ .id="kyub:gravel", .name="Gravel", .max_stack=256, .block_type=BLOCK_GRAVEL, .tex_layer=kv_block_tex_layer_top(BLOCK_GRAVEL) });
    kyub_item_register(&(kyub_item_def_t){ .id="kyub:wood",   .name="Wood",   .max_stack=256, .block_type=BLOCK_WOOD,   .tex_layer=kv_block_tex_layer_top(BLOCK_WOOD)   });
    kyub_item_register(&(kyub_item_def_t){ .id="kyub:leaves", .name="Leaves", .max_stack=256, .block_type=BLOCK_LEAVES, .tex_layer=kv_block_tex_layer_top(BLOCK_LEAVES) });
    kyub_item_register(&(kyub_item_def_t){ .id="kyub:water",  .name="Water",  .max_stack=256, .block_type=BLOCK_WATER,  .tex_layer=kv_block_tex_layer_top(BLOCK_WATER)  });
}

/* ── Terrain generator ───────────────────────────────────────────────────── */

#define SEA_LEVEL 5

typedef enum { BIOME_PLAINS, BIOME_DESERT, BIOME_FOREST, BIOME_TUNDRA, BIOME_MOUNTAINS } Biome;

static Biome get_biome(float temp, float humidity, float mountain) {
    if (mountain > 0.62f)                 return BIOME_MOUNTAINS;
    if (temp > 0.65f && humidity < 0.48f) return BIOME_DESERT;
    if (temp < 0.38f)                     return BIOME_TUNDRA;
    if (humidity > 0.56f)                 return BIOME_FOREST;
    return BIOME_PLAINS;
}

static int surface_height(Biome b, float wx, float wz) {
    switch (b) {
    case BIOME_PLAINS:    return 7  + (int)(noise_fbm2d(wx*0.012f,          wz*0.012f,          4, 2.0f, 0.5f) * 4.0f);
    case BIOME_DESERT:    return 4  + (int)(noise_fbm2d(wx*0.006f,          wz*0.006f,          3, 2.0f, 0.5f) * 3.0f);
    case BIOME_FOREST:    return 8  + (int)(noise_fbm2d(wx*0.016f,          wz*0.016f,          4, 2.0f, 0.5f) * 5.0f);
    case BIOME_TUNDRA:    return 5  + (int)(noise_fbm2d(wx*0.008f,          wz*0.008f,          3, 2.0f, 0.5f) * 4.0f);
    case BIOME_MOUNTAINS: return 9  + (int)(noise_fbm2d(wx*0.022f,          wz*0.022f,          5, 2.0f, 0.5f) * 6.0f);
    default:              return 7;
    }
}

static uint16_t column_block(Biome b, int depth) {
    switch (b) {
    case BIOME_DESERT:    return (depth < 4) ? BLOCK_SAND   : BLOCK_STONE;
    case BIOME_MOUNTAINS: return (depth == 0) ? BLOCK_STONE : (depth < 2) ? BLOCK_GRAVEL : BLOCK_STONE;
    case BIOME_TUNDRA:    return (depth == 0) ? BLOCK_GRAVEL: (depth < 3) ? BLOCK_DIRT   : BLOCK_STONE;
    case BIOME_FOREST:    return (depth == 0) ? BLOCK_GRASS : (depth < 5) ? BLOCK_DIRT   : BLOCK_STONE;
    default:              return (depth == 0) ? BLOCK_GRASS : (depth < 3) ? BLOCK_DIRT   : BLOCK_STONE;
    }
}

static bool is_cave(float wx, float wy, float wz) {
    float n1 = noise_fbm3d(wx*0.040f,          wy*0.055f,          wz*0.040f,          3, 2.0f, 0.5f);
    float n2 = noise_fbm3d(wx*0.040f+300.0f,   wy*0.055f+300.0f,   wz*0.040f+300.0f,   3, 2.0f, 0.5f);
    float d1=n1-0.5f, d2=n2-0.5f;
    return (d1*d1 + d2*d2) < 0.018f;
}

void kyub_terrain_gen(
    uint16_t blocks[KV_CHUNK_SIZE][KV_CHUNK_SIZE][KV_CHUNK_SIZE],
    uint16_t meta[KV_CHUNK_SIZE][KV_CHUNK_SIZE][KV_CHUNK_SIZE],
    int32_t cx, int32_t cy, int32_t cz,
    void *ctx) {
    (void)meta; (void)ctx;

    int wy_base = cy * KV_CHUNK_SIZE;

    for (int lx = 0; lx < KV_CHUNK_SIZE; lx++) {
        for (int lz = 0; lz < KV_CHUNK_SIZE; lz++) {
            float wx = (float)(cx * KV_CHUNK_SIZE + lx);
            float wz = (float)(cz * KV_CHUNK_SIZE + lz);

            float temp     = noise_fbm2d(wx*0.0020f,           wz*0.0020f,           3, 2.0f, 0.5f);
            float humidity = noise_fbm2d(wx*0.0020f + 500.0f,  wz*0.0020f + 500.0f,  3, 2.0f, 0.5f);
            float mountain = noise_fbm2d(wx*0.0030f + 1000.0f, wz*0.0030f + 1000.0f, 2, 2.0f, 0.5f);
            Biome biome    = get_biome(temp, humidity, mountain);
            int   surf     = surface_height(biome, wx, wz);

            for (int ly = 0; ly < KV_CHUNK_SIZE; ly++) {
                int wy = wy_base + ly;
                if (wy > surf) {
                    blocks[lx][ly][lz] = KV_BLOCK_AIR;
                } else {
                    blocks[lx][ly][lz] = column_block(biome, surf - wy);
                }
            }
        }
    }

    /* Cave carving — protect the 2 blocks directly below each column's surface */
    for (int lx = 0; lx < KV_CHUNK_SIZE; lx++) {
        for (int lz = 0; lz < KV_CHUNK_SIZE; lz++) {
            float wx = (float)(cx * KV_CHUNK_SIZE + lx);
            float wz = (float)(cz * KV_CHUNK_SIZE + lz);
            float temp     = noise_fbm2d(wx*0.0020f,           wz*0.0020f,           3, 2.0f, 0.5f);
            float humidity = noise_fbm2d(wx*0.0020f + 500.0f,  wz*0.0020f + 500.0f,  3, 2.0f, 0.5f);
            float mountain = noise_fbm2d(wx*0.0030f + 1000.0f, wz*0.0030f + 1000.0f, 2, 2.0f, 0.5f);
            int surf = surface_height(get_biome(temp, humidity, mountain), wx, wz);

            for (int ly = 1; ly < KV_CHUNK_SIZE; ly++) {
                int wy = wy_base + ly;
                if (wy >= surf - 1) continue;
                if (blocks[lx][ly][lz] == KV_BLOCK_AIR) continue;
                if (is_cave(wx, (float)wy, wz))
                    blocks[lx][ly][lz] = KV_BLOCK_AIR;
            }
        }
    }

    /* Sea-level water fill */
    for (int lx = 0; lx < KV_CHUNK_SIZE; lx++) {
        for (int lz = 0; lz < KV_CHUNK_SIZE; lz++) {
            for (int ly = KV_CHUNK_SIZE - 1; ly >= 1; ly--) {
                int wy = wy_base + ly;
                if (wy > SEA_LEVEL) continue;
                if (blocks[lx][ly][lz] == KV_BLOCK_AIR)
                    blocks[lx][ly][lz] = BLOCK_WATER;
                else
                    break;
            }
        }
    }
}
