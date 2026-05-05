#ifndef WORLD_H
#define WORLD_H

#include "voxel.h"
#include "mesh.h"

typedef struct {
    Chunk *chunk;
    Mesh *mesh;
    bool active;
} LoadedChunk;

typedef struct {
    LoadedChunk *chunks;
    int capacity;
    int count;
    int render_distance;
} World;

void world_init(World *world, int render_distance);
void world_update(World *world, vec3 camera_pos);
void world_free(World *world);

#endif // WORLD_H
