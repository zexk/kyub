#ifndef VOXEL_H
#define VOXEL_H

#include <stdint.h>
#include "renderer/renderer.h"
#include "math3d.h"

#define CHUNK_SIZE 16

typedef enum {
    BLOCK_AIR = 0,
    BLOCK_DIRT = 1,
    BLOCK_GRASS = 2,
    BLOCK_STONE = 3,
    BLOCK_SAND = 4,
    BLOCK_GRAVEL = 5,
    BLOCK_WOOD = 6,
    BLOCK_LEAVES = 7,
} BlockType;

typedef struct {
    uint8_t blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
    int x, z;
    vec3 min, max;
} Chunk;

void voxel_upload_texture(R_Texture *tex, const Chunk *chunk);
void chunk_init(Chunk *chunk, int x, int z);

#endif // VOXEL_H
