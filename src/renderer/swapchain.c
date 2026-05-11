#include "renderer/renderer_internal.h"
bool create_swapchain(void) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vk.physical_device, g_vk.surface, &caps);
    
    g_vk.swap_extent = caps.currentExtent;
    if (g_vk.swap_extent.width == UINT32_MAX) {
        g_vk.swap_extent.width = (uint32_t)g_vk.width;
        g_vk.swap_extent.height = (uint32_t)g_vk.height;
    }
    
    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_vk.physical_device, g_vk.surface, &format_count, NULL);
    VkSurfaceFormatKHR *formats = malloc(sizeof(VkSurfaceFormatKHR) * format_count);
    if (!formats) {
        fprintf(stderr, "Failed to allocate surface formats\n");
        return false;
    }
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_vk.physical_device, g_vk.surface, &format_count, formats);
    
    g_vk.swap_format = formats[0].format;
    for (uint32_t i = 0; i < format_count; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB) {
            g_vk.swap_format = formats[i].format;
            break;
        }
    }
    free(formats);
    
    uint32_t mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_vk.physical_device, g_vk.surface, &mode_count, NULL);
    VkPresentModeKHR *modes = malloc(sizeof(VkPresentModeKHR) * mode_count);
    if (!modes) {
        fprintf(stderr, "Failed to allocate present modes\n");
        return false;
    }
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_vk.physical_device, g_vk.surface, &mode_count, modes);
    
    VkPresentModeKHR desired_mode = g_vk.present_mode;
    bool mode_available = false;
    for (uint32_t i = 0; i < mode_count; i++) {
        if (modes[i] == desired_mode) {
            mode_available = true;
            break;
        }
    }
    if (!mode_available) {
        /* Fall back to FIFO which is guaranteed to be supported */
        desired_mode = VK_PRESENT_MODE_FIFO_KHR;
    }
    free(modes);

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }
    g_vk.swap_image_count = image_count;

    VkSwapchainCreateInfoKHR create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = g_vk.surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = g_vk.swap_format;
    create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    create_info.imageExtent = g_vk.swap_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = caps.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = desired_mode;
    create_info.clipped = VK_TRUE;
    
    if (vkCreateSwapchainKHR(g_vk.device, &create_info, NULL, &g_vk.swapchain) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create swapchain\n");
        return false;
    }
    
    vkGetSwapchainImagesKHR(g_vk.device, g_vk.swapchain, &g_vk.swap_image_count, NULL);
    g_vk.swap_images = malloc(sizeof(VkImage) * g_vk.swap_image_count);
    if (!g_vk.swap_images) {
        fprintf(stderr, "Failed to allocate swap images\n");
        return false;
    }
    vkGetSwapchainImagesKHR(g_vk.device, g_vk.swapchain, &g_vk.swap_image_count, g_vk.swap_images);
    
    return true;
}

bool create_swap_image_views(void) {
    g_vk.swap_views = malloc(sizeof(VkImageView) * g_vk.swap_image_count);
    if (!g_vk.swap_views) {
        fprintf(stderr, "Failed to allocate swap image views\n");
        return false;
    }
    
    for (uint32_t i = 0; i < g_vk.swap_image_count; i++) {
        VkImageViewCreateInfo create_info = {0};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = g_vk.swap_images[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = g_vk.swap_format;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;
        
        VkResult res = vkCreateImageView(g_vk.device, &create_info, NULL, &g_vk.swap_views[i]);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "Failed to create swap image view %u\n", i);
            return false;
        }
    }
    return true;
}

bool create_depth_buffer(void) {
    VkImageCreateInfo image_info = {0};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = g_vk.swap_extent.width;
    image_info.extent.height = g_vk.swap_extent.height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = VK_FORMAT_D32_SFLOAT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkResult res = vkCreateImage(g_vk.device, &image_info, NULL, &g_vk.depth_image);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "Failed to create depth image\n");
        return false;
    }
    
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(g_vk.device, g_vk.depth_image, &mem_reqs);
    
    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    uint32_t mem_type = find_memory_type(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mem_type == UINT32_MAX) {
        vkDestroyImage(g_vk.device, g_vk.depth_image, NULL);
        return false;
    }
    alloc_info.memoryTypeIndex = mem_type;

    res = vkAllocateMemory(g_vk.device, &alloc_info, NULL, &g_vk.depth_memory);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "Failed to allocate depth memory\n");
        vkDestroyImage(g_vk.device, g_vk.depth_image, NULL);
        return false;
    }
    vkBindImageMemory(g_vk.device, g_vk.depth_image, g_vk.depth_memory, 0);
    
    VkImageViewCreateInfo view_info = {0};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = g_vk.depth_image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_D32_SFLOAT;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    
    res = vkCreateImageView(g_vk.device, &view_info, NULL, &g_vk.depth_view);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "Failed to create depth image view\n");
        vkFreeMemory(g_vk.device, g_vk.depth_memory, NULL);
        vkDestroyImage(g_vk.device, g_vk.depth_image, NULL);
        return false;
    }
    return true;
}

void cleanup_swapchain(void) {
    vkDeviceWaitIdle(g_vk.device);

    if (g_vk.depth_view) {
        vkDestroyImageView(g_vk.device, g_vk.depth_view, NULL);
        g_vk.depth_view = VK_NULL_HANDLE;
    }
    if (g_vk.depth_image) {
        vkDestroyImage(g_vk.device, g_vk.depth_image, NULL);
        g_vk.depth_image = VK_NULL_HANDLE;
    }
    if (g_vk.depth_memory) {
        vkFreeMemory(g_vk.device, g_vk.depth_memory, NULL);
        g_vk.depth_memory = VK_NULL_HANDLE;
    }

    if (g_vk.swap_views) {
        for (uint32_t i = 0; i < g_vk.swap_image_count; i++) {
            if (g_vk.swap_views[i]) {
                vkDestroyImageView(g_vk.device, g_vk.swap_views[i], NULL);
            }
        }
        free(g_vk.swap_views);
        g_vk.swap_views = NULL;
    }

    if (g_vk.swapchain) {
        vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, NULL);
        g_vk.swapchain = VK_NULL_HANDLE;
    }

    if (g_vk.swap_images) {
        free(g_vk.swap_images);
        g_vk.swap_images = NULL;
    }
}

bool recreate_swapchain(void) {
    vkDeviceWaitIdle(g_vk.device);

    cleanup_swapchain();

    if (!create_swapchain()) return false;
    if (!create_swap_image_views()) return false;
    if (!create_depth_buffer()) return false;

    return true;
}
