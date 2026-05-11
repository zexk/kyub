#include "renderer/renderer.h"
#include "platform/platform.h"
#include "logger.h"
#include <glad/gl.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

/* ============================================================================
 * Internal state
 * ============================================================================ */

typedef struct {
    GLuint program;
    GLuint vert;
    GLuint frag;
} GLProgram;

typedef struct {
    GLuint buffer;
    uint64_t size;
} GLBuffer;

typedef struct {
    GLuint texture;
    uint32_t width, height, depth;
} GLTexture;

static struct {
    int width, height;
    Display *display;
    GLXContext context;
    Window window;
    GLuint bound_program;
    GLuint bound_vao;
    GLuint bound_vbo;
    GLuint bound_ibo;
    int active_texture_unit;
    GLuint bound_textures[16];
    float clear_color[4];
    bool clear_depth;
} g_gl = {0};

static GLProgram g_programs[16];
static int g_program_count = 0;

static GLBuffer g_buffers[256];
static int g_buffer_count = 0;

static GLTexture g_textures[256];
static int g_texture_count = 0;

typedef struct {
    GLuint vao;
    int stride;
} GLVAO;

static GLVAO g_vaos[256];
static int g_vao_count = 0;

typedef struct {
    char name[64];
    GLint location;
} GLUniformMapping;

static GLUniformMapping g_uniforms[32];
static int g_uniform_count = 0;

/* ============================================================================
 * Helpers
 * ============================================================================ */

static char* load_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        LOG_ERROR(CAT_RENDERER, "Failed to open file: %s", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (n != (size_t)size) {
        free(buf);
        return NULL;
    }
    buf[size] = '\0';
    if (out_size) *out_size = (size_t)size;
    return buf;
}

static bool compile_shader(GLuint shader, const char *source, const char *path) {
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        LOG_ERROR(CAT_RENDERER, "Shader compile error (%s): %s", path, log);
        return false;
    }
    return true;
}

static bool link_program(GLuint program) {
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        LOG_ERROR(CAT_RENDERER, "Program link error: %s", log);
        return false;
    }
    return true;
}

static GLenum primitive_to_gl(R_Primitive prim) {
    switch (prim) {
        case R_PRIM_TRIANGLES:     return GL_TRIANGLES;
        case R_PRIM_LINES:         return GL_LINES;
        case R_PRIM_TRIANGLE_FAN:  return GL_TRIANGLE_FAN;
        default:                   return GL_TRIANGLES;
    }
}

static GLenum cap_to_gl(R_Cap cap) {
    switch (cap) {
        case R_CAP_DEPTH_TEST:          return GL_DEPTH_TEST;
        case R_CAP_CULL_FACE:           return GL_CULL_FACE;
        case R_CAP_BLEND:               return GL_BLEND;
        case R_CAP_MULTISAMPLE:         return GL_MULTISAMPLE;
        case R_CAP_POLYGON_OFFSET_LINE: return GL_POLYGON_OFFSET_LINE;
        case R_CAP_SCISSOR_TEST:        return GL_SCISSOR_TEST;
        default:                        return 0;
    }
}

static GLenum depth_func_to_gl(R_DepthFunc func) {
    switch (func) {
        case R_FUNC_LESS:    return GL_LESS;
        case R_FUNC_LEQUAL:  return GL_LEQUAL;
        case R_FUNC_ALWAYS:  return GL_ALWAYS;
        default:             return GL_LESS;
    }
}

static GLenum blend_factor_to_gl(R_BlendFactor factor) {
    switch (factor) {
        case R_BLEND_ZERO:                return GL_ZERO;
        case R_BLEND_ONE:                 return GL_ONE;
        case R_BLEND_SRC_ALPHA:           return GL_SRC_ALPHA;
        case R_BLEND_ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
        default:                          return GL_ONE;
    }
}

static GLenum type_to_gl(R_Type type) {
    switch (type) {
        case R_TYPE_FLOAT: return GL_FLOAT;
        case R_TYPE_UBYTE: return GL_UNSIGNED_BYTE;
        default:           return GL_FLOAT;
    }
}

static GLenum target_to_gl(R_TextureTarget target) {
    switch (target) {
        case R_TEX_2D: return GL_TEXTURE_2D;
        case R_TEX_3D: return GL_TEXTURE_3D;
        default:       return GL_TEXTURE_2D;
    }
}

