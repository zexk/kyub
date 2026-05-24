#define VK_USE_PLATFORM_XLIB_KHR
#include "renderer/renderer_internal.h"
#ifdef ENABLE_VALIDATION
VkDebugUtilsMessengerEXT g_debug_messenger = VK_NULL_HANDLE;
#endif

VulkanContext g_vk = {0};
VkPipelineCache g_pipeline_cache = VK_NULL_HANDLE;

/* ============================================================================
 * Helper functions
 * ============================================================================ */

UniformMapping g_uniforms[32];
int g_uniform_count = 0;
uint8_t g_push_constants[256];
bool g_push_dirty = false;

int g_active_texture_unit = 0;
R_Texture g_bound_textures[16] = {R_INVALID_HANDLE};

VAOState g_vao_state = {0};
R_VAO g_current_vao = R_INVALID_HANDLE;

uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(g_vk.physical_device, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    fprintf(stderr, "Failed to find suitable memory type\n");
    return UINT32_MAX;
}

VkShaderModule create_shader_module(const char *code, size_t size) {
    VkShaderModuleCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = size;
    create_info.pCode = (const uint32_t *)code;
    
    VkShaderModule module;
    if (vkCreateShaderModule(g_vk.device, &create_info, NULL, &module) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create shader module\n");
        return VK_NULL_HANDLE;
    }
    return module;
}

/* ============================================================================
 * Public API: Initialization
 * ============================================================================ */

VkBuffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkDeviceMemory *out_memory);
VkImage create_image(uint32_t width, uint32_t height, uint32_t depth, uint32_t array_layers, VkFormat format, VkImageUsageFlags usage, VkDeviceMemory *out_memory);

