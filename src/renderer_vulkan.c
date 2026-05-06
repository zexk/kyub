#define VK_USE_PLATFORM_XLIB_KHR
#include "renderer.h"
#include "platform.h"
#include "platform_x11.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <X11/Xlib.h>

/* Maximum number of frames in flight */
#define MAX_FRAMES_IN_FLIGHT 2

/* Maximum resources */
#define MAX_PIPELINES 16
#define MAX_BUFFERS 256
#define MAX_TEXTURES 256
#define MAX_VAO 256

/* Pipeline state for dynamic state */
typedef struct {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkDescriptorSetLayout desc_set_layout;
    VkDescriptorSet desc_set;
    VkShaderModule vert_module;
    VkShaderModule frag_module;
} Pipeline;

/* Vulkan context state */
typedef struct {
    /* Instance and physical device */
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    
    /* Queues */
    VkQueue graphics_queue;
    VkQueue compute_queue;
    VkQueue present_queue;
    uint32_t graphics_family;
    uint32_t compute_family;
    uint32_t present_family;
    
    /* Surface and swapchain */
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkExtent2D swap_extent;
    VkFormat swap_format;
    uint32_t swap_image_count;
    VkImage *swap_images;
    VkImageView *swap_views;
    VkFramebuffer *framebuffers;
    VkImage depth_image;
    VkDeviceMemory depth_memory;
    VkImageView depth_view;
    
    /* Render pass */
    VkRenderPass render_pass;
    
    /* Command buffers and pools */
    VkCommandPool cmd_pool;
    VkCommandPool transient_pool;
    VkCommandBuffer *cmd_buffers;
    
    /* Synchronization */
    VkSemaphore *image_avail_sems;
    VkSemaphore *render_done_sems;
    VkFence *in_flight_fences;
    
    /* Current frame state */
    uint32_t current_frame;
    uint32_t image_index;
    bool framebuffer_resized;
    int width, height;
    
    /* Descriptor pool */
    VkDescriptorPool desc_pool;
    
    /* Pipelines */
    Pipeline *pipelines;
    uint32_t pipeline_count;
    uint32_t active_pipeline;
    
    /* Resource tracking */
    VkBuffer *buffers;
    VkDeviceMemory *buffer_memories;
    uint64_t *buffer_sizes;
    uint32_t buffer_count;
    
    VkImage *textures;
    VkDeviceMemory *texture_memories;
    VkImageView *texture_views;
    VkSampler *texture_samplers;
    uint32_t texture_count;
    VkSampler default_sampler;
    
    VkBuffer *vaos;
    uint32_t vao_count;
    
    /* VAO buffer tracking - stores which buffer is bound to each VAO */
    VkBuffer vao_buffers[MAX_VAO];
    VkBuffer vao_index_buffers[MAX_VAO];
    
    /* Dynamic state cache */
    VkCullModeFlagBits cull_mode;
    VkBool32 depth_test;
    VkBool32 depth_write;
    VkCompareOp depth_op;
    VkBool32 blend_enable;
    VkBlendFactor blend_src;
    VkBlendFactor blend_dst;
    VkPrimitiveTopology topology;
    float line_width;
    float poly_offset_factor;
    float poly_offset_units;
    
    /* Active bindings */
    VkBuffer bound_vbo;
    VkBuffer bound_index_buffer;
    VkPipeline active_pipeline_handle;
} VulkanContext;

static VulkanContext g_vk = {0};

/* ============================================================================
 * Helper functions
 * ============================================================================ */

static uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(g_vk.physical_device, &mem_props);
    
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    fprintf(stderr, "Failed to find suitable memory type\n");
    return 0;
}

static VkShaderModule create_shader_module(const char *code, size_t size) {
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

static VkPrimitiveTopology prim_to_vk(R_Primitive prim) {
    switch (prim) {
        case R_PRIM_TRIANGLES: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case R_PRIM_LINES: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case R_PRIM_TRIANGLE_FAN: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        default: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

static char* load_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size);
    if (buf) {
        fread(buf, 1, size, f);
    }
    fclose(f);
    return buf;
}

/* ============================================================================
 * Instance and device creation
 * ============================================================================ */

static bool create_instance(void) {
    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Kyub";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "KyubEngine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;
    
    const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
    };
    
    VkInstanceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
    create_info.ppEnabledExtensionNames = extensions;
    
    if (vkCreateInstance(&create_info, NULL, &g_vk.instance) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance\n");
        return false;
    }
    return true;
}

static bool select_physical_device(void) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_vk.instance, &device_count, NULL);
    if (device_count == 0) {
        fprintf(stderr, "No Vulkan-compatible devices found\n");
        return false;
    }
    
    VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(g_vk.instance, &device_count, devices);
    
    g_vk.physical_device = devices[0];
    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            g_vk.physical_device = devices[i];
            printf("Using GPU: %s\n", props.deviceName);
            break;
        }
    }
    
    free(devices);
    return true;
}

static bool find_queue_families(void) {
    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_vk.physical_device, &queue_count, NULL);
    
    VkQueueFamilyProperties *queues = malloc(sizeof(VkQueueFamilyProperties) * queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_vk.physical_device, &queue_count, queues);
    
    g_vk.graphics_family = UINT32_MAX;
    g_vk.compute_family = UINT32_MAX;
    g_vk.present_family = UINT32_MAX;
    
    for (uint32_t i = 0; i < queue_count; i++) {
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            if (g_vk.graphics_family == UINT32_MAX) g_vk.graphics_family = i;
        }
        if (queues[i].queueFlags & VK_QUEUE_COMPUTE_BIT && g_vk.compute_family == UINT32_MAX) {
            g_vk.compute_family = i;
        }
    }
    
    g_vk.present_family = g_vk.graphics_family;
    
    free(queues);
    return g_vk.graphics_family != UINT32_MAX;
}

static bool create_device(void) {
    float queue_priority = 1.0f;
    
    uint32_t unique_families[3];
    uint32_t unique_count = 0;
    
    unique_families[unique_count++] = g_vk.graphics_family;
    if (g_vk.compute_family != g_vk.graphics_family && g_vk.compute_family != UINT32_MAX) {
        unique_families[unique_count++] = g_vk.compute_family;
    }
    
    VkDeviceQueueCreateInfo queue_create_infos[3];
    for (uint32_t i = 0; i < unique_count; i++) {
        queue_create_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[i].pNext = NULL;
        queue_create_infos[i].flags = 0;
        queue_create_infos[i].queueFamilyIndex = unique_families[i];
        queue_create_infos[i].queueCount = 1;
        queue_create_infos[i].pQueuePriorities = &queue_priority;
    }
    
    const char *device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    
    VkDeviceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = unique_count;
    create_info.pQueueCreateInfos = queue_create_infos;
    create_info.enabledExtensionCount = sizeof(device_extensions) / sizeof(device_extensions[0]);
    create_info.ppEnabledExtensionNames = device_extensions;
    
    if (vkCreateDevice(g_vk.physical_device, &create_info, NULL, &g_vk.device) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create logical device\n");
        return false;
    }
    
    vkGetDeviceQueue(g_vk.device, g_vk.graphics_family, 0, &g_vk.graphics_queue);
    vkGetDeviceQueue(g_vk.device, g_vk.present_family, 0, &g_vk.present_queue);
    if (g_vk.compute_family != UINT32_MAX) {
        vkGetDeviceQueue(g_vk.device, g_vk.compute_family, 0, &g_vk.compute_queue);
    } else {
        g_vk.compute_queue = g_vk.graphics_queue;
        g_vk.compute_family = g_vk.graphics_family;
    }
    
    return true;
}