static GLenum usage_to_gl(R_Usage usage) {
    switch (usage) {
        case R_USAGE_STATIC:  return GL_STATIC_DRAW;
        case R_USAGE_DYNAMIC: return GL_DYNAMIC_DRAW;
        default:              return GL_STATIC_DRAW;
    }
}

static GLenum tex_param_to_gl(R_TexParam param) {
    switch (param) {
        case R_TEX_WRAP_S:     return GL_TEXTURE_WRAP_S;
        case R_TEX_WRAP_T:     return GL_TEXTURE_WRAP_T;
        case R_TEX_WRAP_R:     return GL_TEXTURE_WRAP_R;
        case R_TEX_MIN_FILTER: return GL_TEXTURE_MIN_FILTER;
        case R_TEX_MAG_FILTER: return GL_TEXTURE_MAG_FILTER;
        default:               return 0;
    }
}

static GLint tex_value_to_gl(R_TexValue value) {
    switch (value) {
        case R_TEX_REPEAT:         return GL_REPEAT;
        case R_TEX_CLAMP_TO_EDGE:  return GL_CLAMP_TO_EDGE;
        case R_TEX_NEAREST:        return GL_NEAREST;
        case R_TEX_LINEAR:         return GL_LINEAR;
        default:                   return GL_NEAREST;
    }
}

/* ============================================================================
 * Init / Lifecycle
 * ============================================================================ */

extern Display *g_x11_display;
extern Window g_x11_window;

bool renderer_init(int width, int height) {
    g_gl.width = width;
    g_gl.height = height;
    g_gl.display = g_x11_display;
    g_gl.window = g_x11_window;

    /* Initialize GLAD */
    int version = gladLoadGL((GLADloadfunc)glXGetProcAddressARB);
    if (version == 0) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return false;
    }

    /* Verify OpenGL 4.5 support */
    if (!GLAD_GL_VERSION_4_5) {
        fprintf(stderr, "OpenGL 4.5 not supported (got %d.%d)\n",
                GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
        return false;
    }

    /* Get GLX context */
    g_gl.context = glXGetCurrentContext();
    if (!g_gl.context) {
        fprintf(stderr, "No active GLX context\n");
        return false;
    }

    /* Default state */
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.0f);
    glPolygonOffset(0.0f, 0.0f);

    /* Default clear color */
    g_gl.clear_color[0] = 0.0f;
    g_gl.clear_color[1] = 0.0f;
    g_gl.clear_color[2] = 0.0f;
    g_gl.clear_color[3] = 1.0f;

    printf("OpenGL renderer initialized (%d.%d)\n",
           GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
    return true;
}

void renderer_shutdown(void) {
    /* Delete programs */
    for (int i = 0; i < g_program_count; i++) {
        if (g_programs[i].program) {
            glDeleteProgram(g_programs[i].program);
        }
        if (g_programs[i].vert) {
            glDeleteShader(g_programs[i].vert);
        }
        if (g_programs[i].frag) {
            glDeleteShader(g_programs[i].frag);
        }
    }
    g_program_count = 0;

    /* Delete buffers */
    for (int i = 0; i < g_buffer_count; i++) {
        if (g_buffers[i].buffer) {
            glDeleteBuffers(1, &g_buffers[i].buffer);
        }
    }
    g_buffer_count = 0;

    /* Delete textures */
    for (int i = 0; i < g_texture_count; i++) {
        if (g_textures[i].texture) {
            glDeleteTextures(1, &g_textures[i].texture);
        }
    }
    g_texture_count = 0;

    /* Delete VAOs */
    for (int i = 0; i < g_vao_count; i++) {
        if (g_vaos[i].vao) {
            glDeleteVertexArrays(1, &g_vaos[i].vao);
        }
    }
    g_vao_count = 0;

    g_gl.bound_program = 0;
    g_gl.bound_vao = 0;
    g_gl.bound_vbo = 0;
    g_gl.bound_ibo = 0;
}

void renderer_swap(void) {
    glXSwapBuffers(g_gl.display, g_gl.window);
}

void renderer_swap_interval(int interval) {
    (void)interval;
    /* glXSwapIntervalEXT not available in all drivers; ignore for now */
}

void renderer_get_size(int *width, int *height) {
    *width = g_gl.width;
    *height = g_gl.height;
}

/* ============================================================================
 * Frame control
 * ============================================================================ */

