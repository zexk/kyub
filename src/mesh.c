#include "mesh.h"
#include <stdlib.h>
#include <string.h>

void mesh_init(Mesh *mesh) {
    mesh->vertex_count = 0;
    mesh->vertex_capacity = 4096;
    mesh->vertices = malloc(sizeof(Vertex) * mesh->vertex_capacity);
    mesh->vao = 0;
    mesh->vbo = 0;
}

static void add_vertex(Mesh *mesh, float x, float y, float z, float r, float g, float b, float nx, float ny, float nz, float ao) {
    if (mesh->vertex_count >= mesh->vertex_capacity) {
        mesh->vertex_capacity *= 2;
        mesh->vertices = realloc(mesh->vertices, sizeof(Vertex) * mesh->vertex_capacity);
    }
    mesh->vertices[mesh->vertex_count++] = (Vertex){x, y, z, r, g, b, nx, ny, nz, ao};
}

typedef struct { float r, g, b; } Color;
static Color get_block_color(BlockType type) {
    switch (type) {
        case BLOCK_DIRT:  return (Color){0.4f, 0.25f, 0.1f};
        case BLOCK_GRASS: return (Color){0.2f, 0.8f, 0.2f};
        case BLOCK_STONE: return (Color){0.5f, 0.5f, 0.5f};
        default:          return (Color){1.0f, 0.0f, 1.0f};
    }
}

static bool is_transparent(Chunk *chunk, int x, int y, int z) {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) return true;
    return chunk->blocks[x][y][z] == BLOCK_AIR;
}

