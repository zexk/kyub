#include "shader.h"
#include <stdio.h>
#include <stdlib.h>

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = malloc(length + 1);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate memory for file: %s\n", path);
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, length, file) != (size_t)length) {
        fprintf(stderr, "Failed to read file: %s\n", path);
        free(buffer);
        fclose(file);
        return NULL;
    }
    buffer[length] = '\0';

    fclose(file);
    return buffer;
}

static unsigned int compile_shader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        const char *type_str = "UNKNOWN";
        if (type == GL_VERTEX_SHADER) type_str = "VERTEX";
        else if (type == GL_FRAGMENT_SHADER) type_str = "FRAGMENT";
        else if (type == GL_COMPUTE_SHADER) type_str = "COMPUTE";
        fprintf(stderr, "Shader compilation error (%s):\n%s\n", type_str, info_log);
        return 0;
    }

    return shader;
}

unsigned int shader_create_program(const char* vert_path, const char* frag_path) {
    char* vert_source = read_file(vert_path);
    char* frag_source = read_file(frag_path);

    if (!vert_source || !frag_source) {
        free(vert_source);
        free(frag_source);
        return 0;
    }

    unsigned int vert_shader = compile_shader(GL_VERTEX_SHADER, vert_source);
    unsigned int frag_shader = compile_shader(GL_FRAGMENT_SHADER, frag_source);

    free(vert_source);
    free(frag_source);

    if (!vert_shader || !frag_shader) {
        return 0;
    }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vert_shader);
    glAttachShader(program, frag_shader);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, 512, NULL, info_log);
        fprintf(stderr, "Shader program linking error:\n%s\n", info_log);
        return 0;
    }

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    return program;
}

unsigned int shader_create_compute_program(const char* comp_path) {
    char* comp_source = read_file(comp_path);
    if (!comp_source) return 0;

    unsigned int comp_shader = compile_shader(GL_COMPUTE_SHADER, comp_source);
    free(comp_source);
    if (!comp_shader) return 0;

    unsigned int program = glCreateProgram();
    glAttachShader(program, comp_shader);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, 512, NULL, info_log);
        fprintf(stderr, "Compute shader program linking error:\n%s\n", info_log);
        return 0;
    }

    glDeleteShader(comp_shader);
    return program;
}