void renderer_viewport(int x, int y, int width, int height) {
    (void)x;
    (void)y;
    if (g_gl.width != width || g_gl.height != height) {
        g_gl.width = width;
        g_gl.height = height;
    }
    glViewport(0, 0, width, height);
}

void renderer_clear(float r, float g, float b, float a) {
    g_gl.clear_color[0] = r;
    g_gl.clear_color[1] = g;
    g_gl.clear_color[2] = b;
    g_gl.clear_color[3] = a;
    g_gl.clear_depth = true;

    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void renderer_clear_depth(void) {
    glClear(GL_DEPTH_BUFFER_BIT);
}

/* ============================================================================
 * State
 * ============================================================================ */

void renderer_enable(R_Cap cap) {
    GLenum glcap = cap_to_gl(cap);
    if (glcap) glEnable(glcap);
}

void renderer_disable(R_Cap cap) {
    GLenum glcap = cap_to_gl(cap);
    if (glcap) glDisable(glcap);
}

void renderer_depth_mask(bool write) {
    glDepthMask(write ? GL_TRUE : GL_FALSE);
}

void renderer_depth_func(R_DepthFunc func) {
    glDepthFunc(depth_func_to_gl(func));
}

void renderer_blend_func(R_BlendFactor src, R_BlendFactor dst) {
    glBlendFunc(blend_factor_to_gl(src), blend_factor_to_gl(dst));
}

void renderer_polygon_offset(float factor, float units) {
    glPolygonOffset(factor, units);
}

void renderer_line_width(float width) {
    glLineWidth(width);
}

void renderer_push_attrib(void) {
    /* No-op for GL — state is explicit */
}

void renderer_pop_attrib(void) {
    /* No-op for GL — state is explicit */
}

/* ============================================================================
 * Shaders
 * ============================================================================ */

R_Program renderer_create_program(const char *vert_path, const char *frag_path) {
    char vert_gl[256], frag_gl[256];
    snprintf(vert_gl, sizeof(vert_gl), "%s.gl.vert", vert_path);
    snprintf(frag_gl, sizeof(frag_gl), "%s.gl.frag", frag_path);

    size_t vert_size, frag_size;
    char *vert_src = load_file(vert_gl, &vert_size);
    char *frag_src = load_file(frag_gl, &frag_size);

    if (!vert_src || !frag_src) {
        free(vert_src);
        free(frag_src);
        return R_INVALID_HANDLE;
    }

    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);

    if (!compile_shader(vert, vert_src, vert_gl) ||
        !compile_shader(frag, frag_src, frag_gl)) {
        glDeleteShader(vert);
        glDeleteShader(frag);
        free(vert_src);
        free(frag_src);
        return R_INVALID_HANDLE;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);

    if (!link_program(program)) {
        glDeleteProgram(program);
        glDeleteShader(vert);
        glDeleteShader(frag);
        free(vert_src);
        free(frag_src);
        return R_INVALID_HANDLE;
    }

    glDetachShader(program, vert);
    glDetachShader(program, frag);

    free(vert_src);
    free(frag_src);

    if (g_program_count >= R_MAX_PIPELINES) {
        glDeleteProgram(program);
        glDeleteShader(vert);
        glDeleteShader(frag);
        return R_INVALID_HANDLE;
    }

    int idx = g_program_count++;
    g_programs[idx] = (GLProgram){program, vert, frag};
    return (R_Program)idx;
}

R_Program renderer_create_compute(const char *comp_path) {
    (void)comp_path;
    return R_INVALID_HANDLE;
}

void renderer_destroy_program(R_Program program) {
    if (program >= (R_Program)g_program_count) return;
    GLProgram *p = &g_programs[program];
    if (p->program) {
        glDeleteProgram(p->program);
        p->program = 0;
    }
    if (p->vert) {
        glDeleteShader(p->vert);
        p->vert = 0;
    }
    if (p->frag) {
        glDeleteShader(p->frag);
        p->frag = 0;
    }
}

void renderer_use_program(R_Program program) {
    if (program >= (R_Program)g_program_count) return;
    GLuint p = g_programs[program].program;
    g_gl.bound_program = p;
    glUseProgram(p);
    g_uniform_count = 0;
}

