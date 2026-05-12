#ifndef TEXTURE_H
#define TEXTURE_H

#include "renderer/renderer.h"

R_Texture texture_load(const char *path);
R_Texture texture_load_array(const char **paths, int count, int tex_width, int tex_height);

#endif // TEXTURE_H
