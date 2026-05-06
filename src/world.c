#include "world.h"
#include "shader.h"
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
    world->mesh_compute_program = shader_create_compute_program("shaders/mesh.comp");
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
#ifdef ENABLE_COMPUTE
            mesh_prepare_gpu(world->chunks[i].mesh);
            mesh_generate_gpu(world->chunks[i].mesh, world->mesh_compute_program, world->chunks[i].voxel_tex, x, z);
#else
            mesh_generate_greedy(world->chunks[i].mesh, world->chunks[i].chunk);
            mesh_upload(world->chunks[i].mesh);
#endif
            world->chunks[i].active = true;
            world->count++;
            return;
        }
    }
}

static void unload_chunk(World *world, int index) {
    if (!world->chunks[index].active) return;
    mesh_free(world->chunks[index].mesh);
    free(world->chunks[index].mesh);
    free(world->chunks[index].chunk);
    if (world->chunks[index].voxel_tex) glDeleteTextures(1, &world->chunks[index].voxel_tex);
    world->chunks[index].active = false;
    world->count--;
}

void world_update(World *world, vec3 camera_pos) {
    int cx = (int)(camera_pos.x / CHUNK_SIZE);
    int cz = (int)(camera_pos.z / CHUNK_SIZE);
    if (camera_pos.x < 0) cx--;
    if (camera_pos.z < 0) cz--;

    // Load new
    for (int x = cx - world->render_distance; x <= cx + world->render_distance; x++) {
        for (int z = cz - world->render_distance; z <= cz + world->render_distance; z++) {
            if (!chunk_is_loaded(world, x, z)) {
                load_chunk(world, x, z);
            }
        }
    }

    // Unload old
    for (int i = 0; i < world->capacity; i++) {
        if (world->chunks[i].active) {
            int dx = abs(world->chunks[i].chunk->x - cx);
            int dz = abs(world->chunks[i].chunk->z - cz);
            if (dx > world->render_distance || dz > world->render_distance) {
                unload_chunk(world, i);
            }
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
    if (!lc) return BLOCK_AIR;
    int lx = x - cx * CHUNK_SIZE;
    int lz = z - cz * CHUNK_SIZE;
    return lc->chunk->blocks[lx][y][lz];
}

void world_set_block(World *world, int x, int y, int z, BlockType type) {
    if (y < 0 || y >= CHUNK_SIZE) return;
    int cx = (int)floorf((float)x / CHUNK_SIZE);
    int cz = (int)floorf((float)z / CHUNK_SIZE);
    LoadedChunk *lc = find_chunk(world, cx, cz);
    if (!lc) return;
    int lx = x - cx * CHUNK_SIZE;
    int lz = z - cz * CHUNK_SIZE;
    lc->chunk->blocks[lx][y][lz] = type;
#ifdef ENABLE_COMPUTE
    // Update voxel texture on GPU. Swap X and Z offsets for GL layout.
    glBindTexture(GL_TEXTURE_3D, lc->voxel_tex);
    glTexSubImage3D_ext(GL_TEXTURE_3D, 0, lz, y, lx, 1, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_BYTE, &lc->chunk->blocks[lx][y][lz]);
    glBindTexture(GL_TEXTURE_3D, 0);
    mesh_generate_gpu(lc->mesh, world->mesh_compute_program, lc->voxel_tex, cx, cz);
#else
    mesh_generate_greedy(lc->mesh, lc->chunk);
    mesh_upload(lc->mesh);
#endif
}