bool renderer_init(int width, int height) {
    g_vk.width = width;
    g_vk.height = height;

    if (!create_instance()) {
        fprintf(stderr, "Failed to create instance\n");
        return false;
    }
    if (!select_physical_device()) {
        fprintf(stderr, "Failed to select physical device\n");
        vkDestroyInstance(g_vk.instance, NULL);
        return false;
    }
    if (!create_surface()) {
        fprintf(stderr, "Failed to create surface\n");
        vkDestroyInstance(g_vk.instance, NULL);
        return false;
    }
    if (!find_queue_families()) {
        fprintf(stderr, "Failed to find queue families\n");
        vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, NULL);
        vkDestroyInstance(g_vk.instance, NULL);
        return false;
    }
    if (!create_device()) {
        fprintf(stderr, "Failed to create device\n");
        vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, NULL);
        vkDestroyInstance(g_vk.instance, NULL);
        return false;
    }

    g_vk.present_mode = VK_PRESENT_MODE_FIFO_KHR;

    if (!create_swapchain()) {
        fprintf(stderr, "Failed to create swapchain\n");
        vkDestroyDevice(g_vk.device, NULL);
        vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, NULL);
        vkDestroyInstance(g_vk.instance, NULL);
        return false;
    }
    if (!create_swap_image_views()) {
        fprintf(stderr, "Failed to create swap image views\n");
        free(g_vk.swap_images);
        vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, NULL);
        vkDestroyDevice(g_vk.device, NULL);
        vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, NULL);
        vkDestroyInstance(g_vk.instance, NULL);
        return false;
    }

    if (g_vk.swap_extent.width == 0 || g_vk.swap_extent.height == 0) {
        fprintf(stderr, "Invalid swap extent: %ux%u\n", g_vk.swap_extent.width, g_vk.swap_extent.height);
        free(g_vk.swap_views);
        free(g_vk.swap_images);
        vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, NULL);
        vkDestroyDevice(g_vk.device, NULL);
        vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, NULL);
        vkDestroyInstance(g_vk.instance, NULL);
        return false;
    }

    if (!create_depth_buffer()) {
        fprintf(stderr, "Failed to create depth buffer\n");
        free(g_vk.swap_views);
        free(g_vk.swap_images);
        vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, NULL);
        vkDestroyDevice(g_vk.device, NULL);
        vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, NULL);
        vkDestroyInstance(g_vk.instance, NULL);
        return false;
    }
    if (!create_command_pool()) {
        fprintf(stderr, "Failed to create command pool\n");
        vkDestroyImageView(g_vk.device, g_vk.depth_view, NULL);
        vkDestroyImage(g_vk.device, g_vk.depth_image, NULL);
        vkFreeMemory(g_vk.device, g_vk.depth_memory, NULL);
        free(g_vk.swap_views);
        free(g_vk.swap_images);
        vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, NULL);
        vkDestroyDevice(g_vk.device, NULL);
        vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, NULL);
        vkDestroyInstance(g_vk.instance, NULL);
        return false;
    }
    if (!create_command_buffers()) {
        fprintf(stderr, "Failed to create command buffers\n");
        vkDestroyCommandPool(g_vk.device, g_vk.cmd_pool, NULL);
        vkDestroyImageView(g_vk.device, g_vk.depth_view, NULL);
        vkDestroyImage(g_vk.device, g_vk.depth_image, NULL);
        vkFreeMemory(g_vk.device, g_vk.depth_memory, NULL);
        free(g_vk.swap_views);
        free(g_vk.swap_images);
        vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, NULL);
        vkDestroyDevice(g_vk.device, NULL);
        vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, NULL);
        vkDestroyInstance(g_vk.instance, NULL);
        return false;
    }
    if (!create_sync_objects()) {
        fprintf(stderr, "Failed to create sync objects\n");
        vkDestroyCommandPool(g_vk.device, g_vk.cmd_pool, NULL);
        vkDestroyImageView(g_vk.device, g_vk.depth_view, NULL);
        vkDestroyImage(g_vk.device, g_vk.depth_image, NULL);
        vkFreeMemory(g_vk.device, g_vk.depth_memory, NULL);
        free(g_vk.swap_views);
        free(g_vk.swap_images);
        vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, NULL);
        vkDestroyDevice(g_vk.device, NULL);
        vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, NULL);
        vkDestroyInstance(g_vk.instance, NULL);
        return false;
    }
    if (!create_descriptor_pool()) {
        fprintf(stderr, "Failed to create descriptor pool\n");
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(g_vk.device, g_vk.image_avail_sems[i], NULL);
            vkDestroySemaphore(g_vk.device, g_vk.render_done_sems[i], NULL);
            vkDestroyFence(g_vk.device, g_vk.in_flight_fences[i], NULL);
        }
        free(g_vk.image_avail_sems);
        free(g_vk.render_done_sems);
        free(g_vk.in_flight_fences);
        vkDestroyCommandPool(g_vk.device, g_vk.cmd_pool, NULL);
        vkDestroyImageView(g_vk.device, g_vk.depth_view, NULL);
        vkDestroyImage(g_vk.device, g_vk.depth_image, NULL);
        vkFreeMemory(g_vk.device, g_vk.depth_memory, NULL);
        free(g_vk.swap_views);
        free(g_vk.swap_images);
        vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, NULL);
        vkDestroyDevice(g_vk.device, NULL);
        vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, NULL);
        vkDestroyInstance(g_vk.instance, NULL);
        return false;
    }
    if (!create_default_sampler()) {
        fprintf(stderr, "Failed to create default sampler\n");
        vkDestroyDescriptorPool(g_vk.device, g_vk.desc_pool, NULL);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(g_vk.device, g_vk.image_avail_sems[i], NULL);
            vkDestroySemaphore(g_vk.device, g_vk.render_done_sems[i], NULL);
            vkDestroyFence(g_vk.device, g_vk.in_flight_fences[i], NULL);
        }
        free(g_vk.image_avail_sems);
        free(g_vk.render_done_sems);
        free(g_vk.in_flight_fences);
        vkDestroyCommandPool(g_vk.device, g_vk.cmd_pool, NULL);
        vkDestroyImageView(g_vk.device, g_vk.depth_view, NULL);
        vkDestroyImage(g_vk.device, g_vk.depth_image, NULL);
        vkFreeMemory(g_vk.device, g_vk.depth_memory, NULL);
        free(g_vk.swap_views);
        free(g_vk.swap_images);
        vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, NULL);
        vkDestroyDevice(g_vk.device, NULL);
        vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, NULL);
        vkDestroyInstance(g_vk.instance, NULL);
        return false;
    }

    init_resource_arrays();

    g_vk.staging_size = 16 * 1024 * 1024; /* 16MB persistent staging buffer */
    g_vk.staging_buffer = create_buffer(g_vk.staging_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         &g_vk.staging_memory);
    if (g_vk.staging_buffer == VK_NULL_HANDLE) {
        fprintf(stderr, "Failed to create persistent staging buffer\n");
        renderer_shutdown();
        return false;
    }

    g_vk.cull_mode = VK_CULL_MODE_BACK_BIT;
    g_vk.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    g_vk.line_width = 1.0f;
    g_vk.poly_offset_factor = 0.0f;
    g_vk.poly_offset_units = 0.0f;

    printf("Vulkan renderer initialized\n");
    return true;
}

