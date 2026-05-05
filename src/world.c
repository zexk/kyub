#include "world.h"
#include <stdlib.h>
#include <stdio.h>

void world_init(World *world, int render_distance) {
    world->render_distance = render_distance;
    world->capacity = (2 * render_distance + 1) * (2 * render_distance + 1);
    world->chunks = calloc(world->capacity, sizeof(LoadedChunk));
    world->count = 0;
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
            mesh_generate_greedy(world->chunks[i].mesh, world->chunks[i].chunk);
            mesh_upload(world->chunks[i].mesh);
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
