#include "voxel.h"
#include "noise.h"
#include "math3d.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

void chunk_init(Chunk *chunk, int x, int z) {
    chunk->x = x;
    chunk->z = z;

    // AABB Calculation (assuming y range is 0 to CHUNK_SIZE)
    chunk->min = (vec3){(float)(x * CHUNK_SIZE), 0.0f, (float)(z * CHUNK_SIZE)};
    chunk->max = (vec3){(float)((x + 1) * CHUNK_SIZE), (float)CHUNK_SIZE, (float)((z + 1) * CHUNK_SIZE)};

    memset(chunk->blocks, BLOCK_AIR, sizeof(chunk->blocks));

    for (int lx = 0; lx < CHUNK_SIZE; lx++) {
        for (int lz = 0; lz < CHUNK_SIZE; lz++) {
            float world_x = (float)(x * CHUNK_SIZE + lx);
            float world_z = (float)(z * CHUNK_SIZE + lz);
            float n = perlin2d(world_x, world_z, 0.01f, 4);
            int height = (int)(n * (CHUNK_SIZE - 1));

            for (int y = 0; y <= height; y++) {
                if (y == height) chunk->blocks[lx][y][lz] = BLOCK_GRASS;
                else if (y > height - 3) chunk->blocks[lx][y][lz] = BLOCK_DIRT;
                else chunk->blocks[lx][y][lz] = BLOCK_STONE;
            }
        }
    }
}

void world_init(World *world, int grid_size) {
    world->grid_size = grid_size;
    world->chunks = malloc(sizeof(Chunk) * grid_size * grid_size);
    for (int x = 0; x < grid_size; x++) {
        for (int z = 0; z < grid_size; z++) {
            chunk_init(&world->chunks[x * grid_size + z], x - grid_size/2, z - grid_size/2);
        }
    }
}

void world_free(World *world) {
    free(world->chunks);
}