void mesh_generate_greedy(Mesh *mesh, Chunk *chunk) {
    mesh->vertex_count = 0;
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                BlockType type = chunk->blocks[x][y][z];
                if (type == BLOCK_AIR) continue;
                
                Color c = get_block_color(type);
                float ox = (float)(x + chunk->x * CHUNK_SIZE);
                float oy = (float)y;
                float oz = (float)(z + chunk->z * CHUNK_SIZE);

                // Front
                if (is_transparent(chunk, x, y, z + 1)) {
                    add_vertex(mesh, ox, oy, oz + 1, c.r, c.g, c.b, 0, 0, 1, 1.0f);
                    add_vertex(mesh, ox + 1, oy, oz + 1, c.r, c.g, c.b, 0, 0, 1, 1.0f);
                    add_vertex(mesh, ox + 1, oy + 1, oz + 1, c.r, c.g, c.b, 0, 0, 1, 1.0f);
                    add_vertex(mesh, ox, oy, oz + 1, c.r, c.g, c.b, 0, 0, 1, 1.0f);
                    add_vertex(mesh, ox + 1, oy + 1, oz + 1, c.r, c.g, c.b, 0, 0, 1, 1.0f);
                    add_vertex(mesh, ox, oy + 1, oz + 1, c.r, c.g, c.b, 0, 0, 1, 1.0f);
                }
                // Back
                if (is_transparent(chunk, x, y, z - 1)) {
                    add_vertex(mesh, ox + 1, oy, oz, c.r, c.g, c.b, 0, 0, -1, 1.0f);
                    add_vertex(mesh, ox, oy, oz, c.r, c.g, c.b, 0, 0, -1, 1.0f);
                    add_vertex(mesh, ox, oy + 1, oz, c.r, c.g, c.b, 0, 0, -1, 1.0f);
                    add_vertex(mesh, ox + 1, oy, oz, c.r, c.g, c.b, 0, 0, -1, 1.0f);
                    add_vertex(mesh, ox, oy + 1, oz, c.r, c.g, c.b, 0, 0, -1, 1.0f);
                    add_vertex(mesh, ox + 1, oy + 1, oz, c.r, c.g, c.b, 0, 0, -1, 1.0f);
                }
                // Left
                if (is_transparent(chunk, x - 1, y, z)) {
                    add_vertex(mesh, ox, oy, oz, c.r, c.g, c.b, -1, 0, 0, 1.0f);
                    add_vertex(mesh, ox, oy, oz + 1, c.r, c.g, c.b, -1, 0, 0, 1.0f);
                    add_vertex(mesh, ox, oy + 1, oz + 1, c.r, c.g, c.b, -1, 0, 0, 1.0f);
                    add_vertex(mesh, ox, oy, oz, c.r, c.g, c.b, -1, 0, 0, 1.0f);
                    add_vertex(mesh, ox, oy + 1, oz + 1, c.r, c.g, c.b, -1, 0, 0, 1.0f);
                    add_vertex(mesh, ox, oy + 1, oz, c.r, c.g, c.b, -1, 0, 0, 1.0f);
                }
                // Right
                if (is_transparent(chunk, x + 1, y, z)) {
                    add_vertex(mesh, ox + 1, oy, oz + 1, c.r, c.g, c.b, 1, 0, 0, 1.0f);
                    add_vertex(mesh, ox + 1, oy, oz, c.r, c.g, c.b, 1, 0, 0, 1.0f);
                    add_vertex(mesh, ox + 1, oy + 1, oz, c.r, c.g, c.b, 1, 0, 0, 1.0f);
                    add_vertex(mesh, ox + 1, oy, oz + 1, c.r, c.g, c.b, 1, 0, 0, 1.0f);
                    add_vertex(mesh, ox + 1, oy + 1, oz, c.r, c.g, c.b, 1, 0, 0, 1.0f);
                    add_vertex(mesh, ox + 1, oy + 1, oz + 1, c.r, c.g, c.b, 1, 0, 0, 1.0f);
                }
                // Top
                if (is_transparent(chunk, x, y + 1, z)) {
                    add_vertex(mesh, ox, oy + 1, oz + 1, c.r, c.g, c.b, 0, 1, 0, 1.0f);
                    add_vertex(mesh, ox + 1, oy + 1, oz + 1, c.r, c.g, c.b, 0, 1, 0, 1.0f);
                    add_vertex(mesh, ox + 1, oy + 1, oz, c.r, c.g, c.b, 0, 1, 0, 1.0f);
                    add_vertex(mesh, ox, oy + 1, oz + 1, c.r, c.g, c.b, 0, 1, 0, 1.0f);
                    add_vertex(mesh, ox + 1, oy + 1, oz, c.r, c.g, c.b, 0, 1, 0, 1.0f);
                    add_vertex(mesh, ox, oy + 1, oz, c.r, c.g, c.b, 0, 1, 0, 1.0f);
                }
                // Bottom
                if (is_transparent(chunk, x, y - 1, z)) {
                    add_vertex(mesh, ox, oy, oz, c.r, c.g, c.b, 0, -1, 0, 1.0f);
                    add_vertex(mesh, ox + 1, oy, oz, c.r, c.g, c.b, 0, -1, 0, 1.0f);
                    add_vertex(mesh, ox + 1, oy, oz + 1, c.r, c.g, c.b, 0, -1, 0, 1.0f);
                    add_vertex(mesh, ox, oy, oz, c.r, c.g, c.b, 0, -1, 0, 1.0f);
                    add_vertex(mesh, ox + 1, oy, oz + 1, c.r, c.g, c.b, 0, -1, 0, 1.0f);
                    add_vertex(mesh, ox, oy, oz + 1, c.r, c.g, c.b, 0, -1, 0, 1.0f);
                }
            }
        }
    }
}

void mesh_upload(Mesh *mesh) {
    if (mesh->vao == 0) glGenVertexArrays(1, &mesh->vao);
    if (mesh->vbo == 0) glGenBuffers(1, &mesh->vbo);
    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh->vertex_count * sizeof(Vertex), mesh->vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(9 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void mesh_free(Mesh *mesh) {
    free(mesh->vertices);
    if (mesh->vbo != 0) glDeleteBuffers(1, &mesh->vbo);
    if (mesh->vao != 0) glDeleteVertexArrays(1, &mesh->vao);
}