static bool create_surface(void) {
    Display *display = (Display *)platform_x11_get_display();
    Window window = (Window)(size_t)platform_x11_get_window();
    
    VkXlibSurfaceCreateInfoKHR create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    create_info.dpy = display;
    create_info.window = window;
    
    if (vkCreateXlibSurfaceKHR(g_vk.instance, &create_info, NULL, &g_vk.surface) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create X11 surface\n");
        return false;
    }
    return true;
}

static bool create_swapchain(void) {
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
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_vk.physical_device, g_vk.surface, &mode_count, modes);
    
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < mode_count; i++) {
        if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
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
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    
    if (vkCreateSwapchainKHR(g_vk.device, &create_info, NULL, &g_vk.swapchain) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create swapchain\n");
        return false;
    }
    
    vkGetSwapchainImagesKHR(g_vk.device, g_vk.swapchain, &g_vk.swap_image_count, NULL);
    g_vk.swap_images = malloc(sizeof(VkImage) * g_vk.swap_image_count);
    vkGetSwapchainImagesKHR(g_vk.device, g_vk.swapchain, &g_vk.swap_image_count, g_vk.swap_images);
    
    return true;
}

static bool create_swap_image_views(void) {
    g_vk.swap_views = malloc(sizeof(VkImageView) * g_vk.swap_image_count);
    
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
        
        vkCreateImageView(g_vk.device, &create_info, NULL, &g_vk.swap_views[i]);
    }
    return true;
}

static bool create_depth_buffer(void) {
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
    
    vkCreateImage(g_vk.device, &image_info, NULL, &g_vk.depth_image);
    
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(g_vk.device, g_vk.depth_image, &mem_reqs);
    
    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    vkAllocateMemory(g_vk.device, &alloc_info, NULL, &g_vk.depth_memory);
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
    
    vkCreateImageView(g_vk.device, &view_info, NULL, &g_vk.depth_view);
    return true;
}

static bool create_render_pass(void) {
    VkAttachmentDescription color_attach = {0};
    color_attach.format = g_vk.swap_format;
    color_attach.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attach.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attach.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentDescription depth_attach = {0};
    depth_attach.format = VK_FORMAT_D32_SFLOAT;
    depth_attach.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attach.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attach.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attach.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference color_ref = {0};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference depth_ref = {0};
    depth_ref.attachment = 1;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;
    
    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    VkAttachmentDescription attachments[2] = {color_attach, depth_attach};
    VkRenderPassCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = 2;
    create_info.pAttachments = attachments;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = 1;
    create_info.pDependencies = &dependency;
    
    vkCreateRenderPass(g_vk.device, &create_info, NULL, &g_vk.render_pass);
    return true;
}

static bool create_framebuffers(void) {
    g_vk.framebuffers = malloc(sizeof(VkFramebuffer) * g_vk.swap_image_count);
    
    for (uint32_t i = 0; i < g_vk.swap_image_count; i++) {
        VkImageView attachments[2] = {g_vk.swap_views[i], g_vk.depth_view};
        
        VkFramebufferCreateInfo create_info = {0};
        create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        create_info.renderPass = g_vk.render_pass;
        create_info.attachmentCount = 2;
        create_info.pAttachments = attachments;
        create_info.width = g_vk.swap_extent.width;
        create_info.height = g_vk.swap_extent.height;
        create_info.layers = 1;
        
        vkCreateFramebuffer(g_vk.device, &create_info, NULL, &g_vk.framebuffers[i]);
    }
    return true;
}

static void cleanup_swapchain(void) {
    vkDeviceWaitIdle(g_vk.device);

    if (g_vk.framebuffers) {
        for (uint32_t i = 0; i < g_vk.swap_image_count; i++) {
            if (g_vk.framebuffers[i]) {
                vkDestroyFramebuffer(g_vk.device, g_vk.framebuffers[i], NULL);
            }
        }
        free(g_vk.framebuffers);
        g_vk.framebuffers = NULL;
    }

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

static bool recreate_swapchain(void) {
    vkDeviceWaitIdle(g_vk.device);

    cleanup_swapchain();

    if (!create_swapchain()) return false;
    if (!create_swap_image_views()) return false;
    if (!create_depth_buffer()) return false;
    if (!create_framebuffers()) return false;

    return true;
}

static bool create_command_pool(void) {
    VkCommandPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = g_vk.graphics_family;
    
    vkCreateCommandPool(g_vk.device, &pool_info, NULL, &g_vk.cmd_pool);
    return true;
}

static bool create_command_buffers(void) {
    g_vk.cmd_buffers = malloc(sizeof(VkCommandBuffer) * MAX_FRAMES_IN_FLIGHT);
    
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = g_vk.cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    
    vkAllocateCommandBuffers(g_vk.device, &alloc_info, g_vk.cmd_buffers);
    return true;
}

static bool create_sync_objects(void) {
    g_vk.image_avail_sems = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    g_vk.render_done_sems = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    g_vk.in_flight_fences = malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);
    
    VkSemaphoreCreateInfo sem_info = {0};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fence_info = {0};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkCreateSemaphore(g_vk.device, &sem_info, NULL, &g_vk.image_avail_sems[i]);
        vkCreateSemaphore(g_vk.device, &sem_info, NULL, &g_vk.render_done_sems[i]);
        vkCreateFence(g_vk.device, &fence_info, NULL, &g_vk.in_flight_fences[i]);
    }
    return true;
}

static bool create_descriptor_pool(void) {
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32},
    };
    
    VkDescriptorPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]);
    pool_info.pPoolSizes = pool_sizes;
    pool_info.maxSets = 256;
    
    vkCreateDescriptorPool(g_vk.device, &pool_info, NULL, &g_vk.desc_pool);
    return true;
}

static bool create_default_sampler(void) {
    VkSamplerCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
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
    
    vkCreateSampler(g_vk.device, &create_info, NULL, &g_vk.default_sampler);
    return true;
}

