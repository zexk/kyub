#include "mesh.h"
#include <stdlib.h>
#include <string.h>

void mesh_init(Mesh *mesh) {
    mesh->vertex_count = 0;
    mesh->vertex_capacity = 4096;
    mesh->vertices = malloc(sizeof(Vertex) * mesh->vertex_capacity);
    mesh->vao = 0;
    mesh->vbo = 0;
    mesh->ebo = 0;
    mesh->indirect_draw_buffer = 0;
    mesh->atomic_counter_buffer = 0;
    // Allocate a large VBO for GPU generation (max vertices heuristic)
    // 16MB is safe for 16^3 checkerboard chunk (max faces).
    size_t vbo_size = 16 * 1024 * 1024;
    if (mesh->vbo == 0) glGenBuffers(1, &mesh->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, vbo_size, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

#define EPSILON 0.002f  // Larger offset to prevent z-fighting gaps

static void add_vertex(Mesh *mesh, float x, float y, float z, float r, float g, float b, float nx, float ny, float nz, float ao, float u, float v) {
    if (mesh->vertex_count >= mesh->vertex_capacity) {
        mesh->vertex_capacity *= 2;
        mesh->vertices = realloc(mesh->vertices, sizeof(Vertex) * mesh->vertex_capacity);
    }
    // Add tiny offset to prevent seam gaps (z-fighting)
    x += (nx != 0.0f) ? nx * EPSILON : 0.0f;
    y += (ny != 0.0f) ? ny * EPSILON : 0.0f;
    z += (nz != 0.0f) ? nz * EPSILON : 0.0f;
    mesh->vertices[mesh->vertex_count++] = (Vertex){
        .x = x, .y = y, .z = z, .w = 1.0f,
        .r = r, .g = g, .b = b, .a = 1.0f,
        .nx = nx, .ny = ny, .nz = nz, .ao = ao,
        .u = u, .v = v, .p1 = 0, .p2 = 0
    };
}

typedef struct { float r, g, b; } Color;
static Color get_block_color(BlockType type) { return (Color){1.0f, 1.0f, 1.0f}; }

typedef struct { float u_off, v_off, w, h; } UVRect;
static UVRect get_uv_rect(BlockType type) {
    float s = 0.0625f;  // 1/16 = 0.0625
    float padding = 0.006f;  // ~2 pixel padding to prevent seams
    float uv_size = s - (padding * 2);
    switch (type) {
        case BLOCK_GRASS: return (UVRect){padding, padding, uv_size, uv_size};
        case BLOCK_DIRT:  return (UVRect){s + padding, padding, uv_size, uv_size};
        case BLOCK_STONE: return (UVRect){padding, s + padding, uv_size, uv_size};
        default:          return (UVRect){s + padding, s + padding, uv_size, uv_size};
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

void mesh_generate_gpu(Mesh *mesh, GLuint compute_program, GLuint voxel_tex, int chunk_x, int chunk_z) {
    glUseProgram(compute_program);

    glBindImageTexture(0, voxel_tex, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R8UI);

    // Bind vertex buffer as SSBO
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mesh->vbo);

    // Bind atomic counter
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, mesh->atomic_counter_buffer);
    GLuint zero = 0;
    glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);

    // Set uniforms
    glUniform2i(glGetUniformLocation(compute_program, "uChunkPos"), chunk_x, chunk_z);

    // Dispatch compute shader
    // CHUNK_SIZE = 16. Local size in shader is 4x4x4.
    // So we need 16/4 = 4 groups in each dimension.
    glDispatchCompute(4, 4, 4);

    // Memory barrier to ensure writing to VBO and atomic counter is finished
    glMemoryBarrier(0xFFFFFFFF); // GL_ALL_BARRIER_BITS

    // Update the indirect draw buffer
    mesh_update_draw_count(mesh);
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
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(11 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(12 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(mesh->vao);
}

void mesh_prepare_gpu(Mesh *mesh) {
    // Ensure VAO exists
    if (mesh->vao == 0) glGenVertexArrays(1, &mesh->vao);
    glBindVertexArray(mesh->vao);
    // VBO should already be allocated; just bind it
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    // Set attribute pointers (same layout as mesh_upload)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(11 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(12 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Allocate indirect draw buffer (count, instanceCount, first, baseInstance)
    glGenBuffers(1, &mesh->indirect_draw_buffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, mesh->indirect_draw_buffer);
    GLuint zero_cmd[4] = {0, 1, 0, 0};
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(zero_cmd), zero_cmd, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    // Allocate atomic counter buffer
    glGenBuffers(1, &mesh->atomic_counter_buffer);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, mesh->atomic_counter_buffer);
    GLuint zero = 0;
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), &zero, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
}

void mesh_update_draw_count(Mesh *mesh) {
    // Read the atomic counter (number of vertices generated)
    GLuint count = 0;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, mesh->atomic_counter_buffer);
    glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &count);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

    // Update indirect draw buffer: count, instanceCount=1, first=0, baseInstance=0
    GLuint cmd[4] = { count, 1, 0, 0 };
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, mesh->indirect_draw_buffer);
    glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(cmd), cmd);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void mesh_free(Mesh *mesh) {
    free(mesh->vertices);
    if (mesh->indirect_draw_buffer != 0) glDeleteBuffers(1, &mesh->indirect_draw_buffer);
    if (mesh->atomic_counter_buffer != 0) glDeleteBuffers(1, &mesh->atomic_counter_buffer);
    if (mesh->vao != 0) glDeleteVertexArrays(1, &mesh->vao);
}
