#ifndef MESH_H
#define MESH_H

#include "voxel.h"
#include "gl_ext.h"

typedef struct {
    float x, y, z;
    float r, g, b;
    float nx, ny, nz;
    float ao;
    float u, v;
} Vertex;

typedef struct {
    Vertex *vertices;
    uint32_t vertex_count;
    uint32_t vertex_capacity;
    GLuint vao;
    GLuint vbo;
} Mesh;

void mesh_init(Mesh *mesh);
void mesh_generate_greedy(Mesh *mesh, Chunk *chunk);
void mesh_upload(Mesh *mesh);
void mesh_free(Mesh *mesh);

#endif // MESH_H