int renderer_uniform_location(R_Program program, const char *name) {
    (void)program;
    if (!g_gl.bound_program) return -1;

    for (int i = 0; i < g_uniform_count; i++) {
        if (strcmp(g_uniforms[i].name, name) == 0) {
            return g_uniforms[i].location;
        }
    }
    if (g_uniform_count >= 32) return -1;

    GLint loc = glGetUniformLocation(g_gl.bound_program, name);
    if (loc < 0) return -1;

    strncpy(g_uniforms[g_uniform_count].name, name, 63);
    g_uniforms[g_uniform_count].name[63] = '\0';
    g_uniforms[g_uniform_count].location = loc;
    return g_uniform_count++;
}

void renderer_uniform_mat4(int location, const float *matrix) {
    if (location < 0 || location >= g_uniform_count || !matrix) return;
    GLint loc = g_uniforms[location].location;
    glProgramUniformMatrix4fv(g_gl.bound_program, loc, 1, GL_FALSE, matrix);
}

void renderer_uniform_vec3(int location, float x, float y, float z) {
    if (location < 0 || location >= g_uniform_count) return;
    GLint loc = g_uniforms[location].location;
    glProgramUniform3f(g_gl.bound_program, loc, x, y, z);
}

void renderer_uniform_vec2(int location, float x, float y) {
    if (location < 0 || location >= g_uniform_count) return;
    GLint loc = g_uniforms[location].location;
    glProgramUniform2f(g_gl.bound_program, loc, x, y);
}

void renderer_uniform_float(int location, float value) {
    if (location < 0 || location >= g_uniform_count) return;
    GLint loc = g_uniforms[location].location;
    glProgramUniform1f(g_gl.bound_program, loc, value);
}

void renderer_uniform_int(int location, int value) {
    if (location < 0 || location >= g_uniform_count) return;
    GLint loc = g_uniforms[location].location;
    glProgramUniform1i(g_gl.bound_program, loc, value);
}

void renderer_uniform_ivec2(int location, int x, int y) {
    if (location < 0 || location >= g_uniform_count) return;
    GLint loc = g_uniforms[location].location;
    glProgramUniform2i(g_gl.bound_program, loc, x, y);
}

/* ============================================================================
 * Buffers
 * ============================================================================ */

R_Buffer renderer_create_buffer(void) {
    if (g_buffer_count >= R_MAX_BUFFERS) return R_INVALID_HANDLE;
    GLuint buffer;
    glCreateBuffers(1, &buffer);
    int idx = g_buffer_count++;
    g_buffers[idx] = (GLBuffer){buffer, 0};
    return (R_Buffer)idx;
}

void renderer_destroy_buffer(R_Buffer buffer) {
    if (buffer >= (R_Buffer)g_buffer_count) return;
    if (g_buffers[buffer].buffer) {
        glDeleteBuffers(1, &g_buffers[buffer].buffer);
        g_buffers[buffer].buffer = 0;
    }
}

void renderer_bind_buffer(R_BufferTarget target, R_Buffer buffer) {
    if (buffer == R_INVALID_HANDLE) {
        if (target == R_BUF_ELEMENT) {
            g_gl.bound_ibo = 0;
        } else {
            g_gl.bound_vbo = 0;
        }
        return;
    }
    if (buffer >= (R_Buffer)g_buffer_count) return;
    GLuint buf = g_buffers[buffer].buffer;
    if (target == R_BUF_ELEMENT) {
        g_gl.bound_ibo = buf;
    } else {
        g_gl.bound_vbo = buf;
    }
    /* For VAO-bound rendering, binding happens in draw calls */
}