static void init_resource_arrays(void) {
    g_vk.pipelines = calloc(MAX_PIPELINES, sizeof(Pipeline));
    g_vk.buffers = calloc(MAX_BUFFERS, sizeof(VkBuffer));
    g_vk.buffer_memories = calloc(MAX_BUFFERS, sizeof(VkDeviceMemory));
    g_vk.buffer_sizes = calloc(MAX_BUFFERS, sizeof(uint64_t));
    g_vk.textures = calloc(MAX_TEXTURES, sizeof(VkImage));
    g_vk.texture_memories = calloc(MAX_TEXTURES, sizeof(VkDeviceMemory));
    g_vk.texture_views = calloc(MAX_TEXTURES, sizeof(VkImageView));
    g_vk.texture_samplers = calloc(MAX_TEXTURES, sizeof(VkSampler));
    g_vk.vaos = calloc(MAX_VAO, sizeof(VkBuffer));
}

/* ============================================================================
 * Public API: Initialization
 * ============================================================================ */

void renderer_init(int width, int height) {
    g_vk.width = width;
    g_vk.height = height;
    
    create_instance();
    select_physical_device();
    create_surface();
    find_queue_families();
    create_device();
    create_swapchain();
    create_swap_image_views();
    create_depth_buffer();
    create_render_pass();
    create_framebuffers();
    create_command_pool();
    create_command_buffers();
    create_sync_objects();
    create_descriptor_pool();
    create_default_sampler();
    init_resource_arrays();

    /* Default state */
    g_vk.cull_mode = VK_CULL_MODE_BACK_BIT;
    g_vk.depth_test = VK_TRUE;
    g_vk.depth_write = VK_TRUE;
    g_vk.depth_op = VK_COMPARE_OP_LESS;
    g_vk.blend_enable = VK_FALSE;
    g_vk.blend_src = VK_BLEND_FACTOR_SRC_ALPHA;
    g_vk.blend_dst = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    g_vk.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    g_vk.line_width = 1.0f;
    g_vk.poly_offset_factor = 0.0f;
    g_vk.poly_offset_units = 0.0f;

    printf("Vulkan renderer initialized\n");
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
        vkDestroyFramebuffer(g_vk.device, g_vk.framebuffers[i], NULL);
        vkDestroyImageView(g_vk.device, g_vk.swap_views[i], NULL);
    }
    
    vkDestroyImageView(g_vk.device, g_vk.depth_view, NULL);
    vkDestroyImage(g_vk.device, g_vk.depth_image, NULL);
    vkFreeMemory(g_vk.device, g_vk.depth_memory, NULL);
    
    vkDestroyRenderPass(g_vk.device, g_vk.render_pass, NULL);
    vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, NULL);
    vkDestroyDevice(g_vk.device, NULL);
    vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, NULL);
    vkDestroyInstance(g_vk.instance, NULL);
}

/* ============================================================================
 * Frame recording state
 * ============================================================================ */

static VkCommandBuffer g_active_cmd = VK_NULL_HANDLE;
static bool g_frame_started = false;
static float g_clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
static bool g_clear_depth = false;

/* ============================================================================
 * Public API: Frame control
 * ============================================================================ */

void renderer_clear(float r, float g, float b, float a) {
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

    /* Wait for previous frame */
    vkWaitForFences(g_vk.device, 1, &g_vk.in_flight_fences[g_vk.current_frame], VK_TRUE, UINT64_MAX);

    /* Acquire next image */
    VkResult result = vkAcquireNextImageKHR(g_vk.device, g_vk.swapchain, UINT64_MAX,
                                            g_vk.image_avail_sems[g_vk.current_frame], VK_NULL_HANDLE,
                                            &g_vk.image_index);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        if (recreate_swapchain()) {
            /* Try to acquire again after recreation */
            result = vkAcquireNextImageKHR(g_vk.device, g_vk.swapchain, UINT64_MAX,
                                           g_vk.image_avail_sems[g_vk.current_frame], VK_NULL_HANDLE,
                                           &g_vk.image_index);
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            return;
        }
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
    
    /* Begin render pass */
    VkRenderPassBeginInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = g_vk.render_pass;
    render_pass_info.framebuffer = g_vk.framebuffers[g_vk.image_index];
    render_pass_info.renderArea.offset.x = 0;
    render_pass_info.renderArea.offset.y = 0;
    render_pass_info.renderArea.extent = g_vk.swap_extent;
    
    VkClearValue clear_values[2];
    clear_values[0].color.float32[0] = g_clear_color[0];
    clear_values[0].color.float32[1] = g_clear_color[1];
    clear_values[0].color.float32[2] = g_clear_color[2];
    clear_values[0].color.float32[3] = g_clear_color[3];
    clear_values[1].depthStencil.depth = 1.0f;
    clear_values[1].depthStencil.stencil = 0;
    
    render_pass_info.clearValueCount = 2;
    render_pass_info.pClearValues = clear_values;
    
    vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
}

void renderer_swap(void) {
    if (!g_frame_started || g_active_cmd == VK_NULL_HANDLE) return;
    
    /* End render pass */
    vkCmdEndRenderPass(g_active_cmd);
    
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
    vkQueueSubmit(g_vk.graphics_queue, 1, &submit_info, g_vk.in_flight_fences[g_vk.current_frame]);
    
    /* Present */
    VkPresentInfoKHR present_info = {0};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &g_vk.render_done_sems[g_vk.current_frame];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &g_vk.swapchain;
    present_info.pImageIndices = &g_vk.image_index;
    
    VkResult result = vkQueuePresentKHR(g_vk.present_queue, &present_info);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || g_vk.framebuffer_resized) {
        g_vk.framebuffer_resized = false;
        recreate_swapchain();
    }
    
    g_vk.current_frame = (g_vk.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    g_active_cmd = VK_NULL_HANDLE;
    g_frame_started = false;
}

void renderer_swap_interval(int interval) {
    (void)interval;
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
    switch (cap) {
        case R_CAP_DEPTH_TEST:
            g_vk.depth_test = VK_TRUE;
            break;
        case R_CAP_CULL_FACE:
            g_vk.cull_mode = VK_CULL_MODE_BACK_BIT;
            break;
        case R_CAP_BLEND:
            g_vk.blend_enable = VK_TRUE;
            break;
        case R_CAP_POLYGON_OFFSET_LINE:
            /* Enable polygon offset - values set via renderer_polygon_offset */
            break;
        case R_CAP_MULTISAMPLE:
            /* Vulkan always uses single sample (no MSAA yet) - no-op */
            break;
        case R_CAP_SCISSOR_TEST:
            /* Scissor always enabled in Vulkan - no-op */
            break;
        default:
            break;
    }
}

void renderer_disable(R_Cap cap) {
    switch (cap) {
        case R_CAP_DEPTH_TEST:
            g_vk.depth_test = VK_FALSE;
            break;
        case R_CAP_CULL_FACE:
            g_vk.cull_mode = VK_CULL_MODE_NONE;
            break;
        case R_CAP_BLEND:
            g_vk.blend_enable = VK_FALSE;
            break;
        case R_CAP_POLYGON_OFFSET_LINE:
            /* Disable polygon offset */
            g_vk.poly_offset_factor = 0.0f;
            g_vk.poly_offset_units = 0.0f;
            break;
        case R_CAP_MULTISAMPLE:
            /* No-op */
            break;
        case R_CAP_SCISSOR_TEST:
            /* No-op - scissor always enabled */
            break;
        default:
            break;
    }
}

