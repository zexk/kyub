#include "world.h"
#include "logger.h"
#include <stdlib.h>
#include <stdio.h>

#define MAX_RENDER_DISTANCE 8

void world_init(World *world, int render_distance) {
    if (render_distance < 1) render_distance = 1;
    if (render_distance > MAX_RENDER_DISTANCE) render_distance = MAX_RENDER_DISTANCE;
    world->render_distance = render_distance;
    world->capacity = (2 * MAX_RENDER_DISTANCE + 1) * (2 * MAX_RENDER_DISTANCE + 1);
    world->chunks = calloc(world->capacity, sizeof(LoadedChunk));
    world->count = 0;
#ifdef ENABLE_COMPUTE
    world->mesh_compute_program = renderer_create_compute("shaders/mesh.comp");
#endif
}

static bool chunk_is_loaded(World *world, int x, int z) {
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
            voxel_upload_texture(&world->chunks[i].voxel_tex, world->chunks[i].chunk);
#if defined(ENABLE_COMPUTE) && !defined(RENDERER_VULKAN)
            mesh_prepare_gpu(world->chunks[i].mesh);
            mesh_generate_gpu(world->chunks[i].mesh, world->mesh_compute_program, world->chunks[i].voxel_tex, x, z);
#else
            mesh_generate_greedy(world->chunks[i].mesh, world->chunks[i].chunk);
            mesh_upload(world->chunks[i].mesh);
#endif
            world->chunks[i].active = true;
            world->chunks[i].dirty = false;
            world->count++;
            LOG_DEBUG(CAT_WORLD, "Loaded chunk %d,%d (slot %d)", x, z, i);
            return;
        }
    }
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
    int cx = (int)(camera_pos.x / CHUNK_SIZE);
    int cz = (int)(camera_pos.z / CHUNK_SIZE);
    if (camera_pos.x < 0) cx--;
    if (camera_pos.z < 0) cz--;

    // Load new chunks (+1 buffer for interaction range at chunk boundaries)
    int load_dist = world->render_distance + 1;
    for (int x = cx - load_dist; x <= cx + load_dist; x++) {
        for (int z = cz - load_dist; z <= cz + load_dist; z++) {
            if (!chunk_is_loaded(world, x, z)) {
                load_chunk(world, x, z);
            }
        }
    }

    // Unload old chunks
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
            int cx = world->chunks[i].chunk->x;
            int cz = world->chunks[i].chunk->z;
#if defined(ENABLE_COMPUTE) && !defined(RENDERER_VULKAN)
            renderer_bind_texture(R_TEX_3D, world->chunks[i].voxel_tex);
            renderer_tex_sub_image_3d(0, 0, 0, CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, world->chunks[i].chunk->blocks);
            renderer_bind_texture(R_TEX_3D, R_INVALID_HANDLE);
            mesh_generate_gpu(world->chunks[i].mesh, world->mesh_compute_program, world->chunks[i].voxel_tex, cx, cz);
#else
            mesh_generate_greedy(world->chunks[i].mesh, world->chunks[i].chunk);
            mesh_upload(world->chunks[i].mesh);
#endif
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

static LoadedChunk* find_chunk(World *world, int cx, int cz) {
    for (int i = 0; i < world->capacity; i++) {
        if (world->chunks[i].active && world->chunks[i].chunk->x == cx && world->chunks[i].chunk->z == cz)
            return &world->chunks[i];
    }
    return NULL;
}

BlockType world_get_block(World *world, int x, int y, int z) {
    if (y < 0 || y >= CHUNK_SIZE) return BLOCK_AIR;
    int cx = (int)floorf((float)x / CHUNK_SIZE);
    int cz = (int)floorf((float)z / CHUNK_SIZE);
    LoadedChunk *lc = find_chunk(world, cx, cz);
    if (!lc) {
        LOG_WARN(CAT_WORLD, "get_block: chunk %d,%d not loaded for block %d,%d,%d", cx, cz, x, y, z);
        return BLOCK_AIR;
    }
    int lx = x - cx * CHUNK_SIZE;
    int lz = z - cz * CHUNK_SIZE;
    return lc->chunk->blocks[lx][y][lz];
}

bool world_is_solid(World *world, int x, int y, int z) {
    return world_get_block(world, x, y, z) != BLOCK_AIR;
}

void world_set_block(World *world, int x, int y, int z, BlockType type) {
    if (y < 0 || y >= CHUNK_SIZE) return;
    int cx = (int)floorf((float)x / CHUNK_SIZE);
    int cz = (int)floorf((float)z / CHUNK_SIZE);
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