void renderer_buffer_data(R_BufferTarget target, size_t size, const void *data, R_Usage usage) {
    R_Buffer handle;
    if (target == R_BUF_ELEMENT) {
        handle = g_gl.bound_ibo;
    } else {
        handle = g_gl.bound_vbo;
    }
    /* We need to find the handle index from the GL buffer name */
    int idx = -1;
    for (int i = 0; i < g_buffer_count; i++) {
        if (g_buffers[i].buffer == (GLuint)handle) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return;

    GLenum gl_usage = usage_to_gl(usage);
    glNamedBufferData(g_buffers[idx].buffer, (GLsizeiptr)size, data, gl_usage);
    g_buffers[idx].size = size;
}

void renderer_buffer_sub_data(R_BufferTarget target, size_t offset, size_t size, const void *data) {
    R_Buffer handle;
    if (target == R_BUF_ELEMENT) {
        handle = g_gl.bound_ibo;
    } else {
        handle = g_gl.bound_vbo;
    }
    int idx = -1;
    for (int i = 0; i < g_buffer_count; i++) {
        if (g_buffers[i].buffer == (GLuint)handle) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return;

    glNamedBufferSubData(g_buffers[idx].buffer, (GLintptr)offset, (GLsizeiptr)size, data);
}

void renderer_get_buffer_sub_data(R_BufferTarget target, size_t offset, size_t size, void *data) {
    (void)target;
    (void)offset;
    (void)size;
    (void)data;
}

void renderer_bind_buffer_base(R_BufferTarget target, int index, R_Buffer buffer) {
    (void)target;
    (void)index;
    (void)buffer;
}

/* ============================================================================
 * Vertex Arrays
 * ============================================================================ */

R_VAO renderer_create_vao(void) {
    if (g_vao_count >= R_MAX_VAO) return R_INVALID_HANDLE;
    GLuint vao;
    glCreateVertexArrays(1, &vao);
    int idx = g_vao_count++;
    g_vaos[idx] = (GLVAO){vao, 0};
    return (R_VAO)idx;
}

void renderer_destroy_vao(R_VAO vao) {
    if (vao >= (R_VAO)g_vao_count) return;
    if (g_vaos[vao].vao) {
        glDeleteVertexArrays(1, &g_vaos[vao].vao);
        g_vaos[vao].vao = 0;
    }
}

void renderer_bind_vao(R_VAO vao) {
    if (vao == R_INVALID_HANDLE) {
        g_gl.bound_vao = 0;
        glBindVertexArray(0);
        return;
    }
    if (vao >= (R_VAO)g_vao_count) return;
    GLuint va = g_vaos[vao].vao;
    g_gl.bound_vao = va;
    glBindVertexArray(va);
}

void renderer_attrib_pointer(int index, int size, R_Type type, bool normalized, int stride, int offset) {
    if (!g_gl.bound_vao) return;
    GLenum gl_type = type_to_gl(type);
    GLboolean norm = normalized ? GL_TRUE : GL_FALSE;
    /* Find VAO index from bound VAO handle */
    int vao_idx = -1;
    for (int i = 0; i < g_vao_count; i++) {
        if (g_vaos[i].vao == g_gl.bound_vao) {
            vao_idx = i;
            break;
        }
    }
    if (vao_idx >= 0 && stride > 0) {
        g_vaos[vao_idx].stride = stride;
    }
    glVertexArrayAttribFormat(g_gl.bound_vao, (GLuint)index, size, gl_type, norm, (GLuint)offset);
    glVertexArrayAttribBinding(g_gl.bound_vao, (GLuint)index, 0);
}

void renderer_enable_attrib(int index) {
    if (!g_gl.bound_vao) return;
    glEnableVertexArrayAttrib(g_gl.bound_vao, (GLuint)index);
}

/* ============================================================================
 * Textures
 * ============================================================================ */

R_Texture renderer_create_texture(void) {
    if (g_texture_count >= R_MAX_TEXTURES) return R_INVALID_HANDLE;
    GLuint tex;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    int idx = g_texture_count++;
    g_textures[idx] = (GLTexture){tex, 0, 0, 0};
    return (R_Texture)idx;
}

void renderer_destroy_texture(R_Texture texture) {
    if (texture >= (R_Texture)g_texture_count) return;
    if (g_textures[texture].texture) {
        glDeleteTextures(1, &g_textures[texture].texture);
        g_textures[texture].texture = 0;
    }
}

void renderer_bind_texture(R_TextureTarget target, R_Texture texture) {
    if (texture >= (R_Texture)g_texture_count) return;
    GLuint tex = (texture == R_INVALID_HANDLE) ? 0 : g_textures[texture].texture;
    GLenum gl_target = target_to_gl(target);
    g_gl.bound_textures[g_gl.active_texture_unit] = tex;
    if (tex) {
        glBindTexture(gl_target, tex);
    }
}

void renderer_active_texture(int unit) {
    g_gl.active_texture_unit = unit;
    glActiveTexture(GL_TEXTURE0 + unit);
}

void renderer_tex_image_2d(int width, int height, const void *data) {
    R_Texture tex = g_gl.bound_textures[g_gl.active_texture_unit];
    if (tex == 0) return;
    int idx = -1;
    for (int i = 0; i < g_texture_count; i++) {
        if (g_textures[i].texture == tex) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return;

    if (g_textures[idx].width != (uint32_t)width ||
        g_textures[idx].height != (uint32_t)height) {
        glTextureStorage2D(tex, 1, GL_RGBA8, width, height);
        g_textures[idx].width = width;
        g_textures[idx].height = height;
    }

    if (data) {
        glTextureSubImage2D(tex, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    }
}

void renderer_tex_image_3d(int width, int height, int depth, const void *data) {
    R_Texture tex = g_gl.bound_textures[g_gl.active_texture_unit];
    if (tex == 0) return;
    int idx = -1;
    for (int i = 0; i < g_texture_count; i++) {
        if (g_textures[i].texture == tex) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return;

    if (g_textures[idx].width != (uint32_t)width ||
        g_textures[idx].height != (uint32_t)height ||
        g_textures[idx].depth != (uint32_t)depth) {
        GLuint gl_tex = (GLuint)tex;
        glDeleteTextures(1, &gl_tex);
        glCreateTextures(GL_TEXTURE_3D, 1, &gl_tex);
        g_textures[idx].texture = (R_Texture)gl_tex;
        glTextureStorage3D(gl_tex, 1, GL_R8, width, height, depth);
        g_textures[idx].width = width;
        g_textures[idx].height = height;
        g_textures[idx].depth = depth;
    }

    if (data) {
        glTextureSubImage3D(tex, 0, 0, 0, 0, width, height, depth, GL_RED_INTEGER, GL_UNSIGNED_BYTE, data);
    }
}

void renderer_tex_sub_image_3d(int x, int y, int z, int width, int height, int depth, const void *data) {
    (void)x;
    (void)y;
    (void)z;
    (void)width;
    (void)height;
    (void)depth;
    (void)data;
}

void renderer_tex_param(R_TextureTarget target, R_TexParam param, R_TexValue value) {
    R_Texture tex = g_gl.bound_textures[g_gl.active_texture_unit];
    if (tex == 0) return;
    GLenum p = tex_param_to_gl(param);
    GLint v = tex_value_to_gl(value);
    if (p) {
        glTextureParameteri(tex, p, v);
    }
}

void renderer_generate_mipmap(void) {
    R_Texture tex = g_gl.bound_textures[g_gl.active_texture_unit];
    if (tex == 0) return;
    glGenerateTextureMipmap(tex);
}

void renderer_bind_image_texture(int unit, R_Texture texture, R_Access access) {
    (void)unit;
    (void)texture;
    (void)access;
}

/* ============================================================================
 * Drawing
 * ============================================================================ */

void renderer_draw_arrays(R_Primitive primitive, int first, int count) {
    if (g_gl.bound_vao && g_gl.bound_vbo) {
        int stride = 0;
        for (int i = 0; i < g_vao_count; i++) {
            if (g_vaos[i].vao == g_gl.bound_vao) {
                stride = g_vaos[i].stride;
                break;
            }
        }
        glVertexArrayVertexBuffer(g_gl.bound_vao, 0, g_gl.bound_vbo, 0, stride);
    }
    glDrawArrays(primitive_to_gl(primitive), first, count);
}

void renderer_draw_elements(R_Primitive primitive, int count, int offset) {
    if (g_gl.bound_vao && g_gl.bound_vbo) {
        int stride = 0;
        for (int i = 0; i < g_vao_count; i++) {
            if (g_vaos[i].vao == g_gl.bound_vao) {
                stride = g_vaos[i].stride;
                break;
            }
        }
        glVertexArrayVertexBuffer(g_gl.bound_vao, 0, g_gl.bound_vbo, 0, stride);
    }
    if (g_gl.bound_vao && g_gl.bound_ibo) {
        glVertexArrayElementBuffer(g_gl.bound_vao, g_gl.bound_ibo);
    }
    glDrawElements(primitive_to_gl(primitive), count, GL_UNSIGNED_SHORT, (const void *)(intptr_t)offset);
}

void renderer_draw_arrays_indirect(void) {
}

/* ============================================================================
 * Compute (no-op)
 * ============================================================================ */

void renderer_dispatch_compute(int groups_x, int groups_y, int groups_z) {
    (void)groups_x;
    (void)groups_y;
    (void)groups_z;
}

void renderer_memory_barrier(R_BarrierBits bits) {
    (void)bits;
}
