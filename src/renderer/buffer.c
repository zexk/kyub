#include "renderer/renderer_internal.h"
/* ============================================================================
 * Buffer helpers
 * ============================================================================ */

VkBuffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkDeviceMemory *out_memory) {
    VkBufferCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size = size;
    create_info.usage = usage;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(g_vk.device, &create_info, NULL, &buffer) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create buffer of size %zu\n", (size_t)size);
        return VK_NULL_HANDLE;
    }

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(g_vk.device, buffer, &mem_reqs);

    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    uint32_t mem_type = find_memory_type(mem_reqs.memoryTypeBits, props);
    if (mem_type == UINT32_MAX) {
        vkDestroyBuffer(g_vk.device, buffer, NULL);
        return VK_NULL_HANDLE;
    }
    alloc_info.memoryTypeIndex = mem_type;

    if (vkAllocateMemory(g_vk.device, &alloc_info, NULL, out_memory) != VK_SUCCESS) {
        fprintf(stderr, "Failed to allocate %zu bytes of memory for buffer\n", (size_t)mem_reqs.size);
        vkDestroyBuffer(g_vk.device, buffer, NULL);
        return VK_NULL_HANDLE;
    }
    vkBindBufferMemory(g_vk.device, buffer, *out_memory, 0);

    return buffer;
}
/* ============================================================================
 * Public API: Buffers
 * ============================================================================ */

R_Buffer renderer_create_buffer(void) {
    CHECK_DEVICE_RET(R_INVALID_HANDLE);
    if (g_vk.buffer_count >= MAX_BUFFERS) return R_INVALID_HANDLE;

    uint32_t idx = g_vk.buffer_count++;
    g_vk.buffers[idx] = VK_NULL_HANDLE;
    g_vk.buffer_memories[idx] = VK_NULL_HANDLE;
    g_vk.buffer_sizes[idx] = 0;

    return idx;
}

void renderer_destroy_buffer(R_Buffer buffer) {
    CHECK_DEVICE();
    if (buffer >= g_vk.buffer_count) return;
    if (g_vk.buffers[buffer]) {
        vkDestroyBuffer(g_vk.device, g_vk.buffers[buffer], NULL);
        vkFreeMemory(g_vk.device, g_vk.buffer_memories[buffer], NULL);
        g_vk.buffers[buffer] = VK_NULL_HANDLE;
    }
}

void renderer_bind_buffer(R_BufferTarget target, R_Buffer buffer) {
    if (buffer == R_INVALID_HANDLE) {
        g_vk.bound_vbo = VK_NULL_HANDLE;
        g_vk.bound_vbo_handle = R_INVALID_HANDLE;
        g_vk.bound_index_buffer = VK_NULL_HANDLE;
        g_vk.bound_ibo_handle = R_INVALID_HANDLE;
        return;
    }
    if (buffer >= g_vk.buffer_count) return;

    VkBuffer vk_buffer = g_vk.buffers[buffer];

    if (target == R_BUF_ELEMENT) {
        g_vk.bound_index_buffer = vk_buffer;
        g_vk.bound_ibo_handle = buffer;
        if (g_current_vao != R_INVALID_HANDLE && g_current_vao < MAX_VAO) {
            g_vk.vao_index_buffers[g_current_vao] = vk_buffer;
        }
    } else {
        g_vk.bound_vbo = vk_buffer;
        g_vk.bound_vbo_handle = buffer;
        if (g_current_vao != R_INVALID_HANDLE && g_current_vao < MAX_VAO) {
            g_vk.vao_buffers[g_current_vao] = vk_buffer;
        }
    }
}

