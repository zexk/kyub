#ifndef RENDERER_H
#define RENDERER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define R_MAX_FRAMES_IN_FLIGHT 2
#define R_MAX_PIPELINES 16
#define R_MAX_BUFFERS 256
#define R_MAX_TEXTURES 256
#define R_MAX_VAO 256
#define R_FENCE_TIMEOUT_NS 1000000000ULL
#define R_PUSH_CONSTANT_SIZE 256

/* Opaque handle types - uint64_t to accommodate both GL (32-bit) and Vulkan (64-bit) */
typedef uint64_t R_Program;
typedef uint64_t R_Buffer;
typedef uint64_t R_Texture;
typedef uint64_t R_VAO;

/* Special values */
#define R_INVALID_HANDLE UINT64_MAX

/* Capabilities for renderer_enable/disable */
typedef enum {
    R_CAP_DEPTH_TEST,
    R_CAP_CULL_FACE,
    R_CAP_BLEND,
    R_CAP_MULTISAMPLE,
    R_CAP_POLYGON_OFFSET_LINE,
    R_CAP_SCISSOR_TEST,
} R_Cap;

/* Depth functions */
typedef enum {
    R_FUNC_LESS,
    R_FUNC_LEQUAL,
    R_FUNC_ALWAYS,
} R_DepthFunc;

/* Blend factors */
typedef enum {
    R_BLEND_ZERO,
    R_BLEND_ONE,
    R_BLEND_SRC_ALPHA,
    R_BLEND_ONE_MINUS_SRC_ALPHA,
} R_BlendFactor;

/* Primitive types */
typedef enum {
    R_PRIM_TRIANGLES,
    R_PRIM_LINES,
    R_PRIM_TRIANGLE_FAN,
} R_Primitive;

/* Buffer targets */
typedef enum {
    R_BUF_ARRAY,
    R_BUF_ELEMENT,
    R_BUF_SHADER_STORAGE,
    R_BUF_ATOMIC_COUNTER,
    R_BUF_DRAW_INDIRECT,
} R_BufferTarget;

/* Buffer usage hints */
typedef enum {
    R_USAGE_STATIC,
    R_USAGE_DYNAMIC,
} R_Usage;

/* Texture targets */
typedef enum {
    R_TEX_2D,
    R_TEX_3D,
} R_TextureTarget;

/* Texture parameters */
typedef enum {
    R_TEX_WRAP_S,
    R_TEX_WRAP_T,
    R_TEX_WRAP_R,
    R_TEX_MIN_FILTER,
    R_TEX_MAG_FILTER,
} R_TexParam;

/* Texture parameter values */
typedef enum {
    R_TEX_REPEAT,
    R_TEX_CLAMP_TO_EDGE,
    R_TEX_NEAREST,
    R_TEX_LINEAR,
} R_TexValue;

/* Image access modes */
typedef enum {
    R_ACCESS_READ_ONLY,
} R_Access;

/* Memory barrier bits */
typedef enum {
    R_BARRIER_ALL,
} R_BarrierBits;

/* Attribute component types */
typedef enum {
    R_TYPE_FLOAT,
    R_TYPE_UBYTE,
} R_Type;

/* ============================================================================
 * Init / Lifecycle
 * ============================================================================ */

bool renderer_init(int width, int height);
void renderer_shutdown(void);
void renderer_swap(void);
void renderer_swap_interval(int interval);
void renderer_get_size(int *width, int *height);

/* ============================================================================
 * Global State
 * ============================================================================ */

void renderer_viewport(int x, int y, int width, int height);
void renderer_clear(float r, float g, float b, float a);
void renderer_clear_depth(void);
void renderer_enable(R_Cap cap);
void renderer_disable(R_Cap cap);
void renderer_depth_mask(bool write);
void renderer_depth_func(R_DepthFunc func);
void renderer_blend_func(R_BlendFactor src, R_BlendFactor dst);
void renderer_polygon_offset(float factor, float units);
void renderer_line_width(float width);
void renderer_push_attrib(void);
void renderer_pop_attrib(void);

/* ============================================================================
 * Shaders
 * ============================================================================ */

R_Program renderer_create_program(const char *vert_path, const char *frag_path);
R_Program renderer_create_compute(const char *comp_path);
void renderer_destroy_program(R_Program program);
void renderer_use_program(R_Program program);
int renderer_uniform_location(R_Program program, const char *name);
void renderer_uniform_mat4(int location, const float *matrix);
void renderer_uniform_vec3(int location, float x, float y, float z);
void renderer_uniform_vec2(int location, float x, float y);
void renderer_uniform_float(int location, float value);
void renderer_uniform_int(int location, int value);
void renderer_uniform_ivec2(int location, int x, int y);

/* ============================================================================
 * Buffers
 * ============================================================================ */

R_Buffer renderer_create_buffer(void);
void renderer_destroy_buffer(R_Buffer buffer);
void renderer_bind_buffer(R_BufferTarget target, R_Buffer buffer);
void renderer_buffer_data(R_BufferTarget target, size_t size, const void *data, R_Usage usage);
void renderer_buffer_sub_data(R_BufferTarget target, size_t offset, size_t size, const void *data);
void renderer_get_buffer_sub_data(R_BufferTarget target, size_t offset, size_t size, void *data);
void renderer_bind_buffer_base(R_BufferTarget target, int index, R_Buffer buffer);

/* ============================================================================
 * Vertex Arrays
 * ============================================================================ */

R_VAO renderer_create_vao(void);
void renderer_destroy_vao(R_VAO vao);
void renderer_bind_vao(R_VAO vao);
void renderer_attrib_pointer(int index, int size, R_Type type, bool normalized, int stride, int offset);
void renderer_enable_attrib(int index);

/* ============================================================================
 * Textures
 * ============================================================================ */

R_Texture renderer_create_texture(void);
void renderer_destroy_texture(R_Texture texture);
void renderer_bind_texture(R_TextureTarget target, R_Texture texture);
void renderer_active_texture(int unit);
void renderer_tex_image_2d(int width, int height, const void *data);
void renderer_tex_image_3d(int width, int height, int depth, const void *data);
void renderer_tex_sub_image_3d(int x, int y, int z, int width, int height, int depth, const void *data);
void renderer_tex_param(R_TextureTarget target, R_TexParam param, R_TexValue value);
void renderer_generate_mipmap(void);
void renderer_bind_image_texture(int unit, R_Texture texture, R_Access access);

/* ============================================================================
 * Drawing
 * ============================================================================ */

void renderer_draw_arrays(R_Primitive primitive, int first, int count);
void renderer_draw_elements(R_Primitive primitive, int count, int offset);
void renderer_draw_arrays_indirect(void);

/* ============================================================================
 * Compute
 * ============================================================================ */

void renderer_dispatch_compute(int groups_x, int groups_y, int groups_z);
void renderer_memory_barrier(R_BarrierBits bits);

#endif /* RENDERER_H */
