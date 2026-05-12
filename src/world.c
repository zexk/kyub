#include "world.h"
#include "logger.h"
#include <stdlib.h>
#include <math.h>

#define MAX_RENDER_DISTANCE 8

void world_init(World *world, int render_distance) {
    if (render_distance < 1) render_distance = 1;
    if (render_distance > MAX_RENDER_DISTANCE) render_distance = MAX_RENDER_DISTANCE;
    world->render_distance = render_distance;
    world->capacity = (2 * MAX_RENDER_DISTANCE + 1) * (2 * MAX_RENDER_DISTANCE + 1);
    world->chunks = calloc(world->capacity, sizeof(LoadedChunk));
    world->count = 0;
#ifdef ENABLE_COMPUTE
    world->mesh_compute_program = renderer_create_compute("build/shaders/mesh");
#endif
}

static int world_chunk_coord(int x) {
    if (x >= 0) return x / CHUNK_SIZE;
    return -((-x + CHUNK_SIZE - 1) / CHUNK_SIZE);
}

static bool chunk_is_loaded(const World *world, int x, int z) {
    for (int i = 0; i < world->capacity; i++) {
        if (world->chunks[i].active && world->chunks[i].chunk->x == x && world->chunks[i].chunk->z == z)
            return true;
    }
    return false;
}

static void load_chunk(World *world, int x, int z) {
    for (int i = 0; i < world->capacity; i++) {
        if (!world->chunks[i].active) {
            world->chunks[i].chunk = malloc(sizeof(Chunk));
            chunk_init(world->chunks[i].chunk, x, z);
            world->chunks[i].mesh = malloc(sizeof(Mesh));
            mesh_init(world->chunks[i].mesh);
            world->chunks[i].voxel_tex = R_INVALID_HANDLE;
            voxel_upload_texture(&world->chunks[i].voxel_tex, world->chunks[i].chunk);
            mesh_generate_greedy(world->chunks[i].mesh, world->chunks[i].chunk);
            mesh_upload(world->chunks[i].mesh);
            world->chunks[i].active = true;
            world->chunks[i].dirty = false;
            world->count++;
            LOG_DEBUG(CAT_WORLD, "Loaded chunk %d,%d (slot %d)", x, z, i);
            return;
        }
    }
    LOG_WARN(CAT_WORLD, "load_chunk FAILED: no free slot for chunk %d,%d (active=%d, capacity=%d)", x, z, world->count, world->capacity);
}

static void unload_chunk(World *world, int index) {
    if (!world->chunks[index].active) return;
    int cx = world->chunks[index].chunk->x;
    int cz = world->chunks[index].chunk->z;
    mesh_free(world->chunks[index].mesh);
    free(world->chunks[index].mesh);
    free(world->chunks[index].chunk);
    if (world->chunks[index].voxel_tex != R_INVALID_HANDLE) renderer_destroy_texture(world->chunks[index].voxel_tex);
    world->chunks[index].active = false;
    world->count--;
    LOG_DEBUG(CAT_WORLD, "Unloaded chunk %d,%d", cx, cz);
}

void world_update(World *world, vec3 camera_pos) {
    int cx = world_chunk_coord((int)floorf(camera_pos.x));
    int cz = world_chunk_coord((int)floorf(camera_pos.z));

    // Load new chunks (+1 buffer for interaction range at chunk boundaries)
    int load_dist = world->render_distance + 1;
    for (int x = cx - load_dist; x <= cx + load_dist; x++) {
        for (int z = cz - load_dist; z <= cz + load_dist; z++) {
            if (!chunk_is_loaded(world, x, z)) {
                load_chunk(world, x, z);
            }
        }
    }

    for (int i = 0; i < world->capacity; i++) {
        if (world->chunks[i].active) {
            int dx = abs(world->chunks[i].chunk->x - cx);
            int dz = abs(world->chunks[i].chunk->z - cz);
            if (dx > world->render_distance + 2 || dz > world->render_distance + 2) {
                unload_chunk(world, i);
            }
        }
    }

    // Rebuild dirty chunks (deferred mesh update)
    for (int i = 0; i < world->capacity; i++) {
        if (world->chunks[i].active && world->chunks[i].dirty) {
            mesh_generate_greedy(world->chunks[i].mesh, world->chunks[i].chunk);
            mesh_upload(world->chunks[i].mesh);
            world->chunks[i].dirty = false;
        }
    }
}

void world_free(World *world) {
    for (int i = 0; i < world->capacity; i++) {
        if (world->chunks[i].active) unload_chunk(world, i);
    }
    free(world->chunks);
}

static LoadedChunk* find_chunk(const World *world, int cx, int cz) {
    for (int i = 0; i < world->capacity; i++) {
        if (world->chunks[i].active && world->chunks[i].chunk->x == cx && world->chunks[i].chunk->z == cz)
            return &world->chunks[i];
    }
    return NULL;
}

BlockType world_get_block(const World *world, int x, int y, int z) {
    if (y < 0 || y >= CHUNK_SIZE) return BLOCK_AIR;
    int cx = world_chunk_coord(x);
    int cz = world_chunk_coord(z);
    LoadedChunk *lc = find_chunk(world, cx, cz);
    if (!lc) {
        LOG_WARN(CAT_WORLD, "get_block: chunk %d,%d not loaded for block %d,%d,%d", cx, cz, x, y, z);
        return BLOCK_AIR;
    }
    int lx = x - cx * CHUNK_SIZE;
    int lz = z - cz * CHUNK_SIZE;
    return lc->chunk->blocks[lx][y][lz];
}

bool world_is_solid(const World *world, int x, int y, int z) {
    return world_get_block(world, x, y, z) != BLOCK_AIR;
}

void world_set_block(World *world, int x, int y, int z, BlockType type) {
    if (y < 0 || y >= CHUNK_SIZE) return;
    int cx = world_chunk_coord(x);
    int cz = world_chunk_coord(z);
    LoadedChunk *lc = find_chunk(world, cx, cz);
    if (!lc) {
        if (world->count < world->capacity) {
            load_chunk(world, cx, cz);
            lc = find_chunk(world, cx, cz);
        }
    }
    if (!lc) {
        LOG_WARN(CAT_WORLD, "set_block: chunk %d,%d not loaded for block %d,%d,%d", cx, cz, x, y, z);
        return;
    }
    int lx = x - cx * CHUNK_SIZE;
    int lz = z - cz * CHUNK_SIZE;
    lc->chunk->blocks[lx][y][lz] = type;
    lc->dirty = true;
}