void renderer_depth_mask(bool write) {
    g_vk.depth_write = write ? VK_TRUE : VK_FALSE;
}

void renderer_depth_func(R_DepthFunc func) {
    switch (func) {
        case R_FUNC_LESS:
            g_vk.depth_op = VK_COMPARE_OP_LESS;
            break;
        case R_FUNC_LEQUAL:
            g_vk.depth_op = VK_COMPARE_OP_LESS_OR_EQUAL;
            break;
        case R_FUNC_ALWAYS:
            g_vk.depth_op = VK_COMPARE_OP_ALWAYS;
            break;
    }
}

static VkBlendFactor blend_factor_to_vk(R_BlendFactor factor) {
    switch (factor) {
        case R_BLEND_ZERO: return VK_BLEND_FACTOR_ZERO;
        case R_BLEND_ONE: return VK_BLEND_FACTOR_ONE;
        case R_BLEND_SRC_ALPHA: return VK_BLEND_FACTOR_SRC_ALPHA;
        case R_BLEND_ONE_MINUS_SRC_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        default: return VK_BLEND_FACTOR_ONE;
    }
}

void renderer_blend_func(R_BlendFactor src, R_BlendFactor dst) {
    g_vk.blend_src = blend_factor_to_vk(src);
    g_vk.blend_dst = blend_factor_to_vk(dst);
}

void renderer_polygon_offset(float factor, float units) {
    g_vk.poly_offset_factor = factor;
    g_vk.poly_offset_units = units;
}

void renderer_line_width(float width) {
    g_vk.line_width = width;
}

void renderer_push_attrib(void) {
    /* TODO: Implement state stack */
}

void renderer_pop_attrib(void) {
    /* TODO: Implement state stack */
}

/* ============================================================================
 * Shader loading
 * ============================================================================ */

