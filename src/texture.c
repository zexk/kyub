#define STB_IMAGE_IMPLEMENTATION
#include "texture.h"
#include "logger.h"
#include "platform/platform.h"
#include <stb/stb_image.h>
#include <stdio.h>

R_Texture texture_load(const char *path) {
    char *resolved = platform_resolve_path(path);
    if (!resolved) return R_INVALID_HANDLE;

    int width, height, channels;
    unsigned char *data = stbi_load(resolved, &width, &height, &channels, 0);
    if (!data) {
        LOG_ERROR(CAT_RENDERER, "Failed to load texture: %s", resolved);
        free(resolved);
        return R_INVALID_HANDLE;
    }
    free(resolved);

    R_Texture texture = renderer_create_texture();
    renderer_bind_texture(R_TEX_2D, texture);

    renderer_tex_param(R_TEX_2D, R_TEX_WRAP_S, R_TEX_REPEAT);
    renderer_tex_param(R_TEX_2D, R_TEX_WRAP_T, R_TEX_REPEAT);
    renderer_tex_param(R_TEX_2D, R_TEX_MIN_FILTER, R_TEX_NEAREST);
    renderer_tex_param(R_TEX_2D, R_TEX_MAG_FILTER, R_TEX_NEAREST);

    renderer_tex_image_2d(width, height, data);
    renderer_generate_mipmap();

    stbi_image_free(data);
    LOG_INFO(CAT_RENDERER, "Loaded texture: %s (%dx%d)", path, width, height);
    return texture;
}
