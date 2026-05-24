#include "renderer/renderer_internal.h"
/* ============================================================================
 * Texture helpers
 * ============================================================================ */

VkImage create_image(uint32_t width, uint32_t height, uint32_t depth, uint32_t array_layers,
                     VkFormat format, VkImageUsageFlags usage, VkDeviceMemory *out_memory) {
    VkImageCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    if (array_layers > 1) {
        create_info.imageType = VK_IMAGE_TYPE_2D;
        create_info.extent.depth = 1;
        create_info.arrayLayers = array_layers;
    } else {
        create_info.imageType = depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
        create_info.extent.depth = depth;
        create_info.arrayLayers = 1;
    }
    create_info.extent.width = width;
    create_info.extent.height = height;
    create_info.mipLevels = 1;
    create_info.format = format;
    create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    create_info.usage = usage;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.samples = VK_SAMPLE_COUNT_1_BIT;

    VkImage image;
    if (vkCreateImage(g_vk.device, &create_info, NULL, &image) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(g_vk.device, image, &mem_reqs);

    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    uint32_t mem_type = find_memory_type(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mem_type == UINT32_MAX) {
        vkDestroyImage(g_vk.device, image, NULL);
        return VK_NULL_HANDLE;
    }
    alloc_info.memoryTypeIndex = mem_type;

    if (vkAllocateMemory(g_vk.device, &alloc_info, NULL, out_memory) != VK_SUCCESS) {
        vkDestroyImage(g_vk.device, image, NULL);
        return VK_NULL_HANDLE;
    }
    vkBindImageMemory(g_vk.device, image, *out_memory, 0);

    return image;
}

VkImageView create_image_view(VkImage image, VkFormat format, VkImageViewType view_type) {
    VkImageViewCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = image;
    create_info.viewType = view_type;
    create_info.format = format;
    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;
    
    VkImageView view;
    VkResult res = vkCreateImageView(g_vk.device, &create_info, NULL, &view);
    if (res != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return view;
}

static VkImageView create_image_array_view(VkImage image, VkFormat format, uint32_t layers) {
    VkImageViewCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = image;
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    create_info.format = format;
    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = layers;

    VkImageView view;
    if (vkCreateImageView(g_vk.device, &create_info, NULL, &view) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return view;
}

static bool transition_image_array_layout(VkImage image, uint32_t layers,
                                          VkImageLayout old_layout, VkImageLayout new_layout) {
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = g_vk.cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(g_vk.device, &alloc_info, &cmd) != VK_SUCCESS) return false;

    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layers;

    VkPipelineStageFlags src_stage;
    VkPipelineStageFlags dst_stage;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    vkQueueSubmit(g_vk.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_vk.graphics_queue);
    vkFreeCommandBuffers(g_vk.device, g_vk.cmd_pool, 1, &cmd);
    return true;
}

static void upload_image_array_layer(VkImage image, uint32_t layer,
                                     uint32_t width, uint32_t height,
                                     const void *data, VkDeviceSize data_size) {
    if (data_size > g_vk.staging_size) {
        fprintf(stderr, "Staging buffer too small for texture upload (%llu > %llu)\n",
                (unsigned long long)data_size, (unsigned long long)g_vk.staging_size);
        return;
    }

    void *mapped;
    vkMapMemory(g_vk.device, g_vk.staging_memory, 0, data_size, 0, &mapped);
    memcpy(mapped, data, data_size);
    vkUnmapMemory(g_vk.device, g_vk.staging_memory);

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = g_vk.cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(g_vk.device, &alloc_info, &cmd) != VK_SUCCESS) {
        return;
    }

    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    VkBufferImageCopy region = {0};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = layer;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = (VkOffset3D){0, 0, 0};
    region.imageExtent = (VkExtent3D){width, height, 1};

    vkCmdCopyBufferToImage(cmd, g_vk.staging_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    vkQueueSubmit(g_vk.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_vk.graphics_queue);
    vkFreeCommandBuffers(g_vk.device, g_vk.cmd_pool, 1, &cmd);
}

bool transition_image_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout) {
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = g_vk.cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(g_vk.device, &alloc_info, &cmd) != VK_SUCCESS) return false;

    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags src_stage, dst_stage;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    vkQueueSubmit(g_vk.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_vk.graphics_queue);

    vkFreeCommandBuffers(g_vk.device, g_vk.cmd_pool, 1, &cmd);
    return true;
}

