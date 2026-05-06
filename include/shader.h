#ifndef SHADER_H
#define SHADER_H

#include "gl_ext.h"

unsigned int shader_create_program(const char *vert_path, const char *frag_path);
unsigned int shader_create_compute_program(const char *comp_path);

#endif // SHADER_H

