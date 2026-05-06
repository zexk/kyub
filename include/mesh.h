#ifndef MESH_H
#define MESH_H

#include "voxel.h"
#include "gl_ext.h"

typedef struct {
    float x, y, z, w;
    float r, g, b, a;
    float nx, ny, nz, ao;
    float u, v, p1, p2;
} Vertex;

typedef struct {
    Vertex *vertices;
    uint32_t vertex_count;
    uint32_t vertex_capacity;
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    GLuint indirect_draw_buffer;
    GLuint atomic_counter_buffer;
} Mesh;

void mesh_init(Mesh *mesh);
void mesh_generate_greedy(Mesh *mesh, Chunk *chunk);
void mesh_generate_gpu(Mesh *mesh, GLuint compute_program, GLuint voxel_tex, int chunk_x, int chunk_z);
void mesh_upload(Mesh *mesh);
void mesh_prepare_gpu(Mesh *mesh);
void mesh_update_draw_count(Mesh *mesh);
void mesh_free(Mesh *mesh);


#endif // MESH_H
