#define STB_IMAGE_IMPLEMENTATION
#include "texture.h"
#include "logger.h"
#include <stb/stb_image.h>
#include <stdio.h>

R_Texture texture_load(const char *path) {
    int width, height, channels;
    unsigned char *data = stbi_load(path, &width, &height, &channels, 0);
    if (!data) {
        LOG_ERROR(CAT_GL, "Failed to load texture: %s", path);
        return R_INVALID_HANDLE;
    }

    R_Texture texture = renderer_create_texture();
    renderer_bind_texture(R_TEX_2D, texture);

    renderer_tex_param(R_TEX_2D, R_TEX_WRAP_S, R_TEX_REPEAT);
    renderer_tex_param(R_TEX_2D, R_TEX_WRAP_T, R_TEX_REPEAT);
    renderer_tex_param(R_TEX_2D, R_TEX_MIN_FILTER, R_TEX_NEAREST);
    renderer_tex_param(R_TEX_2D, R_TEX_MAG_FILTER, R_TEX_NEAREST);

    renderer_tex_image_2d(width, height, data);
    renderer_generate_mipmap();

    stbi_image_free(data);
    LOG_INFO(CAT_GL, "Loaded texture: %s (%dx%d)", path, width, height);
    return texture;
}