static char* load_spirv_file(const char *path, size_t *out_size) {
    char spv_path[256];
    snprintf(spv_path, sizeof(spv_path), "%s.spv", path);
    
    FILE *f = fopen(spv_path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open shader: %s\n", spv_path);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buf = malloc(size);
    if (buf) {
        fread(buf, 1, size, f);
        *out_size = size;
    }
    fclose(f);
    return buf;
}

/* ============================================================================
 * Pipeline creation
 * ============================================================================ */

static VkShaderModule load_shader_module(const char *path) {
    size_t size;
    char *code = load_spirv_file(path, &size);
    if (!code) return VK_NULL_HANDLE;
    
    VkShaderModule module = create_shader_module(code, size);
    free(code);
    return module;
}

static VkDescriptorSetLayout create_texture_descriptor_layout(void) {
    VkDescriptorSetLayoutBinding binding = {0};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    VkDescriptorSetLayoutCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    create_info.bindingCount = 1;
    create_info.pBindings = &binding;
    
    VkDescriptorSetLayout layout;
    vkCreateDescriptorSetLayout(g_vk.device, &create_info, NULL, &layout);
    return layout;
}

static VkPipelineLayout create_pipeline_layout(VkDescriptorSetLayout tex_layout) {
    /* Push constants: mat4 model (0), mat4 view (64), mat4 proj (128), then other uniforms */
    VkPushConstantRange push_range = {0};
    push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_range.offset = 0;
    push_range.size = 256; /* Enough for 3 matrices + some uniforms */
    
    VkPipelineLayoutCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    create_info.setLayoutCount = tex_layout ? 1 : 0;
    create_info.pSetLayouts = tex_layout ? &tex_layout : NULL;
    create_info.pushConstantRangeCount = 1;
    create_info.pPushConstantRanges = &push_range;
    
    VkPipelineLayout layout;
    vkCreatePipelineLayout(g_vk.device, &create_info, NULL, &layout);
    return layout;
}

typedef enum {
    VERTEX_FORMAT_TERRAIN,   /* 5 attrs: pos3, color3, normal3, ao1, uv2, stride=64 */
    VERTEX_FORMAT_SKYBOX,    /* 1 attr: pos3, stride=12 */
    VERTEX_FORMAT_OUTLINE,   /* 1 attr: pos3, stride=12 */
    VERTEX_FORMAT_HUD,       /* 1 attr: pos2, stride=8 */
    VERTEX_FORMAT_UI,        /* 3 attrs: pos2, uv2, color4, stride=32 */
} VertexFormat;

/* Pipeline configuration - describes all fixed-function state for a pipeline */
typedef struct {
    VertexFormat vformat;
    VkPrimitiveTopology topology;
    VkBool32 depth_test_enable;
    VkBool32 depth_write_enable;
    VkCompareOp depth_compare;
    VkCullModeFlags cull_mode;
    VkBool32 blend_enable;
    VkBlendFactor blend_src;
    VkBlendFactor blend_dst;
    bool has_texture;           /* needs descriptor set for sampler */
    VkBool32 depth_bias_enable; /* for polygon offset */
    uint32_t push_constant_size;
} PipelineConfig;

static void get_pipeline_config(const char *vert_path, const char *frag_path, PipelineConfig *cfg) {
    (void)frag_path;
    
    /* Default: terrain/basic shader */
    *cfg = (PipelineConfig){
        .vformat = VERTEX_FORMAT_TERRAIN,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .depth_test_enable = VK_TRUE,
        .depth_write_enable = VK_TRUE,
        .depth_compare = VK_COMPARE_OP_LESS,
        .cull_mode = VK_CULL_MODE_BACK_BIT,
        .blend_enable = VK_FALSE,
        .blend_src = VK_BLEND_FACTOR_SRC_ALPHA,
        .blend_dst = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .has_texture = true,
        .depth_bias_enable = VK_FALSE,
        .push_constant_size = 256,
    };
    
    if (strstr(vert_path, "skybox")) {
        cfg->vformat = VERTEX_FORMAT_SKYBOX;
        cfg->depth_write_enable = VK_FALSE;
        cfg->depth_compare = VK_COMPARE_OP_LESS_OR_EQUAL; /* For xyww depth trick */
        cfg->has_texture = false;
        cfg->push_constant_size = 128;
    } else if (strstr(vert_path, "outline")) {
        cfg->vformat = VERTEX_FORMAT_OUTLINE;
        cfg->topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        cfg->depth_write_enable = VK_FALSE;
        cfg->cull_mode = VK_CULL_MODE_NONE;
        cfg->depth_bias_enable = VK_TRUE; /* Always enable for outline */
        cfg->has_texture = false;
        cfg->push_constant_size = 208; /* 3 matrices + uColor */
    } else if (strstr(vert_path, "ui")) {
        cfg->vformat = VERTEX_FORMAT_UI;
        cfg->depth_test_enable = VK_FALSE;
        cfg->depth_write_enable = VK_FALSE;
        cfg->cull_mode = VK_CULL_MODE_NONE;
        cfg->blend_enable = VK_TRUE;
        cfg->has_texture = true; /* UI needs font texture */
        cfg->push_constant_size = 8; /* uScreenSize vec2 */
    } else if (strstr(vert_path, "hud")) {
        cfg->vformat = VERTEX_FORMAT_HUD;
        cfg->depth_test_enable = VK_FALSE;
        cfg->depth_write_enable = VK_FALSE;
        cfg->cull_mode = VK_CULL_MODE_NONE;
        cfg->blend_enable = VK_TRUE;
        cfg->has_texture = false;
        cfg->push_constant_size = 16; /* uColor + uAlpha */
    }
}

static VkPipeline create_graphics_pipeline(VkShaderModule vert, VkShaderModule frag,
                                           VkPipelineLayout layout, const PipelineConfig *cfg) {
    VkPipelineShaderStageCreateInfo vert_stage = {0};
    vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage.module = vert;
    vert_stage.pName = "main";
    
    VkPipelineShaderStageCreateInfo frag_stage = {0};
    frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage.module = frag;
    frag_stage.pName = "main";
    
    VkPipelineShaderStageCreateInfo stages[] = {vert_stage, frag_stage};

    /* Configure vertex input based on format */
    VkVertexInputBindingDescription binding = {0};
    binding.binding = 0;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[5] = {0};
    uint32_t attr_count = 0;

    switch (cfg->vformat) {
        case VERTEX_FORMAT_TERRAIN:
            /* Terrain: pos3, color3, normal3, ao1, uv2 - stride=64 */
            binding.stride = 64;
            
            /* Location 0: aPos (vec3) */
            attrs[0].location = 0;
            attrs[0].binding = 0;
            attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attrs[0].offset = 0;
            
            /* Location 1: aColor (vec3) */
            attrs[1].location = 1;
            attrs[1].binding = 0;
            attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attrs[1].offset = 16;
            
            /* Location 2: aNormal (vec3) */
            attrs[2].location = 2;
            attrs[2].binding = 0;
            attrs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
            attrs[2].offset = 32;
            
            /* Location 3: aAO (float) - offset 44 (after nx, ny, nz at 32, 36, 40) */
            attrs[3].location = 3;
            attrs[3].binding = 0;
            attrs[3].format = VK_FORMAT_R32_SFLOAT;
            attrs[3].offset = 44;
            
            /* Location 4: aTexCoord (vec2) - offset 48 (start of fourth vec4) */
            attrs[4].location = 4;
            attrs[4].binding = 0;
            attrs[4].format = VK_FORMAT_R32G32_SFLOAT;
            attrs[4].offset = 48;
            
            attr_count = 5;
            break;
            
        case VERTEX_FORMAT_SKYBOX:
        case VERTEX_FORMAT_OUTLINE:
            /* Skybox/Outline: pos3 only - stride=12 */
            binding.stride = 12;
            
            /* Location 0: aPos (vec3) */
            attrs[0].location = 0;
            attrs[0].binding = 0;
            attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attrs[0].offset = 0;
            
            attr_count = 1;
            break;
            
        case VERTEX_FORMAT_HUD:
            /* HUD: pos2 only - stride=8 */
            binding.stride = 8;

            /* Location 0: aPos (vec2) */
            attrs[0].location = 0;
            attrs[0].binding = 0;
            attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
            attrs[0].offset = 0;

            attr_count = 1;
            break;

        case VERTEX_FORMAT_UI:
            /* UI: pos2, uv2, color4 - stride=32 */
            binding.stride = 32;

            /* Location 0: aPos (vec2) */
            attrs[0].location = 0;
            attrs[0].binding = 0;
            attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
            attrs[0].offset = 0;

            /* Location 1: aUV (vec2) */
            attrs[1].location = 1;
            attrs[1].binding = 0;
            attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
            attrs[1].offset = 8;

            /* Location 2: aColor (vec4) */
            attrs[2].location = 2;
            attrs[2].binding = 0;
            attrs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attrs[2].offset = 16;

            attr_count = 3;
            break;
    }
    
    VkPipelineVertexInputStateCreateInfo vertex_input = {0};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = attr_count;
    vertex_input.pVertexAttributeDescriptions = attrs;
    
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = cfg->topology;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state = {0};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster = {0};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.depthClampEnable = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_FALSE;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = cfg->cull_mode;
    raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    raster.depthBiasEnable = cfg->depth_bias_enable;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample = {0};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {0};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = cfg->depth_test_enable;
    depth_stencil.depthWriteEnable = cfg->depth_write_enable;
    depth_stencil.depthCompareOp = cfg->depth_compare;
    depth_stencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend = {0};
    color_blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend.blendEnable = cfg->blend_enable;
    color_blend.srcColorBlendFactor = cfg->blend_src;
    color_blend.dstColorBlendFactor = cfg->blend_dst;
    color_blend.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend.srcAlphaBlendFactor = cfg->blend_src;
    color_blend.dstAlphaBlendFactor = cfg->blend_dst;
    color_blend.alphaBlendOp = VK_BLEND_OP_ADD;
    
    VkPipelineColorBlendStateCreateInfo blend_state = {0};
    blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_state.attachmentCount = 1;
    blend_state.pAttachments = &color_blend;
    
    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH,
        VK_DYNAMIC_STATE_DEPTH_BIAS,
    };
    
    VkPipelineDynamicStateCreateInfo dynamic_state = {0};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = sizeof(dynamic_states) / sizeof(dynamic_states[0]);
    dynamic_state.pDynamicStates = dynamic_states;
    
    VkGraphicsPipelineCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.stageCount = 2;
    create_info.pStages = stages;
    create_info.pVertexInputState = &vertex_input;
    create_info.pInputAssemblyState = &input_assembly;
    create_info.pViewportState = &viewport_state;
    create_info.pRasterizationState = &raster;
    create_info.pMultisampleState = &multisample;
    create_info.pDepthStencilState = &depth_stencil;
    create_info.pColorBlendState = &blend_state;
    create_info.pDynamicState = &dynamic_state;
    create_info.layout = layout;
    create_info.renderPass = g_vk.render_pass;
    create_info.subpass = 0;
    
    VkPipeline pipeline;
    vkCreateGraphicsPipelines(g_vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, &pipeline);
    return pipeline;
}

/* ============================================================================
 * Public API: Programs and uniforms
 * ============================================================================ */

typedef struct {
    char name[64];
    int offset;
} UniformMapping;

static UniformMapping g_uniforms[32];
static int g_uniform_count = 0;
static uint8_t g_push_constants[256];
static bool g_push_dirty = false;