void renderer_shutdown(void) {
    if (g_vk.device == VK_NULL_HANDLE) return;
    
    vkDeviceWaitIdle(g_vk.device);
    
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(g_vk.device, g_vk.image_avail_sems[i], NULL);
        vkDestroySemaphore(g_vk.device, g_vk.render_done_sems[i], NULL);
        vkDestroyFence(g_vk.device, g_vk.in_flight_fences[i], NULL);
    }
    
    vkDestroyCommandPool(g_vk.device, g_vk.cmd_pool, NULL);
    vkDestroyDescriptorPool(g_vk.device, g_vk.desc_pool, NULL);
    
    for (uint32_t i = 0; i < g_vk.swap_image_count; i++) {
        vkDestroyImageView(g_vk.device, g_vk.swap_views[i], NULL);
    }

    vkDestroyImageView(g_vk.device, g_vk.depth_view, NULL);
    vkDestroyImage(g_vk.device, g_vk.depth_image, NULL);
    vkFreeMemory(g_vk.device, g_vk.depth_memory, NULL);

    vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, NULL);
    vkDestroyPipelineCache(g_vk.device, g_pipeline_cache, NULL);

    vkDestroySampler(g_vk.device, g_vk.default_sampler, NULL);

    if (g_vk.staging_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_vk.device, g_vk.staging_buffer, NULL);
        vkFreeMemory(g_vk.device, g_vk.staging_memory, NULL);
    }

    for (uint32_t i = 0; i < g_vk.pipeline_count; i++) {
        if (g_vk.pipelines[i].pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(g_vk.device, g_vk.pipelines[i].pipeline, NULL);
        }
        if (g_vk.pipelines[i].layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(g_vk.device, g_vk.pipelines[i].layout, NULL);
        }
        if (g_vk.pipelines[i].desc_set_layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(g_vk.device, g_vk.pipelines[i].desc_set_layout, NULL);
        }
    }
    for (uint32_t i = 0; i < g_vk.buffer_count; i++) {
        if (g_vk.buffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(g_vk.device, g_vk.buffers[i], NULL);
            vkFreeMemory(g_vk.device, g_vk.buffer_memories[i], NULL);
        }
    }
    for (uint32_t i = 0; i < g_vk.texture_count; i++) {
        if (g_vk.texture_views[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(g_vk.device, g_vk.texture_views[i], NULL);
        }
        if (g_vk.textures[i] != VK_NULL_HANDLE) {
            vkDestroyImage(g_vk.device, g_vk.textures[i], NULL);
            vkFreeMemory(g_vk.device, g_vk.texture_memories[i], NULL);
        }
        if (g_vk.texture_samplers[i] != g_vk.default_sampler &&
            g_vk.texture_samplers[i] != VK_NULL_HANDLE) {
            vkDestroySampler(g_vk.device, g_vk.texture_samplers[i], NULL);
        }
    }
    for (uint32_t i = 0; i < g_vk.vao_count; i++) {
        if (g_vk.vaos[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(g_vk.device, g_vk.vaos[i], NULL);
        }
        if (g_vk.vao_index_buffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(g_vk.device, g_vk.vao_index_buffers[i], NULL);
        }
    }

    free(g_vk.swap_images);
    free(g_vk.swap_views);
    free(g_vk.pipelines);
    free(g_vk.buffers);
    free(g_vk.buffer_memories);
    free(g_vk.buffer_sizes);
    free(g_vk.textures);
    free(g_vk.texture_memories);
    free(g_vk.texture_views);
    free(g_vk.texture_samplers);
    free(g_vk.texture_widths);
    free(g_vk.texture_heights);
    free(g_vk.texture_depths);
    free(g_vk.vaos);

    vkDestroyDevice(g_vk.device, NULL);
    vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, NULL);

#ifdef ENABLE_VALIDATION
    if (g_debug_messenger != VK_NULL_HANDLE) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_fn =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                g_vk.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroy_fn) {
            destroy_fn(g_vk.instance, g_debug_messenger, NULL);
        }
    }
