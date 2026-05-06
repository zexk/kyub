#ifndef WORLD_H
#define WORLD_H

#include "voxel.h"
#include "mesh.h"
#include "math3d.h"

typedef struct {
    Chunk *chunk;
    Mesh *mesh;
    bool active;
    GLuint voxel_tex;
} LoadedChunk;

typedef struct {
    LoadedChunk *chunks;
    int capacity;
    int count;
    int render_distance;
    GLuint mesh_compute_program;
} World;

void world_init(World *world, int render_distance);
void world_update(World *world, vec3 camera_pos);
void world_free(World *world);
BlockType world_get_block(World *world, int x, int y, int z);
void world_set_block(World *world, int x, int y, int z, BlockType type);

#endif // WORLD_H
