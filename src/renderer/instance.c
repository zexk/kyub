#include "renderer/renderer_internal.h"
#if !defined(PLATFORM_WIN32)
#include <X11/Xlib.h>
#endif

#ifdef ENABLE_VALIDATION
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *user_data) {
    (void)message_type;
    (void)user_data;
    if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        fprintf(stderr, "[Vulkan] %s\n", callback_data->pMessage);
    }
    return VK_FALSE;
}
#endif

/* ============================================================================
 * Instance and device creation
 * ============================================================================ */

bool create_instance(void) {
    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Kyub";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "KyubEngine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(PLATFORM_WIN32)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#else
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
#ifdef ENABLE_VALIDATION
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    };

#ifdef ENABLE_VALIDATION
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);
    bool validation_available = false;
    if (layer_count > 0) {
        VkLayerProperties *layers = malloc(sizeof(VkLayerProperties) * layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, layers);
        for (uint32_t i = 0; i < layer_count; i++) {
            if (strcmp(layers[i].layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                validation_available = true;
                break;
            }
        }
        free(layers);
    }
    if (!validation_available) {
        fprintf(stderr, "Warning: VK_LAYER_KHRONOS_validation not available, disabling validation\n");
    }
#endif

    VkInstanceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
    create_info.ppEnabledExtensionNames = extensions;
#ifdef ENABLE_VALIDATION
    if (validation_available) {
        const char *validation_layers[] = {
            "VK_LAYER_KHRONOS_validation",
        };
        create_info.enabledLayerCount = sizeof(validation_layers) / sizeof(validation_layers[0]);
        create_info.ppEnabledLayerNames = validation_layers;
    }
#endif

    if (vkCreateInstance(&create_info, NULL, &g_vk.instance) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance\n");
        return false;
    }

#ifdef ENABLE_VALIDATION
    if (validation_available) {
        PFN_vkCreateDebugUtilsMessengerEXT create_fn =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                g_vk.instance, "vkCreateDebugUtilsMessengerEXT");
        if (create_fn) {
            VkDebugUtilsMessengerCreateInfoEXT debug_info = {0};
            debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                       | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                       | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                                   | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                   | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debug_info.pfnUserCallback = debug_callback;
            create_fn(g_vk.instance, &debug_info, NULL, &g_debug_messenger);
        }
    }
#endif

    return true;
}

bool select_physical_device(void) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_vk.instance, &device_count, NULL);
    if (device_count == 0) {
        fprintf(stderr, "No Vulkan-compatible devices found\n");
        return false;
    }
    
    VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * device_count);
    if (!devices) {
        fprintf(stderr, "Failed to allocate device list\n");
        return false;
    }
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

bool find_queue_families(void) {
    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_vk.physical_device, &queue_count, NULL);
    
    VkQueueFamilyProperties *queues = malloc(sizeof(VkQueueFamilyProperties) * queue_count);
    if (!queues) {
        fprintf(stderr, "Failed to allocate queue families\n");
        return false;
    }
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
    
    for (uint32_t i = 0; i < queue_count; i++) {
        VkBool32 supports_present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(g_vk.physical_device, i, g_vk.surface, &supports_present);
        if (supports_present) {
            g_vk.present_family = i;
            break;
        }
    }
    
    free(queues);
    if (g_vk.graphics_family == UINT32_MAX) {
        fprintf(stderr, "No graphics queue family found\n");
        return false;
    }
    if (g_vk.present_family == UINT32_MAX) {
        fprintf(stderr, "No present queue family found\n");
        return false;
    }
    return true;
}

bool create_device(void) {
    float queue_priority = 1.0f;

    uint32_t unique_families[3];
    uint32_t unique_count = 0;

    unique_families[unique_count++] = g_vk.graphics_family;
    if (g_vk.compute_family != g_vk.graphics_family && g_vk.compute_family != UINT32_MAX) {
        unique_families[unique_count++] = g_vk.compute_family;
    }
    if (g_vk.present_family != g_vk.graphics_family &&
        g_vk.present_family != g_vk.compute_family &&
        g_vk.present_family != UINT32_MAX) {
        unique_families[unique_count++] = g_vk.present_family;
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

    VkPhysicalDeviceVulkan13Features vulkan_13 = {0};
    vulkan_13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan_13.dynamicRendering = VK_TRUE;
    vulkan_13.synchronization2 = VK_TRUE;

    VkDeviceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = &vulkan_13;
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

    VkPipelineCacheCreateInfo cache_info = {0};
    cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    vkCreatePipelineCache(g_vk.device, &cache_info, NULL, &g_pipeline_cache);

    return true;
}

bool create_surface(void) {
#if defined(PLATFORM_WIN32)
    VkWin32SurfaceCreateInfoKHR create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_info.hinstance = platform_win_get_instance();
    create_info.hwnd = platform_win_get_window();

    if (vkCreateWin32SurfaceKHR(g_vk.instance, &create_info, NULL, &g_vk.surface) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Win32 surface\n");
        return false;
    }
#else
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
#endif
    return true;
}
bool create_descriptor_pool(void) {
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32},
    };
    
    VkDescriptorPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]);
    pool_info.pPoolSizes = pool_sizes;
    pool_info.maxSets = 256;
    
    VkResult res = vkCreateDescriptorPool(g_vk.device, &pool_info, NULL, &g_vk.desc_pool);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "Failed to create descriptor pool\n");
        return false;
    }
    return true;
}

bool create_default_sampler(void) {
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
    
    VkResult res = vkCreateSampler(g_vk.device, &create_info, NULL, &g_vk.default_sampler);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "Failed to create default sampler\n");
        return false;
    }
    return true;
}

void init_resource_arrays(void) {
    g_vk.pipelines = calloc(MAX_PIPELINES, sizeof(Pipeline));
    g_vk.buffers = calloc(MAX_BUFFERS, sizeof(VkBuffer));
    g_vk.buffer_memories = calloc(MAX_BUFFERS, sizeof(VkDeviceMemory));
    g_vk.buffer_sizes = calloc(MAX_BUFFERS, sizeof(uint64_t));
    g_vk.textures = calloc(MAX_TEXTURES, sizeof(VkImage));
    g_vk.texture_memories = calloc(MAX_TEXTURES, sizeof(VkDeviceMemory));
    g_vk.texture_views = calloc(MAX_TEXTURES, sizeof(VkImageView));
    g_vk.texture_samplers = calloc(MAX_TEXTURES, sizeof(VkSampler));
    g_vk.texture_widths = calloc(MAX_TEXTURES, sizeof(uint32_t));
    g_vk.texture_heights = calloc(MAX_TEXTURES, sizeof(uint32_t));
    g_vk.texture_depths = calloc(MAX_TEXTURES, sizeof(uint32_t));
    g_vk.vaos = calloc(MAX_VAO, sizeof(VkBuffer));

    g_vk.buffer_count = 0;
    g_vk.texture_count = 0;
    g_vk.vao_count = 0;
    g_vk.pipeline_count = 0;
    g_vk.active_pipeline = 0;

    g_vk.bound_vbo = VK_NULL_HANDLE;
    g_vk.bound_index_buffer = VK_NULL_HANDLE;
}
