#include "renderer/renderer_internal.h"
/* ============================================================================
 * Public API: Drawing
 * ============================================================================ */

static void setup_dynamic_state(void) {
    VkViewport viewport = {0};
    viewport.x = 0;
    viewport.y = (float)g_vk.swap_extent.height;
    viewport.width = (float)g_vk.swap_extent.width;
    viewport.height = -(float)g_vk.swap_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(g_active_cmd, 0, 1, &viewport);

    VkRect2D scissor = {0};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = g_vk.swap_extent;
    vkCmdSetScissor(g_active_cmd, 0, 1, &scissor);

    vkCmdSetLineWidth(g_active_cmd, g_vk.line_width);
    if (g_vk.poly_offset_factor != 0.0f || g_vk.poly_offset_units != 0.0f) {
        vkCmdSetDepthBias(g_active_cmd, g_vk.poly_offset_factor, 0.0f, 0.0f);
    } else {
        vkCmdSetDepthBias(g_active_cmd, 0.0f, 0.0f, 0.0f);
    }
}

void renderer_draw_arrays(R_Primitive primitive, int first, int count) {
    CHECK_DEVICE();
    assert(g_active_cmd != VK_NULL_HANDLE && "renderer_begin not called");
    assert(g_vk.active_pipeline < g_vk.pipeline_count && "no active pipeline");
    assert((g_vk.bound_vbo != VK_NULL_HANDLE || g_vao_state.buffer != VK_NULL_HANDLE)
           && "no vertex buffer bound");

    Pipeline *pipe = &g_vk.pipelines[g_vk.active_pipeline];
    
    /* Bind pipeline */
    vkCmdBindPipeline(g_active_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->pipeline);
    
    /* Bind descriptor set for texture (only if the pipeline has one) */
    if (pipe->desc_set != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(g_active_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->layout, 
                                0, 1, &pipe->desc_set, 0, NULL);
    }
    
    /* Push constants */
    if (g_push_dirty) {
        vkCmdPushConstants(g_active_cmd, pipe->layout, 
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, R_PUSH_CONSTANT_SIZE, g_push_constants);
        g_push_dirty = false;
    }
    
    /* Bind vertex buffer */
    VkBuffer vbo = g_vk.bound_vbo;
    if (vbo == VK_NULL_HANDLE && g_vao_state.buffer != VK_NULL_HANDLE) {
        vbo = g_vao_state.buffer;
    }
    if (vbo != VK_NULL_HANDLE) {
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(g_active_cmd, 0, 1, &vbo, &offset);
    }

    setup_dynamic_state();

    /* Draw */
    (void)primitive; /* Pipeline already has the topology set */
    vkCmdDraw(g_active_cmd, count, 1, first, 0);
}

void renderer_draw_elements(R_Primitive primitive, int count, int offset) {
    CHECK_DEVICE();
    assert(g_active_cmd != VK_NULL_HANDLE && "renderer_begin not called");
    assert(g_vk.active_pipeline < g_vk.pipeline_count && "no active pipeline");
    assert(g_vk.bound_index_buffer != VK_NULL_HANDLE && "no index buffer bound");

    Pipeline *pipe = &g_vk.pipelines[g_vk.active_pipeline];
    
    /* Bind pipeline */
    vkCmdBindPipeline(g_active_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->pipeline);
    
    /* Bind descriptor set for texture (only if the pipeline has one) */
    if (pipe->desc_set != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(g_active_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->layout, 
                                0, 1, &pipe->desc_set, 0, NULL);
    }
    
    /* Push constants */
    if (g_push_dirty) {
        vkCmdPushConstants(g_active_cmd, pipe->layout, 
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, R_PUSH_CONSTANT_SIZE, g_push_constants);
        g_push_dirty = false;
    }
    
    /* Bind vertex buffer */
    VkBuffer vbo = g_vk.bound_vbo;
    if (vbo == VK_NULL_HANDLE && g_vao_state.buffer != VK_NULL_HANDLE) {
        vbo = g_vao_state.buffer;
    }
    if (vbo != VK_NULL_HANDLE) {
        VkDeviceSize vbo_offset = 0;
        vkCmdBindVertexBuffers(g_active_cmd, 0, 1, &vbo, &vbo_offset);
    }
    
    /* Bind index buffer */
    vkCmdBindIndexBuffer(g_active_cmd, g_vk.bound_index_buffer, 0, VK_INDEX_TYPE_UINT16);

    setup_dynamic_state();

    /* Draw indexed */
    (void)primitive; /* Pipeline already has the topology set */
    vkCmdDrawIndexed(g_active_cmd, count, 1, offset, 0, 0);
}

void renderer_draw_arrays_indirect(void) {
}

/* ============================================================================
 * Public API: Compute (stubbed)
 * ============================================================================ */

void renderer_dispatch_compute(int groups_x, int groups_y, int groups_z) {
    (void)groups_x;
    (void)groups_y;
    (void)groups_z;
}

void renderer_memory_barrier(R_BarrierBits bits) {
    (void)bits;
}