#endif

    vkDestroyInstance(g_vk.instance, NULL);
}

/* ============================================================================
 * Frame recording state
 * ============================================================================ */

VkCommandBuffer g_active_cmd = VK_NULL_HANDLE;
bool g_frame_started = false;
bool g_in_render_pass = false;
float g_clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
bool g_clear_depth = false;

/* ============================================================================
 * Public API: Frame control
 * ============================================================================ */

void renderer_clear(float r, float g, float b, float a) {
    CHECK_DEVICE();
    g_clear_color[0] = r;
    g_clear_color[1] = g;
    g_clear_color[2] = b;
    g_clear_color[3] = a;
    g_clear_depth = true;
    
    /* In Vulkan, clear also begins the frame recording */
    if (g_frame_started) return; /* Already started */
    
    /* Handle resize if needed */
    if (g_vk.framebuffer_resized) {
        g_vk.framebuffer_resized = false;
        recreate_swapchain();
    }

    /* Wait for previous frame with timeout to avoid indefinite hangs */
    VkResult fence_result = vkWaitForFences(g_vk.device, 1, &g_vk.in_flight_fences[g_vk.current_frame], VK_TRUE, FENCE_TIMEOUT_NS);
    if (fence_result == VK_TIMEOUT) {
        fprintf(stderr, "Warning: fence wait timed out, recreating swapchain\n");
        vkResetFences(g_vk.device, 1, &g_vk.in_flight_fences[g_vk.current_frame]);
        recreate_swapchain();
        vkResetFences(g_vk.device, 1, &g_vk.in_flight_fences[g_vk.current_frame]);
    }

    VkResult result = vkAcquireNextImageKHR(g_vk.device, g_vk.swapchain, FENCE_TIMEOUT_NS,
                                            g_vk.image_avail_sems[g_vk.current_frame], VK_NULL_HANDLE,
                                            &g_vk.image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        if (recreate_swapchain()) {
            result = vkAcquireNextImageKHR(g_vk.device, g_vk.swapchain, UINT64_MAX,
                                           g_vk.image_avail_sems[g_vk.current_frame],
                                           VK_NULL_HANDLE, &g_vk.image_index);
        }
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_TIMEOUT) return;
    } else if (result == VK_TIMEOUT) {
        vkResetFences(g_vk.device, 1, &g_vk.in_flight_fences[g_vk.current_frame]);
        recreate_swapchain();
        result = vkAcquireNextImageKHR(g_vk.device, g_vk.swapchain, UINT64_MAX,
                                       g_vk.image_avail_sems[g_vk.current_frame],
                                       VK_NULL_HANDLE, &g_vk.image_index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_TIMEOUT) return;
    }
    // VK_SUCCESS and VK_SUBOPTIMAL_KHR both allow proceeding
    
    if (g_vk.image_index >= g_vk.swap_image_count) {
        fprintf(stderr, "Invalid image index %u >= %u\n", g_vk.image_index, g_vk.swap_image_count);
        return;
    }

    /* Reset and begin command buffer */
    VkCommandBuffer cmd = g_vk.cmd_buffers[g_vk.current_frame];
    vkResetCommandBuffer(cmd, 0);
    
    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);
    
    g_active_cmd = cmd;
    g_frame_started = true;
    
    /* Begin dynamic rendering */
    VkRenderingAttachmentInfo color_attach = {0};
    color_attach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attach.imageView = g_vk.swap_views[g_vk.image_index];
    color_attach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attach.clearValue.color.float32[0] = g_clear_color[0];
    color_attach.clearValue.color.float32[1] = g_clear_color[1];
    color_attach.clearValue.color.float32[2] = g_clear_color[2];
    color_attach.clearValue.color.float32[3] = g_clear_color[3];

    VkRenderingAttachmentInfo depth_attach = {0};
    depth_attach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth_attach.imageView = g_vk.depth_view;
    depth_attach.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth_attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attach.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attach.clearValue.depthStencil.depth = 1.0f;
    depth_attach.clearValue.depthStencil.stencil = 0;

    VkRenderingInfo rendering_info = {0};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea.offset.x = 0;
    rendering_info.renderArea.offset.y = 0;
    rendering_info.renderArea.extent = g_vk.swap_extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attach;
    rendering_info.pDepthAttachment = &depth_attach;

    vkCmdBeginRendering(cmd, &rendering_info);
    g_in_render_pass = true;
}