R_Program renderer_create_program(const char *vert_path, const char *frag_path) {
    if (g_vk.pipeline_count >= MAX_PIPELINES) return R_INVALID_HANDLE;

    VkShaderModule vert = load_shader_module(vert_path);
    VkShaderModule frag = load_shader_module(frag_path);
    if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE) {
        if (vert) vkDestroyShaderModule(g_vk.device, vert, NULL);
        if (frag) vkDestroyShaderModule(g_vk.device, frag, NULL);
        return R_INVALID_HANDLE;
    }

    /* Get pipeline configuration based on shader type */
    PipelineConfig cfg;
    get_pipeline_config(vert_path, frag_path, &cfg);

    uint32_t idx = g_vk.pipeline_count++;
    Pipeline *pipe = &g_vk.pipelines[idx];

    pipe->vert_module = vert;
    pipe->frag_module = frag;

    /* Create descriptor layout if shader needs textures */
    if (cfg.has_texture) {
        pipe->desc_set_layout = create_texture_descriptor_layout();
    } else {
        /* Create empty descriptor layout for shaders without textures */
        VkDescriptorSetLayoutCreateInfo layout_info = {0};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 0;
        layout_info.pBindings = NULL;
        vkCreateDescriptorSetLayout(g_vk.device, &layout_info, NULL, &pipe->desc_set_layout);
    }

    pipe->layout = create_pipeline_layout(pipe->desc_set_layout);
    pipe->pipeline = create_graphics_pipeline(vert, frag, pipe->layout, &cfg);

    /* Allocate descriptor set for texture if needed */
    if (cfg.has_texture) {
        VkDescriptorSetAllocateInfo alloc_info = {0};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = g_vk.desc_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &pipe->desc_set_layout;
        vkAllocateDescriptorSets(g_vk.device, &alloc_info, &pipe->desc_set);
    } else {
        pipe->desc_set = VK_NULL_HANDLE;
    }

    return idx;
}

R_Program renderer_create_compute(const char *comp_path) {
    (void)comp_path;
    /* TODO: Implement compute shaders */
    return R_INVALID_HANDLE;
}

void renderer_destroy_program(R_Program program) {
    if (program >= g_vk.pipeline_count) return;
    Pipeline *pipe = &g_vk.pipelines[program];
    
    vkDestroyPipeline(g_vk.device, pipe->pipeline, NULL);
    vkDestroyPipelineLayout(g_vk.device, pipe->layout, NULL);
    vkDestroyDescriptorSetLayout(g_vk.device, pipe->desc_set_layout, NULL);
    vkDestroyShaderModule(g_vk.device, pipe->vert_module, NULL);
    vkDestroyShaderModule(g_vk.device, pipe->frag_module, NULL);
    
    pipe->pipeline = VK_NULL_HANDLE;
}

void renderer_use_program(R_Program program) {
    g_vk.active_pipeline = program;
    g_uniform_count = 0;
    g_push_dirty = false;
}

int renderer_uniform_location(R_Program program, const char *name) {
    (void)program;
    /* Map uniform names to push constant offsets */
    int base_offset = 192; /* After 3 mat4 matrices (3 * 64 = 192) */
    
    for (int i = 0; i < g_uniform_count; i++) {
        if (strcmp(g_uniforms[i].name, name) == 0) {
            return g_uniforms[i].offset;
        }
    }
    
    if (g_uniform_count >= 32) return -1;
    
    int offset = base_offset + (g_uniform_count * 16); /* 16 bytes per uniform vec4 slot */
    strncpy(g_uniforms[g_uniform_count].name, name, 63);
    g_uniforms[g_uniform_count].offset = offset;
    return g_uniform_count++;
}

void renderer_uniform_mat4(int location, const float *matrix) {
    if (location < 0 || location >= g_uniform_count) return;
    if (!matrix) return;
    
    /* Matrix uniforms go at fixed offsets: model=0, view=64, proj=128 */
    int offset = g_uniforms[location].offset;
    const char *name = g_uniforms[location].name;
    
    if (strstr(name, "model")) offset = 0;
    else if (strstr(name, "view")) offset = 64;
    else if (strstr(name, "projection")) offset = 128;
    
    /* Bounds check */
    if (offset + 64 > 256) return;
    
    memcpy(g_push_constants + offset, matrix, 64);
    g_push_dirty = true;
}

void renderer_uniform_vec3(int location, float x, float y, float z) {
    if (location < 0 || location >= g_uniform_count) return;
    int offset = g_uniforms[location].offset;
    if (offset + 16 > 256) return;
    float vec[4] = {x, y, z, 0.0f};
    memcpy(g_push_constants + offset, vec, 12);
    g_push_dirty = true;
}

void renderer_uniform_vec2(int location, float x, float y) {
    if (location < 0 || location >= g_uniform_count) return;
    int offset = g_uniforms[location].offset;
    if (offset + 8 > 256) return;
    float vec[2] = {x, y};
    memcpy(g_push_constants + offset, vec, 8);
    g_push_dirty = true;
}

void renderer_uniform_float(int location, float value) {
    if (location < 0 || location >= g_uniform_count) return;
    int offset = g_uniforms[location].offset;
    if (offset + 4 > 256) return;
    memcpy(g_push_constants + offset, &value, 4);
    g_push_dirty = true;
}

void renderer_uniform_int(int location, int value) {
    if (location < 0 || location >= g_uniform_count) return;
    int offset = g_uniforms[location].offset;
    if (offset + 4 > 256) return;
    memcpy(g_push_constants + offset, &value, 4);
    g_push_dirty = true;
}

void renderer_uniform_ivec2(int location, int x, int y) {
    if (location < 0 || location >= g_uniform_count) return;
    int offset = g_uniforms[location].offset;
    if (offset + 8 > 256) return;
    int vec[2] = {x, y};
    memcpy(g_push_constants + offset, vec, 8);
    g_push_dirty = true;
}

/* ============================================================================
 * Buffer helpers
 * ============================================================================ */

static VkBuffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkDeviceMemory *out_memory) {
    VkBufferCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size = size;
    create_info.usage = usage;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer buffer;
    vkCreateBuffer(g_vk.device, &create_info, NULL, &buffer);
    
    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(g_vk.device, buffer, &mem_reqs);
    
    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, props);
    
    vkAllocateMemory(g_vk.device, &alloc_info, NULL, out_memory);
    vkBindBufferMemory(g_vk.device, buffer, *out_memory, 0);
    
    return buffer;
}

static void upload_buffer_data(VkBuffer buffer, VkDeviceMemory memory, size_t size, const void *data) {
    /* Create staging buffer */
    VkDeviceMemory staging_mem;
    VkBuffer staging = create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                      &staging_mem);
    
    /* Copy data to staging */
    void *mapped;
    vkMapMemory(g_vk.device, staging_mem, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(g_vk.device, staging_mem);
    
    /* Record copy command */
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = g_vk.cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    vkAllocateCommandBuffers(g_vk.device, &alloc_info, &cmd);
    
    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);
    
    VkBufferCopy copy = {0};
    copy.size = size;
    vkCmdCopyBuffer(cmd, staging, buffer, 1, &copy);
    
    vkEndCommandBuffer(cmd);
    
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    
    vkQueueSubmit(g_vk.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_vk.graphics_queue);
    
    vkFreeCommandBuffers(g_vk.device, g_vk.cmd_pool, 1, &cmd);
    vkDestroyBuffer(g_vk.device, staging, NULL);
    vkFreeMemory(g_vk.device, staging_mem, NULL);
}

