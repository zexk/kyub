#include "gl_ext.h"
#include <stdio.h>

PFNGLCREATESHADERPROC glCreateShader = NULL;
PFNGLSHADERSOURCEPROC glShaderSource = NULL;
PFNGLCOMPILESHADERPROC glCompileShader = NULL;
PFNGLGETSHADERIVPROC glGetShaderiv = NULL;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = NULL;
PFNGLCREATEPROGRAMPROC glCreateProgram = NULL;
PFNGLATTACHSHADERPROC glAttachShader = NULL;
PFNGLLINKPROGRAMPROC glLinkProgram = NULL;
PFNGLGETPROGRAMIVPROC glGetProgramiv = NULL;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = NULL;
PFNGLUSEPROGRAMPROC glUseProgram = NULL;
PFNGLDELETESHADERPROC glDeleteShader = NULL;
PFNGLDELETEPROGRAMPROC glDeleteProgram = NULL;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = NULL;
PFNGLUNIFORM1IPROC glUniform1i = NULL;
PFNGLUNIFORM1FPROC glUniform1f = NULL;
PFNGLUNIFORM2FPROC glUniform2f = NULL;
PFNGLUNIFORM3FPROC glUniform3f = NULL;
PFNGLGENERATEMIPMAPPROC glGenerateMipmap = NULL;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;

PFNGLGENBUFFERSPROC glGenBuffers = NULL;
PFNGLBINDBUFFERPROC glBindBuffer = NULL;
PFNGLBUFFERDATAPROC glBufferData = NULL;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = NULL;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = NULL;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = NULL;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = NULL;
PFNGLMAPBUFFERPROC glMapBuffer = NULL;
PFNGLUNMAPBUFFERPROC glUnmapBuffer = NULL;
PFNGLISVERTEXARRAYPROC glIsVertexArray = NULL;
PFNGLISBUFFERPROC glIsBuffer = NULL;
PFNGLCREATEVERTEXARRAYSPROC glCreateVertexArrays = NULL;
PFNGLCREATEBUFFERSPROC glCreateBuffers = NULL;
PFNGLVERTEXARRAYVERTEXBUFFERPROC glVertexArrayVertexBuffer = NULL;
PFNGLVERTEXARRAYATTRIBFORMATPROC glVertexArrayAttribFormat = NULL;
PFNGLVERTEXARRAYATTRIBBINDINGPROC glVertexArrayAttribBinding = NULL;
PFNGLENABLEVERTEXARRAYATTRIBPROC glEnableVertexArrayAttrib = NULL;
PFNGLNAMEDBUFFERDATAPROC glNamedBufferData = NULL;
PFNGLVERTEXARRAYELEMENTBUFFERPROC glVertexArrayElementBuffer = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = NULL;

static void* get_proc(const char* name) {
    void* p = (void*)glXGetProcAddress((const GLubyte*)name);
    if (!p) {
        fprintf(stderr, "Failed to load GL extension: %s\n", name);
    }
    return p;
}

bool gl_ext_init(void) {
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
    glUniform1f = (PFNGLUNIFORM1FPROC)get_proc("glUniform1f");
    glUniform2f = (PFNGLUNIFORM2FPROC)get_proc("glUniform2f");
    glUniform3f = (PFNGLUNIFORM3FPROC)get_proc("glUniform3f");
    glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)get_proc("glGenerateMipmap");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)get_proc("glGetUniformLocation");

    glGenBuffers = (PFNGLGENBUFFERSPROC)get_proc("glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)get_proc("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)get_proc("glBufferData");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)get_proc("glDeleteBuffers");
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)get_proc("glGenVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)get_proc("glBindVertexArray");
    glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)get_proc("glDeleteVertexArrays");
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

    if (!glCreateShader || !glCreateProgram || !glUseProgram) {
        return false;
    }

    return true;
}
