#include "mesh.h"
#include <stdlib.h>
#include <string.h>

void mesh_init(Mesh *mesh) {
    mesh->vertex_count = 0;
    mesh->vertex_capacity = 4096;
    mesh->vertices = malloc(sizeof(Vertex) * mesh->vertex_capacity);
    if (!mesh->vertices) return;
    mesh->vao = R_INVALID_HANDLE;
    mesh->vbo = R_INVALID_HANDLE;
    mesh->ebo = R_INVALID_HANDLE;
    mesh->indirect_draw_buffer = R_INVALID_HANDLE;
    mesh->atomic_counter_buffer = R_INVALID_HANDLE;
    size_t vbo_size = 16 * 1024 * 1024;
    mesh->vbo = renderer_create_buffer();
    renderer_bind_buffer(R_BUF_ARRAY, mesh->vbo);
    renderer_buffer_data(R_BUF_ARRAY, vbo_size, NULL, R_USAGE_DYNAMIC);
    renderer_bind_buffer(R_BUF_ARRAY, R_INVALID_HANDLE);
}

#define EPSILON 0.002f  // Larger offset to prevent z-fighting gaps

static void add_vertex(Mesh *mesh, float x, float y, float z, float r, float g, float b, float nx, float ny, float nz, float ao, float u, float v) {
    if (mesh->vertex_count >= mesh->vertex_capacity) {
        mesh->vertex_capacity *= 2;
        Vertex *new_vertices = realloc(mesh->vertices, sizeof(Vertex) * mesh->vertex_capacity);
        if (!new_vertices) return;
        mesh->vertices = new_vertices;
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

static float vertex_ao(bool side1, bool side2, bool corner) {
    if (side1 && side2) return 0.5f;
    int count = (side1 ? 1 : 0) + (side2 ? 1 : 0) + (corner ? 1 : 0);
    if (count == 0) return 1.0f;
    if (count == 1) return 0.85f;
    if (count == 2) return 0.7f;
    return 0.5f;
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

    float ao[4];
    if (face == 0) { // Front (+Z)
        ao[0] = vertex_ao(!is_transparent(chunk, x, y-1, z+1), !is_transparent(chunk, x-1, y, z+1), !is_transparent(chunk, x-1, y-1, z+1));
        ao[1] = vertex_ao(!is_transparent(chunk, x+1, y-1, z+1), !is_transparent(chunk, x+2, y, z+1), !is_transparent(chunk, x+2, y-1, z+1));
        ao[2] = vertex_ao(!is_transparent(chunk, x+1, y+2, z+1), !is_transparent(chunk, x+2, y+1, z+1), !is_transparent(chunk, x+2, y+2, z+1));
        ao[3] = vertex_ao(!is_transparent(chunk, x, y+2, z+1), !is_transparent(chunk, x-1, y+1, z+1), !is_transparent(chunk, x-1, y+2, z+1));
    } else if (face == 1) { // Back (-Z)
        ao[0] = vertex_ao(!is_transparent(chunk, x+1, y-1, z), !is_transparent(chunk, x+2, y, z), !is_transparent(chunk, x+2, y-1, z));
        ao[1] = vertex_ao(!is_transparent(chunk, x, y-1, z), !is_transparent(chunk, x-1, y, z), !is_transparent(chunk, x-1, y-1, z));
        ao[2] = vertex_ao(!is_transparent(chunk, x, y+2, z), !is_transparent(chunk, x-1, y+1, z), !is_transparent(chunk, x-1, y+2, z));
        ao[3] = vertex_ao(!is_transparent(chunk, x+1, y+2, z), !is_transparent(chunk, x+2, y+1, z), !is_transparent(chunk, x+2, y+2, z));
    } else if (face == 2) { // Left (-X)
        ao[0] = vertex_ao(!is_transparent(chunk, x, y-1, z), !is_transparent(chunk, x, y, z-1), !is_transparent(chunk, x, y-1, z-1));
        ao[1] = vertex_ao(!is_transparent(chunk, x, y-1, z+1), !is_transparent(chunk, x, y, z+2), !is_transparent(chunk, x, y-1, z+2));
        ao[2] = vertex_ao(!is_transparent(chunk, x, y+2, z+1), !is_transparent(chunk, x, y+1, z+2), !is_transparent(chunk, x, y+2, z+2));
        ao[3] = vertex_ao(!is_transparent(chunk, x, y+2, z), !is_transparent(chunk, x, y+1, z-1), !is_transparent(chunk, x, y+2, z-1));
    } else if (face == 3) { // Right (+X)
        ao[0] = vertex_ao(!is_transparent(chunk, x+1, y-1, z+1), !is_transparent(chunk, x+1, y, z+2), !is_transparent(chunk, x+1, y-1, z+2));
        ao[1] = vertex_ao(!is_transparent(chunk, x+1, y-1, z), !is_transparent(chunk, x+1, y, z-1), !is_transparent(chunk, x+1, y-1, z-1));
        ao[2] = vertex_ao(!is_transparent(chunk, x+1, y+2, z), !is_transparent(chunk, x+1, y+1, z-1), !is_transparent(chunk, x+1, y+2, z-1));
        ao[3] = vertex_ao(!is_transparent(chunk, x+1, y+2, z+1), !is_transparent(chunk, x+1, y+1, z+2), !is_transparent(chunk, x+1, y+2, z+2));
    } else if (face == 4) { // Top (+Y)
        ao[0] = vertex_ao(!is_transparent(chunk, x-1, y+1, z+1), !is_transparent(chunk, x, y+1, z+2), !is_transparent(chunk, x-1, y+1, z+2));
        ao[1] = vertex_ao(!is_transparent(chunk, x+2, y+1, z+1), !is_transparent(chunk, x+1, y+1, z+2), !is_transparent(chunk, x+2, y+1, z+2));
        ao[2] = vertex_ao(!is_transparent(chunk, x+2, y+1, z), !is_transparent(chunk, x+1, y+1, z-1), !is_transparent(chunk, x+2, y+1, z-1));
        ao[3] = vertex_ao(!is_transparent(chunk, x-1, y+1, z), !is_transparent(chunk, x, y+1, z-1), !is_transparent(chunk, x-1, y+1, z-1));
    } else { // Bottom (-Y)
        ao[0] = vertex_ao(!is_transparent(chunk, x-1, y, z), !is_transparent(chunk, x, y, z-1), !is_transparent(chunk, x-1, y, z-1));
        ao[1] = vertex_ao(!is_transparent(chunk, x+2, y, z), !is_transparent(chunk, x+1, y, z-1), !is_transparent(chunk, x+2, y, z-1));
        ao[2] = vertex_ao(!is_transparent(chunk, x+2, y, z+1), !is_transparent(chunk, x+1, y, z+2), !is_transparent(chunk, x+2, y, z+2));
        ao[3] = vertex_ao(!is_transparent(chunk, x-1, y, z+1), !is_transparent(chunk, x, y, z+2), !is_transparent(chunk, x-1, y, z+2));
    }

    add_vertex(mesh, p[0][0], p[0][1], p[0][2], c.r, c.g, c.b, nx, ny, nz, ao[0], uv.u_off, uv.v_off);
    add_vertex(mesh, p[1][0], p[1][1], p[1][2], c.r, c.g, c.b, nx, ny, nz, ao[1], uv.u_off + uv.w, uv.v_off);
    add_vertex(mesh, p[2][0], p[2][1], p[2][2], c.r, c.g, c.b, nx, ny, nz, ao[2], uv.u_off + uv.w, uv.v_off + uv.h);
    add_vertex(mesh, p[0][0], p[0][1], p[0][2], c.r, c.g, c.b, nx, ny, nz, ao[0], uv.u_off, uv.v_off);
    add_vertex(mesh, p[2][0], p[2][1], p[2][2], c.r, c.g, c.b, nx, ny, nz, ao[2], uv.u_off + uv.w, uv.v_off + uv.h);
    add_vertex(mesh, p[3][0], p[3][1], p[3][2], c.r, c.g, c.b, nx, ny, nz, ao[3], uv.u_off, uv.v_off + uv.h);
}

void mesh_generate_gpu(Mesh *mesh, R_Program compute_program, R_Texture voxel_tex, int chunk_x, int chunk_z) {
    renderer_use_program(compute_program);

    renderer_bind_image_texture(0, voxel_tex, R_ACCESS_READ_ONLY);

    // Bind vertex buffer as SSBO
    renderer_bind_buffer_base(R_BUF_SHADER_STORAGE, 1, mesh->vbo);

    // Bind atomic counter
    renderer_bind_buffer_base(R_BUF_ATOMIC_COUNTER, 0, mesh->atomic_counter_buffer);
    uint32_t zero = 0;
    renderer_buffer_sub_data(R_BUF_ATOMIC_COUNTER, 0, sizeof(uint32_t), &zero);

    // Set uniforms
    int loc = renderer_uniform_location(compute_program, "uChunkPos");
    renderer_uniform_ivec2(loc, chunk_x, chunk_z);

    // Dispatch compute shader
    // CHUNK_SIZE = 16. Local size in shader is 4x4x4.
    // So we need 16/4 = 4 groups in each dimension.
    renderer_dispatch_compute(4, 4, 4);

    // Memory barrier to ensure writing to VBO and atomic counter is finished
    renderer_memory_barrier(R_BARRIER_ALL);

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
    if (mesh->vao == R_INVALID_HANDLE) mesh->vao = renderer_create_vao();
    if (mesh->vbo == R_INVALID_HANDLE) mesh->vbo = renderer_create_buffer();
    renderer_bind_vao(mesh->vao);
    renderer_bind_buffer(R_BUF_ARRAY, mesh->vbo);
    /* Upload vertex data without recreating buffer (avoid GPU stall) */
    renderer_buffer_sub_data(R_BUF_ARRAY, 0, mesh->vertex_count * sizeof(Vertex), mesh->vertices);
    renderer_attrib_pointer(0, 3, R_TYPE_FLOAT, false, sizeof(Vertex), 0);
    renderer_enable_attrib(0);
    renderer_attrib_pointer(1, 3, R_TYPE_FLOAT, false, sizeof(Vertex), 4 * sizeof(float));
    renderer_enable_attrib(1);
    renderer_attrib_pointer(2, 3, R_TYPE_FLOAT, false, sizeof(Vertex), 8 * sizeof(float));
    renderer_enable_attrib(2);
    renderer_attrib_pointer(3, 1, R_TYPE_FLOAT, false, sizeof(Vertex), 11 * sizeof(float));
    renderer_enable_attrib(3);
    renderer_attrib_pointer(4, 2, R_TYPE_FLOAT, false, sizeof(Vertex), 12 * sizeof(float));
    renderer_enable_attrib(4);
    renderer_bind_buffer(R_BUF_ARRAY, R_INVALID_HANDLE);
    renderer_bind_vao(R_INVALID_HANDLE); /* Unbind VAO to avoid stale state */
}

void mesh_prepare_gpu(Mesh *mesh) {
    // Ensure VAO exists
    if (mesh->vao == R_INVALID_HANDLE) mesh->vao = renderer_create_vao();
    renderer_bind_vao(mesh->vao);
    // VBO should already be allocated; just bind it
    renderer_bind_buffer(R_BUF_ARRAY, mesh->vbo);
    // Set attribute pointers (same layout as mesh_upload)
    renderer_attrib_pointer(0, 3, R_TYPE_FLOAT, false, sizeof(Vertex), 0);
    renderer_enable_attrib(0);
    renderer_attrib_pointer(1, 3, R_TYPE_FLOAT, false, sizeof(Vertex), 4 * sizeof(float));
    renderer_enable_attrib(1);
    renderer_attrib_pointer(2, 3, R_TYPE_FLOAT, false, sizeof(Vertex), 8 * sizeof(float));
    renderer_enable_attrib(2);
    renderer_attrib_pointer(3, 1, R_TYPE_FLOAT, false, sizeof(Vertex), 11 * sizeof(float));
    renderer_enable_attrib(3);
    renderer_attrib_pointer(4, 2, R_TYPE_FLOAT, false, sizeof(Vertex), 12 * sizeof(float));
    renderer_enable_attrib(4);
    renderer_bind_buffer(R_BUF_ARRAY, R_INVALID_HANDLE);
    renderer_bind_vao(R_INVALID_HANDLE);

    // Allocate indirect draw buffer (count, instanceCount, first, baseInstance)
    mesh->indirect_draw_buffer = renderer_create_buffer();
    renderer_bind_buffer(R_BUF_DRAW_INDIRECT, mesh->indirect_draw_buffer);
    uint32_t zero_cmd[4] = {0, 1, 0, 0};
    renderer_buffer_data(R_BUF_DRAW_INDIRECT, sizeof(zero_cmd), zero_cmd, R_USAGE_DYNAMIC);
    renderer_bind_buffer(R_BUF_DRAW_INDIRECT, R_INVALID_HANDLE);

    // Allocate atomic counter buffer
    mesh->atomic_counter_buffer = renderer_create_buffer();
    renderer_bind_buffer(R_BUF_ATOMIC_COUNTER, mesh->atomic_counter_buffer);
    uint32_t zero = 0;
    renderer_buffer_data(R_BUF_ATOMIC_COUNTER, sizeof(uint32_t), &zero, R_USAGE_DYNAMIC);
    renderer_bind_buffer(R_BUF_ATOMIC_COUNTER, R_INVALID_HANDLE);
}

void mesh_update_draw_count(Mesh *mesh) {
    // Read the atomic counter (number of vertices generated)
    uint32_t count = 0;
    renderer_bind_buffer(R_BUF_ATOMIC_COUNTER, mesh->atomic_counter_buffer);
    renderer_get_buffer_sub_data(R_BUF_ATOMIC_COUNTER, 0, sizeof(uint32_t), &count);
    renderer_bind_buffer(R_BUF_ATOMIC_COUNTER, R_INVALID_HANDLE);

    // Update indirect draw buffer: count, instanceCount=1, first=0, baseInstance=0
    uint32_t cmd[4] = {count, 1, 0, 0};
    renderer_bind_buffer(R_BUF_DRAW_INDIRECT, mesh->indirect_draw_buffer);
    renderer_buffer_sub_data(R_BUF_DRAW_INDIRECT, 0, sizeof(cmd), cmd);
    renderer_bind_buffer(R_BUF_DRAW_INDIRECT, R_INVALID_HANDLE);
}

void mesh_free(Mesh *mesh) {
    free(mesh->vertices);
    mesh->vertices = NULL;
    if (mesh->indirect_draw_buffer != R_INVALID_HANDLE) {
        renderer_destroy_buffer(mesh->indirect_draw_buffer);
        mesh->indirect_draw_buffer = R_INVALID_HANDLE;
    }
    if (mesh->atomic_counter_buffer != R_INVALID_HANDLE) {
        renderer_destroy_buffer(mesh->atomic_counter_buffer);
        mesh->atomic_counter_buffer = R_INVALID_HANDLE;
    }
    if (mesh->vao != R_INVALID_HANDLE) {
        renderer_destroy_vao(mesh->vao);
        mesh->vao = R_INVALID_HANDLE;
    }
    if (mesh->vbo != R_INVALID_HANDLE) {
        renderer_destroy_buffer(mesh->vbo);
        mesh->vbo = R_INVALID_HANDLE;
    }
}