/* ============================================================================
 * Public API: Buffers
 * ============================================================================ */

R_Buffer renderer_create_buffer(void) {
    if (g_vk.buffer_count >= MAX_BUFFERS) return R_INVALID_HANDLE;
    return g_vk.buffer_count++;
}

void renderer_destroy_buffer(R_Buffer buffer) {
    if (buffer >= g_vk.buffer_count) return;
    if (g_vk.buffers[buffer]) {
        vkDestroyBuffer(g_vk.device, g_vk.buffers[buffer], NULL);
        vkFreeMemory(g_vk.device, g_vk.buffer_memories[buffer], NULL);
        g_vk.buffers[buffer] = VK_NULL_HANDLE;
    }
}

/* Current VAO tracking for buffer binding */
static R_VAO g_current_vao = R_INVALID_HANDLE;

void renderer_bind_buffer(R_BufferTarget target, R_Buffer buffer) {
    if (buffer == R_INVALID_HANDLE) {
        g_vk.bound_vbo = VK_NULL_HANDLE;
        g_vk.bound_index_buffer = VK_NULL_HANDLE;
        return;
    }
    if (buffer >= g_vk.buffer_count) return;
    
    VkBuffer vk_buffer = g_vk.buffers[buffer];
    
    if (target == R_BUF_ELEMENT) {
        g_vk.bound_index_buffer = vk_buffer;
        /* Also store in current VAO if one is bound */
        if (g_current_vao != R_INVALID_HANDLE && g_current_vao < MAX_VAO) {
            g_vk.vao_index_buffers[g_current_vao] = vk_buffer;
        }
    } else {
        g_vk.bound_vbo = vk_buffer;
        /* Also store in current VAO if one is bound */
        if (g_current_vao != R_INVALID_HANDLE && g_current_vao < MAX_VAO) {
            g_vk.vao_buffers[g_current_vao] = vk_buffer;
        }
    }
}

