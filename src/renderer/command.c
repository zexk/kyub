#include "renderer/renderer_internal.h"
bool create_command_pool(void) {
    VkCommandPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = g_vk.graphics_family;
    
    VkResult res = vkCreateCommandPool(g_vk.device, &pool_info, NULL, &g_vk.cmd_pool);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "Failed to create command pool\n");
        return false;
    }
    return true;
}

bool create_command_buffers(void) {
    g_vk.cmd_buffers = malloc(sizeof(VkCommandBuffer) * MAX_FRAMES_IN_FLIGHT);
    if (!g_vk.cmd_buffers) {
        fprintf(stderr, "Failed to allocate command buffers\n");
        return false;
    }
    
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = g_vk.cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    
    VkResult res = vkAllocateCommandBuffers(g_vk.device, &alloc_info, g_vk.cmd_buffers);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "Failed to allocate command buffers\n");
        free(g_vk.cmd_buffers);
        return false;
    }
    return true;
}

bool create_sync_objects(void) {
    g_vk.image_avail_sems = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    g_vk.render_done_sems = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    g_vk.in_flight_fences = malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);
    
    if (!g_vk.image_avail_sems || !g_vk.render_done_sems || !g_vk.in_flight_fences) {
        fprintf(stderr, "Failed to allocate sync objects\n");
        free(g_vk.image_avail_sems);
        free(g_vk.render_done_sems);
        free(g_vk.in_flight_fences);
        return false;
    }
    
    VkSemaphoreCreateInfo sem_info = {0};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fence_info = {0};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(g_vk.device, &sem_info, NULL, &g_vk.image_avail_sems[i]) != VK_SUCCESS ||
            vkCreateSemaphore(g_vk.device, &sem_info, NULL, &g_vk.render_done_sems[i]) != VK_SUCCESS ||
            vkCreateFence(g_vk.device, &fence_info, NULL, &g_vk.in_flight_fences[i]) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create sync object %d\n", i);
            for (int j = 0; j < i; j++) {
                vkDestroyFence(g_vk.device, g_vk.in_flight_fences[j], NULL);
                vkDestroySemaphore(g_vk.device, g_vk.image_avail_sems[j], NULL);
                vkDestroySemaphore(g_vk.device, g_vk.render_done_sems[j], NULL);
            }
            return false;
        }
    }
    return true;
}