void copy_to_buffer(VkBuffer dst, VkDeviceSize dst_offset, VkDeviceSize size, const void *data) {
    assert(size <= g_vk.staging_size && "Staging buffer too small for copy");

    void *mapped;
    if (vkMapMemory(g_vk.device, g_vk.staging_memory, 0, size, 0, &mapped) != VK_SUCCESS) {
        return;
    }
    memcpy(mapped, data, size);
    vkUnmapMemory(g_vk.device, g_vk.staging_memory);

    if (g_active_cmd != VK_NULL_HANDLE && !g_in_render_pass) {
        /* Inside a frame but outside render pass: record copy into current command buffer */
        VkBufferCopy copy = {0};
        copy.srcOffset = 0;
        copy.dstOffset = dst_offset;
        copy.size = size;
        vkCmdCopyBuffer(g_active_cmd, g_vk.staging_buffer, dst, 1, &copy);
    } else {
        /* Init time or inside render pass: one-time command buffer + submit + wait */
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

        VkBufferCopy copy = {0};
        copy.srcOffset = 0;
        copy.dstOffset = dst_offset;
        copy.size = size;
        vkCmdCopyBuffer(cmd, g_vk.staging_buffer, dst, 1, &copy);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit_info = {0};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;

        vkQueueSubmit(g_vk.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(g_vk.graphics_queue);

        vkFreeCommandBuffers(g_vk.device, g_vk.cmd_pool, 1, &cmd);
    }
}

void renderer_buffer_data(R_BufferTarget target, size_t size, const void *data, R_Usage usage) {
    CHECK_DEVICE();
    (void)usage;

    R_Buffer buffer_handle;
    if (target == R_BUF_ELEMENT) {
        buffer_handle = g_vk.bound_ibo_handle;
    } else {
        buffer_handle = g_vk.bound_vbo_handle;
    }

    if (buffer_handle == R_INVALID_HANDLE || buffer_handle >= g_vk.buffer_count) return;

    bool reuse = (g_vk.buffers[buffer_handle] != VK_NULL_HANDLE && size <= g_vk.buffer_sizes[buffer_handle]);
    if (!reuse && g_vk.buffers[buffer_handle]) {
        vkDestroyBuffer(g_vk.device, g_vk.buffers[buffer_handle], NULL);
        vkFreeMemory(g_vk.device, g_vk.buffer_memories[buffer_handle], NULL);
        g_vk.buffers[buffer_handle] = VK_NULL_HANDLE;
        g_vk.buffer_memories[buffer_handle] = VK_NULL_HANDLE;
    }

    VkBufferUsageFlags usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (target == R_BUF_ARRAY) usage_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    else if (target == R_BUF_ELEMENT) usage_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    else if (target == R_BUF_SHADER_STORAGE) usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    else if (target == R_BUF_DRAW_INDIRECT) usage_flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

    VkMemoryPropertyFlags mem_flags = (size > (size_t)1024 * 1024)
        ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    if (!reuse) {
        g_vk.buffers[buffer_handle] = create_buffer(size, usage_flags, mem_flags, &g_vk.buffer_memories[buffer_handle]);
        if (g_vk.buffers[buffer_handle] == VK_NULL_HANDLE) return;
        g_vk.buffer_sizes[buffer_handle] = size;
    }

    /* Upload data */
    if (data && size > 0) {
        if (mem_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            void *mapped;
            VkResult result = vkMapMemory(g_vk.device, g_vk.buffer_memories[buffer_handle], 0, size, 0, &mapped);
            if (result == VK_SUCCESS) {
                memcpy(mapped, data, size);
                vkUnmapMemory(g_vk.device, g_vk.buffer_memories[buffer_handle]);
            }
        } else {
            /* DEVICE_LOCAL: staging buffer + copy command */
            copy_to_buffer(g_vk.buffers[buffer_handle], 0, size, data);
        }
    }
    
    /* Update bound buffer reference */
    if (target == R_BUF_ELEMENT) {
        g_vk.bound_index_buffer = g_vk.buffers[buffer_handle];
        /* Also update VAO tracking if a VAO is bound */
        if (g_current_vao != R_INVALID_HANDLE && g_current_vao < MAX_VAO) {
            g_vk.vao_index_buffers[g_current_vao] = g_vk.buffers[buffer_handle];
        }
    } else {
        g_vk.bound_vbo = g_vk.buffers[buffer_handle];
        /* Also update VAO tracking if a VAO is bound */
        if (g_current_vao != R_INVALID_HANDLE && g_current_vao < MAX_VAO) {
            g_vk.vao_buffers[g_current_vao] = g_vk.buffers[buffer_handle];
        }
    }
}

void renderer_buffer_sub_data(R_BufferTarget target, size_t offset, size_t size, const void *data) {
    (void)target;
    if (!data || size == 0) return;

    R_Buffer buffer_handle = (target == R_BUF_ELEMENT) ? g_vk.bound_ibo_handle : g_vk.bound_vbo_handle;
    if (buffer_handle == R_INVALID_HANDLE || buffer_handle >= g_vk.buffer_count) return;
    if (offset + size > g_vk.buffer_sizes[buffer_handle]) return;

    /* Try direct mapping first (works for host-visible buffers) */
    void *mapped;
    VkResult result = vkMapMemory(g_vk.device, g_vk.buffer_memories[buffer_handle], 0, VK_WHOLE_SIZE, 0, &mapped);
    if (result == VK_SUCCESS) {
        memcpy((char *)mapped + offset, data, size);
        vkUnmapMemory(g_vk.device, g_vk.buffer_memories[buffer_handle]);
    } else {
        /* DEVICE_LOCAL: use staging buffer + copy command */
        copy_to_buffer(g_vk.buffers[buffer_handle], offset, size, data);
    }
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
 * VAO implementation
 * ============================================================================ */


R_VAO renderer_create_vao(void) {
    CHECK_DEVICE_RET(R_INVALID_HANDLE);
    if (g_vk.vao_count >= MAX_VAO) return R_INVALID_HANDLE;
    g_vk.vao_buffers[g_vk.vao_count] = VK_NULL_HANDLE;
    g_vk.vao_index_buffers[g_vk.vao_count] = VK_NULL_HANDLE;
    return g_vk.vao_count++;
}

void renderer_destroy_vao(R_VAO vao) {
    (void)vao;
    /* No Vulkan object to destroy - VAO is purely conceptual */
}

void renderer_bind_vao(R_VAO vao) {
    if (vao == R_INVALID_HANDLE) {
        g_current_vao = R_INVALID_HANDLE;
        g_vao_state.buffer = VK_NULL_HANDLE;
        g_vao_state.index_buffer = VK_NULL_HANDLE;
        return;
    }
    if (vao >= g_vk.vao_count) return;
    
    /* Track current VAO */
    g_current_vao = vao;
    
    /* Restore the buffer associated with this VAO */
    g_vao_state.buffer = g_vk.vao_buffers[vao];
    g_vao_state.index_buffer = g_vk.vao_index_buffers[vao];
}

void renderer_attrib_pointer(int index, int size, R_Type type, bool normalized, int stride, int offset) {
    (void)index;
    (void)size;
    (void)type;
    (void)normalized;
    (void)stride;
    (void)offset;
    /* Vertex format is fixed in the pipeline - this is a no-op for Vulkan */
}

void renderer_enable_attrib(int index) {
    (void)index;
    /* All attributes are always enabled in the pipeline */
}