void renderer_swap(void) {
    CHECK_DEVICE();
    if (!g_frame_started || g_active_cmd == VK_NULL_HANDLE) return;
    
    /* End dynamic rendering */
    vkCmdEndRendering(g_active_cmd);
    g_in_render_pass = false;
    
    /* End command buffer */
    vkEndCommandBuffer(g_active_cmd);
    
    /* Submit command buffer */
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &g_vk.image_avail_sems[g_vk.current_frame];
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &g_vk.cmd_buffers[g_vk.current_frame];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &g_vk.render_done_sems[g_vk.current_frame];
    
    vkResetFences(g_vk.device, 1, &g_vk.in_flight_fences[g_vk.current_frame]);
    VkResult submit_result = vkQueueSubmit(g_vk.graphics_queue, 1, &submit_info, g_vk.in_flight_fences[g_vk.current_frame]);
    if (submit_result == VK_ERROR_DEVICE_LOST) {
        fprintf(stderr, "Error: VK_ERROR_DEVICE_LOST during submit, attempting recovery\n");
        recreate_swapchain();
        g_active_cmd = VK_NULL_HANDLE;
        g_frame_started = false;
        return;
    }
    
    /* Present */
    VkPresentInfoKHR present_info = {0};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &g_vk.render_done_sems[g_vk.current_frame];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &g_vk.swapchain;
    present_info.pImageIndices = &g_vk.image_index;
    
    VkResult result = vkQueuePresentKHR(g_vk.present_queue, &present_info);

    if (result == VK_ERROR_DEVICE_LOST) {
        fprintf(stderr, "Error: VK_ERROR_DEVICE_LOST during present, attempting recovery\n");
        recreate_swapchain();
    } else if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || g_vk.framebuffer_resized) {
        g_vk.framebuffer_resized = false;
        recreate_swapchain();
    }
    
    g_vk.current_frame = (g_vk.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    g_active_cmd = VK_NULL_HANDLE;
    g_frame_started = false;
}

void renderer_swap_interval(int interval) {
    VkPresentModeKHR new_mode = (interval == 0)
        ? VK_PRESENT_MODE_MAILBOX_KHR
        : VK_PRESENT_MODE_FIFO_KHR;

    if (g_vk.present_mode == new_mode) return;

    g_vk.present_mode = new_mode;
    recreate_swapchain();
}

void renderer_clear_depth(void) {
    /* Depth is cleared in renderer_clear() - this is a no-op for Vulkan */
}

void renderer_get_size(int *width, int *height) {
    *width = g_vk.width;
    *height = g_vk.height;
}

void renderer_viewport(int x, int y, int width, int height) {
    (void)x;
    (void)y;
    if (g_vk.width != width || g_vk.height != height) {
        g_vk.width = width;
        g_vk.height = height;
        g_vk.framebuffer_resized = true;
    }
}

/* ============================================================================
 * Dynamic state
 * ============================================================================ */

void renderer_enable(R_Cap cap) {
    (void)cap;
    /* Pipeline state is baked at creation time; runtime changes have no effect */
}

void renderer_disable(R_Cap cap) {
    (void)cap;
    /* Pipeline state is baked at creation time; runtime changes have no effect */
}

void renderer_depth_mask(bool write) {
    (void)write;
    /* Pipeline state is baked at creation time; runtime changes have no effect */
}

void renderer_depth_func(R_DepthFunc func) {
    (void)func;
    /* Pipeline state is baked at creation time; runtime changes have no effect */
}

void renderer_blend_func(R_BlendFactor src, R_BlendFactor dst) {
    (void)src;
    (void)dst;
    /* Pipeline state is baked at creation time; runtime changes have no effect */
}

void renderer_polygon_offset(float factor, float units) {
    g_vk.poly_offset_factor = factor;
    g_vk.poly_offset_units = units;
}

void renderer_line_width(float width) {
    g_vk.line_width = width;
}

void renderer_push_attrib(void) {
    /* Pipeline state is baked at creation time; no-op for Vulkan */
}

void renderer_pop_attrib(void) {
    /* Pipeline state is baked at creation time; no-op for Vulkan */
}
