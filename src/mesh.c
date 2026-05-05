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

static void add_vertex(Mesh *mesh, float x, float y, float z, float r, float g, float b, float nx, float ny, float nz, float ao, float u, float v) {
    if (mesh->vertex_count >= mesh->vertex_capacity) {
        mesh->vertex_capacity *= 2;
        mesh->vertices = realloc(mesh->vertices, sizeof(Vertex) * mesh->vertex_capacity);
    }
    mesh->vertices[mesh->vertex_count++] = (Vertex){x, y, z, r, g, b, nx, ny, nz, ao, u, v};
}

typedef struct { float r, g, b; } Color;
static Color get_block_color(BlockType type) { return (Color){1.0f, 1.0f, 1.0f}; }

typedef struct { float u_off, v_off, w, h; } UVRect;
static UVRect get_uv_rect(BlockType type) {
    float s = 0.5f;
    switch (type) {
        case BLOCK_GRASS: return (UVRect){0.0f, 0.0f, s, s};
        case BLOCK_DIRT:  return (UVRect){s, 0.0f, s, s};
        case BLOCK_STONE: return (UVRect){0.0f, s, s, s};
        default:          return (UVRect){s, s, s, s};
    }
}

static bool is_transparent(Chunk *chunk, int x, int y, int z) {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) return true;
    return chunk->blocks[x][y][z] == BLOCK_AIR;
}

static void add_face(Mesh *mesh, Chunk *chunk, int x, int y, int z, int face, BlockType type) {
    float ox = (float)(x + chunk->x * CHUNK_SIZE);
    float oy = (float)y;
    float oz = (float)(z + chunk->z * CHUNK_SIZE);
    Color c = {1.0f, 1.0f, 1.0f};
    UVRect uv = get_uv_rect(type);

    float p[4][3];
    float nx=0, ny=0, nz=0;
    if (face == 0) { // Front (+Z)
        nz = 1; p[0][0]=ox; p[0][1]=oy; p[0][2]=oz+1; p[1][0]=ox+1; p[1][1]=oy; p[1][2]=oz+1; p[2][0]=ox+1; p[2][1]=oy+1; p[2][2]=oz+1; p[3][0]=ox; p[3][1]=oy+1; p[3][2]=oz+1;
    } else if (face == 1) { // Back (-Z)
        nz = -1; p[0][0]=ox+1; p[0][1]=oy; p[0][2]=oz; p[1][0]=ox; p[1][1]=oy; p[1][2]=oz; p[2][0]=ox; p[2][1]=oy+1; p[2][2]=oz; p[3][0]=ox+1; p[3][1]=oy+1; p[3][2]=oz;
    } else if (face == 2) { // Left (-X)
        nx = -1; p[0][0]=ox; p[0][1]=oy; p[0][2]=oz; p[1][0]=ox; p[1][1]=oy; p[1][2]=oz+1; p[2][0]=ox; p[2][1]=oy+1; p[2][2]=oz+1; p[3][0]=ox; p[3][1]=oy+1; p[3][2]=oz;
    } else if (face == 3) { // Right (+X)
        nx = 1; p[0][0]=ox+1; p[0][1]=oy; p[0][2]=oz+1; p[1][0]=ox+1; p[1][1]=oy; p[1][2]=oz; p[2][0]=ox+1; p[2][1]=oy+1; p[2][2]=oz; p[3][0]=ox+1; p[3][1]=oy+1; p[3][2]=oz+1;
    } else if (face == 4) { // Top (+Y)
        ny = 1; p[0][0]=ox; p[0][1]=oy+1; p[0][2]=oz+1; p[1][0]=ox+1; p[1][1]=oy+1; p[1][2]=oz+1; p[2][0]=ox+1; p[2][1]=oy+1; p[2][2]=oz; p[3][0]=ox; p[3][1]=oy+1; p[3][2]=oz;
    } else { // Bottom (-Y)
        ny = -1; p[0][0]=ox; p[0][1]=oy; p[0][2]=oz; p[1][0]=ox+1; p[1][1]=oy; p[1][2]=oz; p[2][0]=ox+1; p[2][1]=oy; p[2][2]=oz+1; p[3][0]=ox; p[3][1]=oy; p[3][2]=oz+1;
    }

    add_vertex(mesh, p[0][0], p[0][1], p[0][2], c.r, c.g, c.b, nx, ny, nz, 1.0f, uv.u_off, uv.v_off);
    add_vertex(mesh, p[1][0], p[1][1], p[1][2], c.r, c.g, c.b, nx, ny, nz, 1.0f, uv.u_off + uv.w, uv.v_off);
    add_vertex(mesh, p[2][0], p[2][1], p[2][2], c.r, c.g, c.b, nx, ny, nz, 1.0f, uv.u_off + uv.w, uv.v_off + uv.h);
    add_vertex(mesh, p[0][0], p[0][1], p[0][2], c.r, c.g, c.b, nx, ny, nz, 1.0f, uv.u_off, uv.v_off);
    add_vertex(mesh, p[2][0], p[2][1], p[2][2], c.r, c.g, c.b, nx, ny, nz, 1.0f, uv.u_off + uv.w, uv.v_off + uv.h);
    add_vertex(mesh, p[3][0], p[3][1], p[3][2], c.r, c.g, c.b, nx, ny, nz, 1.0f, uv.u_off, uv.v_off + uv.h);
}

void mesh_generate_greedy(Mesh *mesh, Chunk *chunk) {
    mesh->vertex_count = 0;
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                if (chunk->blocks[x][y][z] == BLOCK_AIR) continue;
                BlockType type = chunk->blocks[x][y][z];
                if (is_transparent(chunk, x, y, z + 1)) add_face(mesh, chunk, x, y, z, 0, type);
                if (is_transparent(chunk, x, y, z - 1)) add_face(mesh, chunk, x, y, z, 1, type);
                if (is_transparent(chunk, x - 1, y, z)) add_face(mesh, chunk, x, y, z, 2, type);
                if (is_transparent(chunk, x + 1, y, z)) add_face(mesh, chunk, x, y, z, 3, type);
                if (is_transparent(chunk, x, y + 1, z)) add_face(mesh, chunk, x, y, z, 4, type);
                if (is_transparent(chunk, x, y - 1, z)) add_face(mesh, chunk, x, y, z, 5, type);
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
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
void mesh_free(Mesh *mesh) {
    free(mesh->vertices);
    if (mesh->vbo != 0) glDeleteBuffers(1, &mesh->vbo);
    if (mesh->vao != 0) glDeleteVertexArrays(1, &mesh->vao);
}