void upload_image_data(VkImage image, uint32_t width, uint32_t height, uint32_t depth,
                               const void *data, VkDeviceSize data_size) {
    if (data_size > g_vk.staging_size) {
        fprintf(stderr, "Staging buffer too small for texture upload (%llu > %llu)\n",
                (unsigned long long)data_size, (unsigned long long)g_vk.staging_size);
        return;
    }

    void *mapped;
    vkMapMemory(g_vk.device, g_vk.staging_memory, 0, data_size, 0, &mapped);
    memcpy(mapped, data, data_size);
    vkUnmapMemory(g_vk.device, g_vk.staging_memory);

    if (!transition_image_layout(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) {
        return;
    }

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = g_vk.cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(g_vk.device, &alloc_info, &cmd) != VK_SUCCESS) {
        return;
    }

    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    VkBufferImageCopy region = {0};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = (VkOffset3D){0, 0, 0};
    region.imageExtent = (VkExtent3D){width, height, depth};

    vkCmdCopyBufferToImage(cmd, g_vk.staging_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    vkQueueSubmit(g_vk.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_vk.graphics_queue);

    vkFreeCommandBuffers(g_vk.device, g_vk.cmd_pool, 1, &cmd);

    transition_image_layout(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

/* ============================================================================
 * Public API: Textures
 * ============================================================================ */

R_Texture renderer_create_texture(void) {
    CHECK_DEVICE_RET(R_INVALID_HANDLE);
    if (g_vk.texture_count >= MAX_TEXTURES) return R_INVALID_HANDLE;
    uint32_t idx = g_vk.texture_count++;
    g_vk.texture_samplers[idx] = g_vk.default_sampler;
    return idx;
}

void renderer_destroy_texture(R_Texture texture) {
    CHECK_DEVICE();
    if (texture >= g_vk.texture_count) return;
    if (g_vk.textures[texture]) {
        vkDestroyImageView(g_vk.device, g_vk.texture_views[texture], NULL);
        vkDestroyImage(g_vk.device, g_vk.textures[texture], NULL);
        vkFreeMemory(g_vk.device, g_vk.texture_memories[texture], NULL);
        g_vk.textures[texture] = VK_NULL_HANDLE;
    }
    if (g_vk.texture_samplers[texture] != g_vk.default_sampler) {
        vkDestroySampler(g_vk.device, g_vk.texture_samplers[texture], NULL);
    }
    g_vk.texture_samplers[texture] = VK_NULL_HANDLE;
}

void renderer_bind_texture(R_TextureTarget target, R_Texture texture) {
    (void)target;
    if (texture >= g_vk.texture_count) return;
    g_bound_textures[g_active_texture_unit] = texture;
    
    /* Update descriptor set for the active pipeline if it has a texture layout */
    if (g_vk.active_pipeline < g_vk.pipeline_count && texture != R_INVALID_HANDLE) {
        Pipeline *pipe = &g_vk.pipelines[g_vk.active_pipeline];
        if (pipe->desc_set != VK_NULL_HANDLE && pipe->desc_set_layout != VK_NULL_HANDLE) {
            VkDescriptorImageInfo image_info = {0};
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info.imageView = g_vk.texture_views[texture];
            image_info.sampler = g_vk.texture_samplers[texture];
            
            VkWriteDescriptorSet write = {0};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = pipe->desc_set;
            write.dstBinding = 0;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &image_info;
            
            vkUpdateDescriptorSets(g_vk.device, 1, &write, 0, NULL);
        }
    }
}

void renderer_active_texture(int unit) {
    g_active_texture_unit = unit;
}

R_Texture renderer_create_texture_array(int width, int height, int layers) {
    CHECK_DEVICE_RET(R_INVALID_HANDLE);
    R_Texture tex = renderer_create_texture();
    if (tex == R_INVALID_HANDLE) return tex;

    g_vk.textures[tex] = create_image((uint32_t)width, (uint32_t)height, 1, (uint32_t)layers,
                                      VK_FORMAT_R8G8B8A8_SRGB,
                                      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                      &g_vk.texture_memories[tex]);
    if (g_vk.textures[tex] == VK_NULL_HANDLE) {
        return R_INVALID_HANDLE;
    }

    g_vk.texture_widths[tex] = (uint32_t)width;
    g_vk.texture_heights[tex] = (uint32_t)height;
    g_vk.texture_depths[tex] = (uint32_t)layers;
    g_vk.texture_views[tex] = create_image_array_view(g_vk.textures[tex], VK_FORMAT_R8G8B8A8_SRGB, (uint32_t)layers);
    if (g_vk.texture_views[tex] == VK_NULL_HANDLE) {
        renderer_destroy_texture(tex);
        return R_INVALID_HANDLE;
    }
    transition_image_array_layout(g_vk.textures[tex], (uint32_t)layers,
                                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    return tex;
}

void renderer_tex_sub_image_array(int layer, int width, int height, const void *data) {
    CHECK_DEVICE();
    R_Texture tex = g_bound_textures[g_active_texture_unit];
    if (tex >= g_vk.texture_count || !g_vk.textures[tex] || !data) return;
    if (layer < 0 || (uint32_t)layer >= g_vk.texture_depths[tex]) return;

    VkDeviceSize data_size = (VkDeviceSize)width * (VkDeviceSize)height * 4;
    upload_image_array_layer(g_vk.textures[tex], (uint32_t)layer,
                             (uint32_t)width, (uint32_t)height,
                             data, data_size);
    if ((uint32_t)layer + 1 == g_vk.texture_depths[tex]) {
        transition_image_array_layout(g_vk.textures[tex], g_vk.texture_depths[tex],
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void renderer_tex_image_2d(int width, int height, const void *data) {
    CHECK_DEVICE();
    R_Texture tex = g_bound_textures[g_active_texture_unit];
    if (tex >= g_vk.texture_count) return;

    bool same_dims = (g_vk.textures[tex] &&
                      g_vk.texture_widths[tex] == (uint32_t)width &&
                      g_vk.texture_heights[tex] == (uint32_t)height &&
                      g_vk.texture_depths[tex] == 1);
    if (!same_dims && g_vk.textures[tex]) {
        renderer_destroy_texture(tex);
    }

    if (!same_dims) {
        g_vk.textures[tex] = create_image(width, height, 1, 1, VK_FORMAT_R8G8B8A8_SRGB,
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                           &g_vk.texture_memories[tex]);
        g_vk.texture_widths[tex] = width;
        g_vk.texture_heights[tex] = height;
        g_vk.texture_depths[tex] = 1;
    }

    /* Upload data */
    VkDeviceSize data_size = width * height * 4; /* RGBA */
    if (data) {
        upload_image_data(g_vk.textures[tex], width, height, 1, data, data_size);
    }
    
    /* Create view */
    g_vk.texture_views[tex] = create_image_view(g_vk.textures[tex], VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_VIEW_TYPE_2D);
}

void renderer_tex_image_3d(int width, int height, int depth, const void *data) {
    R_Texture tex = g_bound_textures[g_active_texture_unit];
    if (tex >= g_vk.texture_count) return;

    bool same_dims = (g_vk.textures[tex] &&
                      g_vk.texture_widths[tex] == (uint32_t)width &&
                      g_vk.texture_heights[tex] == (uint32_t)height &&
                      g_vk.texture_depths[tex] == (uint32_t)depth);
    if (!same_dims && g_vk.textures[tex]) {
        renderer_destroy_texture(tex);
    }

    if (!same_dims) {
        g_vk.textures[tex] = create_image(width, height, depth, 1, VK_FORMAT_R8G8B8A8_SRGB,
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                           &g_vk.texture_memories[tex]);
        g_vk.texture_widths[tex] = width;
        g_vk.texture_heights[tex] = height;
        g_vk.texture_depths[tex] = depth;
    }

    /* Upload data */
    VkDeviceSize data_size = width * height * depth * 4; /* RGBA */
    if (data) {
        upload_image_data(g_vk.textures[tex], width, height, depth, data, data_size);
    }

    if (!same_dims) {
        g_vk.texture_views[tex] = create_image_view(g_vk.textures[tex], VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_VIEW_TYPE_3D);
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
    (void)target;
    R_Texture tex = g_bound_textures[g_active_texture_unit];
    if (tex >= g_vk.texture_count) return;
    
    /* Create a new sampler with the specified parameters */
    VkSamplerCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    
    /* Default to nearest/clamp */
    create_info.magFilter = VK_FILTER_NEAREST;
    create_info.minFilter = VK_FILTER_NEAREST;
    create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    create_info.anisotropyEnable = VK_FALSE;
    create_info.maxAnisotropy = 1.0f;
    create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    create_info.unnormalizedCoordinates = VK_FALSE;
    create_info.compareEnable = VK_FALSE;
    create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    
    /* Apply parameters */
    if (param == R_TEX_MIN_FILTER || param == R_TEX_MAG_FILTER) {
        VkFilter filter = (value == R_TEX_LINEAR) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        if (param == R_TEX_MIN_FILTER) create_info.minFilter = filter;
        if (param == R_TEX_MAG_FILTER) create_info.magFilter = filter;
    }
    
    if (param == R_TEX_WRAP_S || param == R_TEX_WRAP_T || param == R_TEX_WRAP_R) {
        VkSamplerAddressMode mode = (value == R_TEX_REPEAT) ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        if (param == R_TEX_WRAP_S) create_info.addressModeU = mode;
        if (param == R_TEX_WRAP_T) create_info.addressModeV = mode;
        if (param == R_TEX_WRAP_R) create_info.addressModeW = mode;
    }
    
    /* Destroy old sampler */
    if (g_vk.texture_samplers[tex] != g_vk.default_sampler) {
        vkDestroySampler(g_vk.device, g_vk.texture_samplers[tex], NULL);
    }
    
    vkCreateSampler(g_vk.device, &create_info, NULL, &g_vk.texture_samplers[tex]);
}

void renderer_generate_mipmap(void) {
}

void renderer_bind_image_texture(int unit, R_Texture texture, R_Access access) {
    (void)unit;
    (void)texture;
    (void)access;
}