void renderer_buffer_data(R_BufferTarget target, size_t size, const void *data, R_Usage usage) {
    (void)usage;
    
    /* Find the buffer handle to update based on what's currently bound */
    R_Buffer buffer_handle = R_INVALID_HANDLE;
    
    if (target == R_BUF_ELEMENT) {
        /* Find the buffer handle from bound index buffer */
        for (uint32_t i = 0; i < g_vk.buffer_count; i++) {
            if (g_vk.buffers[i] == g_vk.bound_index_buffer) {
                buffer_handle = i;
                break;
            }
        }
        /* If not found, use the most recently created buffer */
        if (buffer_handle == R_INVALID_HANDLE && g_vk.buffer_count > 0) {
            buffer_handle = g_vk.buffer_count - 1;
        }
    } else {
        /* Find the buffer handle from bound VBO */
        for (uint32_t i = 0; i < g_vk.buffer_count; i++) {
            if (g_vk.buffers[i] == g_vk.bound_vbo) {
                buffer_handle = i;
                break;
            }
        }
        /* If not found, use the most recently created buffer */
        if (buffer_handle == R_INVALID_HANDLE && g_vk.buffer_count > 0) {
            buffer_handle = g_vk.buffer_count - 1;
        }
    }
    
    if (buffer_handle == R_INVALID_HANDLE || buffer_handle >= g_vk.buffer_count) return;
    
    /* Destroy old buffer if exists */
    if (g_vk.buffers[buffer_handle]) {
        vkDestroyBuffer(g_vk.device, g_vk.buffers[buffer_handle], NULL);
        vkFreeMemory(g_vk.device, g_vk.buffer_memories[buffer_handle], NULL);
    }
    
    /* Determine usage flags */
    VkBufferUsageFlags usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (target == R_BUF_ARRAY) usage_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    else if (target == R_BUF_ELEMENT) usage_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    else if (target == R_BUF_SHADER_STORAGE) usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    else if (target == R_BUF_DRAW_INDIRECT) usage_flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    
    /* Create new buffer */
    g_vk.buffers[buffer_handle] = create_buffer(size, usage_flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                 &g_vk.buffer_memories[buffer_handle]);
    g_vk.buffer_sizes[buffer_handle] = size;
    
    /* Upload data */
    if (data) {
        upload_buffer_data(g_vk.buffers[buffer_handle], g_vk.buffer_memories[buffer_handle], size, data);
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
    (void)offset;
    (void)size;
    (void)data;
    /* TODO: Implement sub-data upload */
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
    /* TODO: Implement SSBO binding for compute shaders */
}

/* ============================================================================
 * VAO implementation
 * ============================================================================ */

typedef struct {
    VkBuffer buffer;
    VkBuffer index_buffer;
    /* Vertex format is fixed for now - stored in pipeline */
} VAOState;

static VAOState g_vao_state = {0};

R_VAO renderer_create_vao(void) {
    if (g_vk.vao_count >= MAX_VAO) return R_INVALID_HANDLE;
    /* VAO is just an index for tracking - actual state is in g_vao_state */
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

/* ============================================================================
 * Texture helpers
 * ============================================================================ */

static VkImage create_image(uint32_t width, uint32_t height, uint32_t depth, VkFormat format,
                             VkImageUsageFlags usage, VkDeviceMemory *out_memory) {
    VkImageCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    create_info.imageType = depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    create_info.extent.width = width;
    create_info.extent.height = height;
    create_info.extent.depth = depth;
    create_info.mipLevels = 1;
    create_info.arrayLayers = 1;
    create_info.format = format;
    create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    create_info.usage = usage;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    
    VkImage image;
    vkCreateImage(g_vk.device, &create_info, NULL, &image);
    
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(g_vk.device, image, &mem_reqs);
    
    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    vkAllocateMemory(g_vk.device, &alloc_info, NULL, out_memory);
    vkBindImageMemory(g_vk.device, image, *out_memory, 0);
    
    return image;
}

static VkImageView create_image_view(VkImage image, VkFormat format, VkImageViewType view_type) {
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
    vkCreateImageView(g_vk.device, &create_info, NULL, &view);
    return view;
}

static void transition_image_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout) {
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = g_vk.cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    vkAllocateCommandBuffers(g_vk.device, &alloc_info, &cmd);
    
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
}

static void upload_image_data(VkImage image, uint32_t width, uint32_t height, uint32_t depth, 
                               const void *data, VkDeviceSize data_size) {
    /* Create staging buffer */
    VkDeviceMemory staging_mem;
    VkBuffer staging = create_buffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                      &staging_mem);
    
    /* Copy data to staging */
    void *mapped;
    vkMapMemory(g_vk.device, staging_mem, 0, data_size, 0, &mapped);
    memcpy(mapped, data, data_size);
    vkUnmapMemory(g_vk.device, staging_mem);
    
    /* Transition to transfer dst */
    transition_image_layout(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    /* Record copy command */
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = g_vk.cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    vkAllocateCommandBuffers(g_vk.device, &alloc_info, &cmd);
    
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
    
    vkCmdCopyBufferToImage(cmd, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    vkEndCommandBuffer(cmd);
    
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    
    vkQueueSubmit(g_vk.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_vk.graphics_queue);
    
    vkFreeCommandBuffers(g_vk.device, g_vk.cmd_pool, 1, &cmd);
    
    /* Transition to shader read */
    transition_image_layout(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    vkDestroyBuffer(g_vk.device, staging, NULL);
    vkFreeMemory(g_vk.device, staging_mem, NULL);
}

/* ============================================================================
 * Public API: Textures
 * ============================================================================ */

static int g_active_texture_unit = 0;
static R_Texture g_bound_textures[16] = {R_INVALID_HANDLE};

R_Texture renderer_create_texture(void) {
    if (g_vk.texture_count >= MAX_TEXTURES) return R_INVALID_HANDLE;
    uint32_t idx = g_vk.texture_count++;
    g_vk.texture_samplers[idx] = g_vk.default_sampler;
    return idx;
}

void renderer_destroy_texture(R_Texture texture) {
    if (texture >= g_vk.texture_count) return;
    if (g_vk.textures[texture]) {
        vkDestroyImageView(g_vk.device, g_vk.texture_views[texture], NULL);
        vkDestroyImage(g_vk.device, g_vk.textures[texture], NULL);
        vkFreeMemory(g_vk.device, g_vk.texture_memories[texture], NULL);
        g_vk.textures[texture] = VK_NULL_HANDLE;
    }
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

void renderer_tex_image_2d(int width, int height, const void *data) {
    R_Texture tex = g_bound_textures[g_active_texture_unit];
    if (tex >= g_vk.texture_count) return;
    
    /* Destroy old texture if exists */
    if (g_vk.textures[tex]) {
        renderer_destroy_texture(tex);
    }
    
    /* Create new image */
    VkDeviceSize data_size = width * height * 4; /* RGBA */
    g_vk.textures[tex] = create_image(width, height, 1, VK_FORMAT_R8G8B8A8_SRGB,
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                       &g_vk.texture_memories[tex]);
    
    /* Upload data */
    if (data) {
        upload_image_data(g_vk.textures[tex], width, height, 1, data, data_size);
    }
    
    /* Create view */
    g_vk.texture_views[tex] = create_image_view(g_vk.textures[tex], VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_VIEW_TYPE_2D);
}

void renderer_tex_image_3d(int width, int height, int depth, const void *data) {
    R_Texture tex = g_bound_textures[g_active_texture_unit];
    if (tex >= g_vk.texture_count) return;
    
    /* Destroy old texture if exists */
    if (g_vk.textures[tex]) {
        renderer_destroy_texture(tex);
    }
    
    /* Create new image */
    VkDeviceSize data_size = width * height * depth * 4; /* RGBA */
    g_vk.textures[tex] = create_image(width, height, depth, VK_FORMAT_R8G8B8A8_SRGB,
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                       &g_vk.texture_memories[tex]);
    
    /* Upload data */
    if (data) {
        upload_image_data(g_vk.textures[tex], width, height, depth, data, data_size);
    }
    
    /* Create view */
    g_vk.texture_views[tex] = create_image_view(g_vk.textures[tex], VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_VIEW_TYPE_3D);
}

void renderer_tex_sub_image_3d(int x, int y, int z, int width, int height, int depth, const void *data) {
    (void)x;
    (void)y;
    (void)z;
    (void)width;
    (void)height;
    (void)depth;
    (void)data;
    /* TODO: Implement sub-image upload */
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
    /* TODO: Implement mipmap generation */
}

void renderer_bind_image_texture(int unit, R_Texture texture, R_Access access) {
    (void)unit;
    (void)texture;
    (void)access;
    /* TODO: Implement compute shader image binding */
}

/* ============================================================================
 * Public API: Drawing
 * ============================================================================ */

void renderer_draw_arrays(R_Primitive primitive, int first, int count) {
    if (g_active_cmd == VK_NULL_HANDLE) return;
    if (g_vk.active_pipeline >= g_vk.pipeline_count) return;
    
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
                           0, 256, g_push_constants);
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
    
    /* Set viewport - Vulkan native (no Y-flip) */
    VkViewport viewport = {0};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)g_vk.swap_extent.width;
    viewport.height = (float)g_vk.swap_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(g_active_cmd, 0, 1, &viewport);

    /* Set scissor */
    VkRect2D scissor = {0};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = g_vk.swap_extent;
    vkCmdSetScissor(g_active_cmd, 0, 1, &scissor);

    /* Apply dynamic state */
    vkCmdSetLineWidth(g_active_cmd, g_vk.line_width);
    if (g_vk.poly_offset_factor != 0.0f || g_vk.poly_offset_units != 0.0f) {
        vkCmdSetDepthBias(g_active_cmd, g_vk.poly_offset_factor, 0.0f, 0.0f);
    } else {
        vkCmdSetDepthBias(g_active_cmd, 0.0f, 0.0f, 0.0f);
    }

    /* Draw */
    (void)primitive; /* Pipeline already has the topology set */
    vkCmdDraw(g_active_cmd, count, 1, first, 0);
}

void renderer_draw_elements(R_Primitive primitive, int count, int offset) {
    if (g_active_cmd == VK_NULL_HANDLE) return;
    if (g_vk.active_pipeline >= g_vk.pipeline_count) return;
    if (g_vk.bound_index_buffer == VK_NULL_HANDLE) return;
    
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
                           0, 256, g_push_constants);
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

    /* Set viewport - Vulkan native (no Y-flip) */
    VkViewport viewport = {0};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)g_vk.swap_extent.width;
    viewport.height = (float)g_vk.swap_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(g_active_cmd, 0, 1, &viewport);

    /* Set scissor */
    VkRect2D scissor = {0};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = g_vk.swap_extent;
    vkCmdSetScissor(g_active_cmd, 0, 1, &scissor);

    /* Apply dynamic state */
    vkCmdSetLineWidth(g_active_cmd, g_vk.line_width);
    if (g_vk.poly_offset_factor != 0.0f || g_vk.poly_offset_units != 0.0f) {
        vkCmdSetDepthBias(g_active_cmd, g_vk.poly_offset_factor, 0.0f, 0.0f);
    } else {
        vkCmdSetDepthBias(g_active_cmd, 0.0f, 0.0f, 0.0f);
    }

    /* Draw indexed */
    (void)primitive; /* Pipeline already has the topology set */
    vkCmdDrawIndexed(g_active_cmd, count, 1, 0, offset, 0);
}

void renderer_draw_arrays_indirect(void) {
    /* TODO: Implement indirect drawing for compute-generated meshes */
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
