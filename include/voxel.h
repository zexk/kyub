#ifndef VOXEL_H
#define VOXEL_H

#include <stdint.h>
#include "renderer.h"
#include "math3d.h"

#define CHUNK_SIZE 16

typedef enum {
    BLOCK_AIR = 0,
    BLOCK_DIRT = 1,
    BLOCK_GRASS = 2,
    BLOCK_STONE = 3,
} BlockType;

typedef struct {
    uint8_t blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
    int x, z;
    vec3 min, max;
} Chunk;

void voxel_upload_texture(R_Texture *tex, const Chunk *chunk);
void chunk_init(Chunk *chunk, int x, int z);

#endif // VOXEL_H
