#include "renderer_gl.h"
#include "platform.h"
#include "platform_x11.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/gl.h>

/* Helper to translate R_INVALID_HANDLE (UINT64_MAX) to GL's 0 */
static GLuint to_gl_handle(uint64_t handle) {
    return (handle == R_INVALID_HANDLE) ? 0 : (GLuint)handle;
}

/* Custom GL types not provided by system headers */
typedef char GLchar;
typedef ptrdiff_t GLintptr;
typedef ptrdiff_t GLsizeiptr;

/* Custom GL constants */
#define GL_COMPUTE_SHADER 0x91B9
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#define GL_ATOMIC_COUNTER_BUFFER 0x92C0
#define GL_DISPATCH_INDIRECT_BUFFER 0x90EE
#define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#define GL_BUFFER_UPDATE_BARRIER_BIT 0x00000200
#define GL_ATOMIC_COUNTER_BARRIER_BIT 0x00001000
#define GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT 0x00000001
#define GL_COMMAND_BARRIER_BIT 0x00000040
#define GL_TEXTURE_3D 0x806F
#define GL_RED_INTEGER 0x8D94
#define GL_R8UI 0x8232

/* Function pointer types */
typedef GLuint (APIENTRYP PFNGLCREATESHADERPROC)(GLenum type);
typedef void (APIENTRYP PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (APIENTRYP PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void (APIENTRYP PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef GLuint (APIENTRYP PFNGLCREATEPROGRAMPROC)(void);
typedef void (APIENTRYP PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (APIENTRYP PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (APIENTRYP PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void (APIENTRYP PFNGLDELETESHADERPROC)(GLuint shader);
typedef void (APIENTRYP PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void (APIENTRYP PFNGLUNIFORMMATRIX4FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void (APIENTRYP PFNGLUNIFORM2IPROC)(GLint location, GLint v0, GLint v1);
typedef void (APIENTRYP PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void (APIENTRYP PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRYP PFNGLUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRYP PFNGLTEXIMAGE3DPROC)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP PFNGLTEXSUBIMAGE3DPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP PFNGLGENERATEMIPMAPPROC)(GLenum target);
typedef GLint (APIENTRYP PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar *name);
typedef void (APIENTRYP PFNGLBINDIMAGETEXTUREPROC)(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);

typedef void (APIENTRYP PFNGLGENBUFFERSPROC)(GLsizei n, GLuint *buffers);
typedef void (APIENTRYP PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (APIENTRYP PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (APIENTRYP PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
typedef void (APIENTRYP PFNGLGETBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, void *data);
typedef void (APIENTRYP PFNGLBINDBUFFERBASEPROC)(GLenum target, GLuint index, GLuint buffer);
typedef void (APIENTRYP PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint *buffers);
typedef void (APIENTRYP PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint *arrays);
typedef void (APIENTRYP PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void (APIENTRYP PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint *arrays);
typedef void* (APIENTRYP PFNGLMAPBUFFERPROC)(GLenum target, GLenum access);
typedef GLboolean (APIENTRYP PFNGLUNMAPBUFFERPROC)(GLenum target);
typedef void (APIENTRYP PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void (APIENTRYP PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);

typedef GLboolean (APIENTRYP PFNGLISVERTEXARRAYPROC)(GLuint array);
typedef GLboolean (APIENTRYP PFNGLISBUFFERPROC)(GLuint buffer);
typedef void (APIENTRYP PFNGLCREATEVERTEXARRAYSPROC)(GLsizei n, GLuint *arrays);
typedef void (APIENTRYP PFNGLCREATEBUFFERSPROC)(GLsizei n, GLuint *buffers);
typedef void (APIENTRYP PFNGLVERTEXARRAYVERTEXBUFFERPROC)(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
typedef void (APIENTRYP PFNGLVERTEXARRAYATTRIBFORMATPROC)(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset);
typedef void (APIENTRYP PFNGLVERTEXARRAYATTRIBBINDINGPROC)(GLuint vaobj, GLuint attribindex, GLuint bindingindex);
typedef void (APIENTRYP PFNGLENABLEVERTEXARRAYATTRIBPROC)(GLuint vaobj, GLuint index);
typedef void (APIENTRYP PFNGLNAMEDBUFFERDATAPROC)(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage);
typedef void (APIENTRYP PFNGLVERTEXARRAYELEMENTBUFFERPROC)(GLuint vaobj, GLuint buffer);

typedef void (APIENTRYP PFNGLDISPATCHCOMPUTEPROC)(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
typedef void (APIENTRYP PFNGLMEMORYBARRIERPROC)(GLbitfield barriers);
typedef void (APIENTRYP PFNGLDRAWARRAYSINDIRECTPROC)(GLenum mode, const void *indirect);

/* Function pointers */
static PFNGLCREATESHADERPROC glCreateShader = NULL;
static PFNGLSHADERSOURCEPROC glShaderSource = NULL;
static PFNGLCOMPILESHADERPROC glCompileShader = NULL;
static PFNGLGETSHADERIVPROC glGetShaderiv = NULL;
static PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = NULL;
static PFNGLCREATEPROGRAMPROC glCreateProgram = NULL;
static PFNGLATTACHSHADERPROC glAttachShader = NULL;
static PFNGLLINKPROGRAMPROC glLinkProgram = NULL;
static PFNGLGETPROGRAMIVPROC glGetProgramiv = NULL;
static PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = NULL;
static PFNGLUSEPROGRAMPROC glUseProgram = NULL;
static PFNGLDELETESHADERPROC glDeleteShader = NULL;
static PFNGLDELETEPROGRAMPROC glDeleteProgram = NULL;
static PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = NULL;
static PFNGLUNIFORM1IPROC glUniform1i = NULL;
static PFNGLUNIFORM2IPROC glUniform2i = NULL;
static PFNGLUNIFORM1FPROC glUniform1f = NULL;
static PFNGLUNIFORM2FPROC glUniform2f = NULL;
static PFNGLUNIFORM3FPROC glUniform3f = NULL;
static PFNGLTEXIMAGE3DPROC glTexImage3D_ext = NULL;
static PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D_ext = NULL;
static PFNGLGENERATEMIPMAPPROC glGenerateMipmap_ext = NULL;
static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;
static PFNGLBINDIMAGETEXTUREPROC glBindImageTexture = NULL;

static PFNGLGENBUFFERSPROC glGenBuffers = NULL;
static PFNGLBINDBUFFERPROC glBindBuffer = NULL;
static PFNGLBUFFERDATAPROC glBufferData = NULL;
static PFNGLBUFFERSUBDATAPROC glBufferSubData = NULL;
static PFNGLGETBUFFERSUBDATAPROC glGetBufferSubData_func = NULL;
static PFNGLBINDBUFFERBASEPROC glBindBufferBase = NULL;
static PFNGLDELETEBUFFERSPROC glDeleteBuffers = NULL;
static PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = NULL;
static PFNGLBINDVERTEXARRAYPROC glBindVertexArray = NULL;
static PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = NULL;
static PFNGLMAPBUFFERPROC glMapBuffer = NULL;
static PFNGLUNMAPBUFFERPROC glUnmapBuffer = NULL;
static PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = NULL;
static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = NULL;

static PFNGLISVERTEXARRAYPROC glIsVertexArray = NULL;
static PFNGLISBUFFERPROC glIsBuffer = NULL;
static PFNGLCREATEVERTEXARRAYSPROC glCreateVertexArrays = NULL;
static PFNGLCREATEBUFFERSPROC glCreateBuffers = NULL;
static PFNGLVERTEXARRAYVERTEXBUFFERPROC glVertexArrayVertexBuffer = NULL;
static PFNGLVERTEXARRAYATTRIBFORMATPROC glVertexArrayAttribFormat = NULL;
static PFNGLVERTEXARRAYATTRIBBINDINGPROC glVertexArrayAttribBinding = NULL;
static PFNGLENABLEVERTEXARRAYATTRIBPROC glEnableVertexArrayAttrib = NULL;
static PFNGLNAMEDBUFFERDATAPROC glNamedBufferData = NULL;
static PFNGLVERTEXARRAYELEMENTBUFFERPROC glVertexArrayElementBuffer = NULL;

static PFNGLDISPATCHCOMPUTEPROC glDispatchCompute = NULL;
static PFNGLMEMORYBARRIERPROC glMemoryBarrier = NULL;
static PFNGLDRAWARRAYSINDIRECTPROC glDrawArraysIndirect = NULL;

/* GLX context */
static Display *g_display = NULL;
static Window g_window;
static GLXContext g_context = NULL;
static int g_width = 0;
static int g_height = 0;

static PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = NULL;

typedef void (*PFNGLXSWAPINTERVALEXTPROC_TYPE)(Display*, GLXDrawable, int);

static void* get_proc(const char *name) {
    return (void*)glXGetProcAddress((const GLubyte*)name);
}

static int load_extensions(void) {
    glCreateShader = (PFNGLCREATESHADERPROC)get_proc("glCreateShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)get_proc("glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)get_proc("glCompileShader");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)get_proc("glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)get_proc("glGetShaderInfoLog");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)get_proc("glCreateProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)get_proc("glAttachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)get_proc("glLinkProgram");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)get_proc("glGetProgramiv");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)get_proc("glGetProgramInfoLog");
    glUseProgram = (PFNGLUSEPROGRAMPROC)get_proc("glUseProgram");
    glDeleteShader = (PFNGLDELETESHADERPROC)get_proc("glDeleteShader");
    glDeleteProgram = (PFNGLDELETEPROGRAMPROC)get_proc("glDeleteProgram");
    glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)get_proc("glUniformMatrix4fv");
    glUniform1i = (PFNGLUNIFORM1IPROC)get_proc("glUniform1i");
    glUniform2i = (PFNGLUNIFORM2IPROC)get_proc("glUniform2i");
    glUniform1f = (PFNGLUNIFORM1FPROC)get_proc("glUniform1f");
    glUniform2f = (PFNGLUNIFORM2FPROC)get_proc("glUniform2f");
    glUniform3f = (PFNGLUNIFORM3FPROC)get_proc("glUniform3f");
    glTexImage3D_ext = (PFNGLTEXIMAGE3DPROC)get_proc("glTexImage3D");
    glTexSubImage3D_ext = (PFNGLTEXSUBIMAGE3DPROC)get_proc("glTexSubImage3D");
    glGenerateMipmap_ext = (PFNGLGENERATEMIPMAPPROC)get_proc("glGenerateMipmap");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)get_proc("glGetUniformLocation");
    glBindImageTexture = (PFNGLBINDIMAGETEXTUREPROC)get_proc("glBindImageTexture");

    glGenBuffers = (PFNGLGENBUFFERSPROC)get_proc("glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)get_proc("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)get_proc("glBufferData");
    glBufferSubData = (PFNGLBUFFERSUBDATAPROC)get_proc("glBufferSubData");
    glGetBufferSubData_func = (PFNGLGETBUFFERSUBDATAPROC)get_proc("glGetBufferSubData");
    glBindBufferBase = (PFNGLBINDBUFFERBASEPROC)get_proc("glBindBufferBase");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)get_proc("glDeleteBuffers");
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)get_proc("glGenVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)get_proc("glBindVertexArray");
    glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)get_proc("glDeleteVertexArrays");
    glMapBuffer = (PFNGLMAPBUFFERPROC)get_proc("glMapBuffer");
    glUnmapBuffer = (PFNGLUNMAPBUFFERPROC)get_proc("glUnmapBuffer");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)get_proc("glVertexAttribPointer");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)get_proc("glEnableVertexAttribArray");

    glIsVertexArray = (PFNGLISVERTEXARRAYPROC)get_proc("glIsVertexArray");
    glIsBuffer = (PFNGLISBUFFERPROC)get_proc("glIsBuffer");
    glCreateVertexArrays = (PFNGLCREATEVERTEXARRAYSPROC)get_proc("glCreateVertexArrays");
    glCreateBuffers = (PFNGLCREATEBUFFERSPROC)get_proc("glCreateBuffers");
    glVertexArrayVertexBuffer = (PFNGLVERTEXARRAYVERTEXBUFFERPROC)get_proc("glVertexArrayVertexBuffer");
    glVertexArrayAttribFormat = (PFNGLVERTEXARRAYATTRIBFORMATPROC)get_proc("glVertexArrayAttribFormat");
    glVertexArrayAttribBinding = (PFNGLVERTEXARRAYATTRIBBINDINGPROC)get_proc("glVertexArrayAttribBinding");
    glEnableVertexArrayAttrib = (PFNGLENABLEVERTEXARRAYATTRIBPROC)get_proc("glEnableVertexArrayAttrib");
    glNamedBufferData = (PFNGLNAMEDBUFFERDATAPROC)get_proc("glNamedBufferData");
    glVertexArrayElementBuffer = (PFNGLVERTEXARRAYELEMENTBUFFERPROC)get_proc("glVertexArrayElementBuffer");

    glDispatchCompute = (PFNGLDISPATCHCOMPUTEPROC)get_proc("glDispatchCompute");
    glMemoryBarrier = (PFNGLMEMORYBARRIERPROC)get_proc("glMemoryBarrier");
    glDrawArraysIndirect = (PFNGLDRAWARRAYSINDIRECTPROC)get_proc("glDrawArraysIndirect");

    glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)get_proc("glXSwapIntervalEXT");
    if (!glXSwapIntervalEXT) {
        glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)get_proc("glXSwapIntervalMESA");
    }

    return (glCreateShader && glGenBuffers && glGenVertexArrays) ? 0 : -1;
}

static char* load_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc(size + 1);
    if (buf) {
        fread(buf, 1, size, f);
        buf[size] = '\0';
    }
    fclose(f);
    return buf;
}

/* ============================================================================
 * Init / Lifecycle
 * ============================================================================ */

void renderer_init(int width, int height) {
    g_width = width;
    g_height = height;

    g_display = (Display*)platform_x11_get_display();
    g_window = platform_x11_get_window();

    int screen = DefaultScreen(g_display);
    int attribs[] = {
        GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER,
        GLX_SAMPLE_BUFFERS, 1, GLX_SAMPLES, 4, None
    };
    XVisualInfo *vi = glXChooseVisual(g_display, screen, attribs);
    if (!vi) {
        fprintf(stderr, "Failed to choose GL visual\n");
        exit(1);
    }

    g_context = glXCreateContext(g_display, vi, NULL, GL_TRUE);
    if (!g_context) {
        fprintf(stderr, "Failed to create GL context\n");
        exit(1);
    }

    glXMakeCurrent(g_display, g_window, g_context);

    if (load_extensions() < 0) {
        fprintf(stderr, "Failed to load GL extensions\n");
        exit(1);
    }

    if (glXSwapIntervalEXT) {
        glXSwapIntervalEXT(g_display, g_window, 1);
    }
}

void renderer_shutdown(void) {
    if (g_context) {
        glXMakeCurrent(g_display, None, NULL);
        glXDestroyContext(g_display, g_context);
        g_context = NULL;
    }
}

void* renderer_gl_get_display(void) { return g_display; }
void* renderer_gl_get_window(void) { return (void*)(size_t)g_window; }

/* ============================================================================
 * State Mapping Helpers
 * ============================================================================ */

static GLenum cap_to_gl(R_Cap cap) {
    switch (cap) {
        case R_CAP_DEPTH_TEST: return GL_DEPTH_TEST;
        case R_CAP_CULL_FACE: return GL_CULL_FACE;
        case R_CAP_BLEND: return GL_BLEND;
        case R_CAP_MULTISAMPLE: return GL_MULTISAMPLE;
        case R_CAP_POLYGON_OFFSET_LINE: return GL_POLYGON_OFFSET_LINE;
        case R_CAP_SCISSOR_TEST: return GL_SCISSOR_TEST;
    }
    return 0;
}

static GLenum depth_func_to_gl(R_DepthFunc func) {
    switch (func) {
        case R_FUNC_LESS: return GL_LESS;
        case R_FUNC_LEQUAL: return GL_LEQUAL;
        case R_FUNC_ALWAYS: return GL_ALWAYS;
    }
    return GL_LESS;
}

static GLenum blend_to_gl(R_BlendFactor f) {
    switch (f) {
        case R_BLEND_ZERO: return GL_ZERO;
        case R_BLEND_ONE: return GL_ONE;
        case R_BLEND_SRC_ALPHA: return GL_SRC_ALPHA;
        case R_BLEND_ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
    }
    return GL_ZERO;
}

static GLenum prim_to_gl(R_Primitive p) {
    switch (p) {
        case R_PRIM_TRIANGLES: return GL_TRIANGLES;
        case R_PRIM_LINES: return GL_LINES;
        case R_PRIM_TRIANGLE_FAN: return GL_TRIANGLE_FAN;
    }
    return GL_TRIANGLES;
}

static GLenum buf_target_to_gl(R_BufferTarget t) {
    switch (t) {
        case R_BUF_ARRAY: return GL_ARRAY_BUFFER;
        case R_BUF_ELEMENT: return GL_ELEMENT_ARRAY_BUFFER;
        case R_BUF_SHADER_STORAGE: return GL_SHADER_STORAGE_BUFFER;
        case R_BUF_ATOMIC_COUNTER: return GL_ATOMIC_COUNTER_BUFFER;
        case R_BUF_DRAW_INDIRECT: return GL_DRAW_INDIRECT_BUFFER;
    }
    return GL_ARRAY_BUFFER;
}

static GLenum usage_to_gl(R_Usage u) {
    switch (u) {
        case R_USAGE_STATIC: return GL_STATIC_DRAW;
        case R_USAGE_DYNAMIC: return GL_DYNAMIC_DRAW;
    }
    return GL_STATIC_DRAW;
}

static GLenum tex_target_to_gl(R_TextureTarget t) {
    switch (t) {
        case R_TEX_2D: return GL_TEXTURE_2D;
        case R_TEX_3D: return GL_TEXTURE_3D;
    }
    return GL_TEXTURE_2D;
}

static GLenum tex_param_to_gl(R_TexParam p) {
    switch (p) {
        case R_TEX_WRAP_S: return GL_TEXTURE_WRAP_S;
        case R_TEX_WRAP_T: return GL_TEXTURE_WRAP_T;
        case R_TEX_WRAP_R: return GL_TEXTURE_WRAP_R;
        case R_TEX_MIN_FILTER: return GL_TEXTURE_MIN_FILTER;
        case R_TEX_MAG_FILTER: return GL_TEXTURE_MAG_FILTER;
    }
    return 0;
}

static GLenum tex_value_to_gl(R_TexValue v) {
    switch (v) {
        case R_TEX_REPEAT: return GL_REPEAT;
        case R_TEX_CLAMP_TO_EDGE: return GL_CLAMP_TO_EDGE;
        case R_TEX_NEAREST: return GL_NEAREST;
        case R_TEX_LINEAR: return GL_LINEAR;
    }
    return 0;
}

static GLenum type_to_gl(R_Type t) {
    switch (t) {
        case R_TYPE_FLOAT: return GL_FLOAT;
        case R_TYPE_UBYTE: return GL_UNSIGNED_BYTE;
    }
    return GL_FLOAT;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void renderer_swap(void) {
    glXSwapBuffers(g_display, g_window);
}

void renderer_swap_interval(int interval) {
    if (glXSwapIntervalEXT) {
        glXSwapIntervalEXT(g_display, g_window, interval);
    }
}

void renderer_get_size(int *width, int *height) {
    *width = g_width;
    *height = g_height;
}

void renderer_viewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void renderer_clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void renderer_clear_depth(void) {
    glClear(GL_DEPTH_BUFFER_BIT);
}

void renderer_enable(R_Cap cap) {
    glEnable(cap_to_gl(cap));
}

void renderer_disable(R_Cap cap) {
    glDisable(cap_to_gl(cap));
}

void renderer_depth_mask(bool write) {
    glDepthMask(write ? GL_TRUE : GL_FALSE);
}

void renderer_depth_func(R_DepthFunc func) {
    glDepthFunc(depth_func_to_gl(func));
}

void renderer_blend_func(R_BlendFactor src, R_BlendFactor dst) {
    glBlendFunc(blend_to_gl(src), blend_to_gl(dst));
}

void renderer_polygon_offset(float factor, float units) {
    glPolygonOffset(factor, units);
}

void renderer_line_width(float width) {
    glLineWidth(width);
}

void renderer_push_attrib(void) {
    glPushAttrib(GL_ENABLE_BIT | GL_BLEND);
}

void renderer_pop_attrib(void) {
    glPopAttrib();
}

/* ============================================================================
 * Shaders
 * ============================================================================ */

R_Program renderer_create_program(const char *vert_path, const char *frag_path) {
    /* Use .gl shader variants for OpenGL */
    char gl_vert_path[256];
    char gl_frag_path[256];
    
    /* Replace .vert with .gl.vert */
    const char *vert_ext = strrchr(vert_path, '.');
    if (vert_ext) {
        size_t base_len = vert_ext - vert_path;
        snprintf(gl_vert_path, sizeof(gl_vert_path), "%.*s.gl%s", (int)base_len, vert_path, vert_ext);
    } else {
        snprintf(gl_vert_path, sizeof(gl_vert_path), "%s.gl", vert_path);
    }
    
    /* Replace .frag with .gl.frag */
    const char *frag_ext = strrchr(frag_path, '.');
    if (frag_ext) {
        size_t base_len = frag_ext - frag_path;
        snprintf(gl_frag_path, sizeof(gl_frag_path), "%.*s.gl%s", (int)base_len, frag_path, frag_ext);
    } else {
        snprintf(gl_frag_path, sizeof(gl_frag_path), "%s.gl", frag_path);
    }
    
    char *vert_src = load_file(gl_vert_path);
    char *frag_src = load_file(gl_frag_path);
    if (!vert_src || !frag_src) {
        free(vert_src);
        free(frag_src);
        return R_INVALID_HANDLE;
    }

    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vert, 1, (const GLchar**)&vert_src, NULL);
    glShaderSource(frag, 1, (const GLchar**)&frag_src, NULL);
    glCompileShader(vert);
    glCompileShader(frag);

    GLint success;
    glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(vert, 512, NULL, log);
        fprintf(stderr, "Vertex shader compile error: %s\n", log);
    }
    glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(frag, 512, NULL, log);
        fprintf(stderr, "Fragment shader compile error: %s\n", log);
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(prog, 512, NULL, log);
        fprintf(stderr, "Shader link error: %s\n", log);
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
    free(vert_src);
    free(frag_src);

    return (R_Program)prog;
}

R_Program renderer_create_compute(const char *comp_path) {
    char *comp_src = load_file(comp_path);
    if (!comp_src) {
        fprintf(stderr, "Failed to load compute shader: %s\n", comp_path);
        return R_INVALID_HANDLE;
    }

    GLuint comp = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(comp, 1, (const GLchar**)&comp_src, NULL);
    glCompileShader(comp);

    GLint success;
    glGetShaderiv(comp, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(comp, 512, NULL, log);
        fprintf(stderr, "Compute shader compile error: %s\n", log);
        free(comp_src);
        return R_INVALID_HANDLE;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, comp);
    glLinkProgram(prog);

    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(prog, 512, NULL, log);
        fprintf(stderr, "Compute shader link error: %s\n", log);
    }

    glDeleteShader(comp);
    free(comp_src);

    return (R_Program)prog;
}

void renderer_destroy_program(R_Program program) {
    if (program != R_INVALID_HANDLE) {
        glDeleteProgram(to_gl_handle(program));
    }
}

void renderer_use_program(R_Program program) {
    glUseProgram(to_gl_handle(program));
}

int renderer_uniform_location(R_Program program, const char *name) {
    return glGetUniformLocation(to_gl_handle(program), name);
}

void renderer_uniform_mat4(int location, const float *matrix) {
    glUniformMatrix4fv(location, 1, GL_FALSE, matrix);
}

void renderer_uniform_vec3(int location, float x, float y, float z) {
    glUniform3f(location, x, y, z);
}

void renderer_uniform_vec2(int location, float x, float y) {
    glUniform2f(location, x, y);
}

void renderer_uniform_float(int location, float value) {
    glUniform1f(location, value);
}

void renderer_uniform_int(int location, int value) {
    glUniform1i(location, value);
}

void renderer_uniform_ivec2(int location, int x, int y) {
    glUniform2i(location, x, y);
}

/* ============================================================================
 * Buffers
 * ============================================================================ */

R_Buffer renderer_create_buffer(void) {
    GLuint buf;
    glGenBuffers(1, &buf);
    return (R_Buffer)buf;
}

void renderer_destroy_buffer(R_Buffer buffer) {
    GLuint buf = to_gl_handle(buffer);
    glDeleteBuffers(1, &buf);
}

void renderer_bind_buffer(R_BufferTarget target, R_Buffer buffer) {
    glBindBuffer(buf_target_to_gl(target), to_gl_handle(buffer));
}

void renderer_buffer_data(R_BufferTarget target, size_t size, const void *data, R_Usage usage) {
    glBufferData(buf_target_to_gl(target), (GLsizeiptr)size, data, usage_to_gl(usage));
}

void renderer_buffer_sub_data(R_BufferTarget target, size_t offset, size_t size, const void *data) {
    glBufferSubData(buf_target_to_gl(target), (GLintptr)offset, (GLsizeiptr)size, data);
}

void renderer_get_buffer_sub_data(R_BufferTarget target, size_t offset, size_t size, void *data) {
    if (glGetBufferSubData_func) {
        glGetBufferSubData_func(buf_target_to_gl(target), (GLintptr)offset, (GLsizeiptr)size, data);
    }
}

void renderer_bind_buffer_base(R_BufferTarget target, int index, R_Buffer buffer) {
    glBindBufferBase(buf_target_to_gl(target), (GLuint)index, to_gl_handle(buffer));
}

/* ============================================================================
 * Vertex Arrays
 * ============================================================================ */

R_VAO renderer_create_vao(void) {
    GLuint vao;
    glGenVertexArrays(1, &vao);
    return (R_VAO)vao;
}

void renderer_destroy_vao(R_VAO vao) {
    GLuint v = to_gl_handle(vao);
    glDeleteVertexArrays(1, &v);
}

void renderer_bind_vao(R_VAO vao) {
    glBindVertexArray(to_gl_handle(vao));
}

void renderer_attrib_pointer(int index, int size, R_Type type, bool normalized, int stride, int offset) {
    glVertexAttribPointer((GLuint)index, size, type_to_gl(type), normalized ? GL_TRUE : GL_FALSE,
                          stride, (const void*)(size_t)offset);
}

void renderer_enable_attrib(int index) {
    glEnableVertexAttribArray((GLuint)index);
}

/* ============================================================================
 * Textures
 * ============================================================================ */

R_Texture renderer_create_texture(void) {
    GLuint tex;
    glGenTextures(1, &tex);
    return (R_Texture)tex;
}

void renderer_destroy_texture(R_Texture texture) {
    GLuint t = to_gl_handle(texture);
    glDeleteTextures(1, &t);
}

void renderer_bind_texture(R_TextureTarget target, R_Texture texture) {
    glBindTexture(tex_target_to_gl(target), to_gl_handle(texture));
}

void renderer_active_texture(int unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
}

void renderer_tex_image_2d(int width, int height, const void *data) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
}

void renderer_tex_image_3d(int width, int height, int depth, const void *data) {
    if (glTexImage3D_ext) {
        glTexImage3D_ext(GL_TEXTURE_3D, 0, GL_R8UI, width, height, depth, 0,
                     GL_RED_INTEGER, GL_UNSIGNED_BYTE, data);
    }
}

void renderer_tex_sub_image_3d(int x, int y, int z, int width, int height, int depth, const void *data) {
    if (glTexSubImage3D_ext) {
        glTexSubImage3D_ext(GL_TEXTURE_3D, 0, x, y, z, width, height, depth,
                        GL_RED_INTEGER, GL_UNSIGNED_BYTE, data);
    }
}

void renderer_tex_param(R_TextureTarget target, R_TexParam param, R_TexValue value) {
    glTexParameteri(tex_target_to_gl(target), tex_param_to_gl(param), tex_value_to_gl(value));
}

void renderer_generate_mipmap(void) {
    if (glGenerateMipmap_ext) {
        glGenerateMipmap_ext(GL_TEXTURE_2D);
    }
}

void renderer_bind_image_texture(int unit, R_Texture texture, R_Access access) {
    (void)access;
    if (glBindImageTexture) {
        glBindImageTexture((GLuint)unit, to_gl_handle(texture), 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8UI);
    }
}

/* ============================================================================
 * Drawing
 * ============================================================================ */

void renderer_draw_arrays(R_Primitive primitive, int first, int count) {
    glDrawArrays(prim_to_gl(primitive), first, count);
}

void renderer_draw_elements(R_Primitive primitive, int count, int offset) {
    glDrawElements(prim_to_gl(primitive), count, GL_UNSIGNED_SHORT, (const void*)(size_t)(offset * sizeof(unsigned short)));
}

void renderer_draw_arrays_indirect(void) {
    if (glDrawArraysIndirect) {
        glDrawArraysIndirect(GL_TRIANGLES, NULL);
    }
}

/* ============================================================================
 * Compute
 * ============================================================================ */

void renderer_dispatch_compute(int groups_x, int groups_y, int groups_z) {
    if (glDispatchCompute) {
        glDispatchCompute((GLuint)groups_x, (GLuint)groups_y, (GLuint)groups_z);
    }
}

void renderer_memory_barrier(R_BarrierBits bits) {
    (void)bits;
    if (glMemoryBarrier) {
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    }
}
