#ifndef RENDERER_INTERNAL_H
#define RENDERER_INTERNAL_H

#if defined(PLATFORM_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#else
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include "renderer/renderer.h"
#include "platform/platform.h"
#if defined(PLATFORM_WIN32)
#include "platform/platform_win.h"
#else
#include "platform/platform_x11.h"
#endif
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#define MAX_FRAMES_IN_FLIGHT R_MAX_FRAMES_IN_FLIGHT
#define MAX_PIPELINES R_MAX_PIPELINES
#define MAX_BUFFERS R_MAX_BUFFERS
#define MAX_TEXTURES R_MAX_TEXTURES
#define MAX_VAO R_MAX_VAO
#define FENCE_TIMEOUT_NS R_FENCE_TIMEOUT_NS

typedef struct {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkDescriptorSetLayout desc_set_layout;
    VkDescriptorSet desc_set;
    VkShaderModule vert_module;
    VkShaderModule frag_module;
} Pipeline;

#define CHECK_DEVICE() do { if (g_vk.device == VK_NULL_HANDLE) return; } while(0)
#define CHECK_DEVICE_RET(ret) do { if (g_vk.device == VK_NULL_HANDLE) return (ret); } while(0)

typedef struct {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue compute_queue;
    VkQueue present_queue;
    uint32_t graphics_family;
    uint32_t compute_family;
    uint32_t present_family;
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
    VkCommandPool cmd_pool;
    VkCommandBuffer *cmd_buffers;
    VkSemaphore *image_avail_sems;
    VkSemaphore *render_done_sems;
    VkFence *in_flight_fences;
    uint32_t current_frame;
    uint32_t image_index;
    bool framebuffer_resized;
    int width, height;
    VkDescriptorPool desc_pool;
    Pipeline *pipelines;
    uint32_t pipeline_count;
    uint32_t active_pipeline;
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
    uint32_t *texture_widths;
    uint32_t *texture_heights;
    uint32_t *texture_depths;
    VkBuffer *vaos;
    uint32_t vao_count;
    VkBuffer vao_buffers[MAX_VAO];
    VkBuffer vao_index_buffers[MAX_VAO];
    VkCullModeFlagBits cull_mode;
    VkPrimitiveTopology topology;
    float line_width;
    float poly_offset_factor;
    float poly_offset_units;
    VkBuffer bound_vbo;
    R_Buffer bound_vbo_handle;
    VkBuffer bound_index_buffer;
    R_Buffer bound_ibo_handle;
    VkPipeline active_pipeline_handle;
    VkPresentModeKHR present_mode;
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    VkDeviceSize staging_size;
} VulkanContext;

extern VulkanContext g_vk;

extern VkPipelineCache g_pipeline_cache;

#ifdef ENABLE_VALIDATION
extern VkDebugUtilsMessengerEXT g_debug_messenger;
#endif

extern VkCommandBuffer g_active_cmd;
extern bool g_frame_started;
extern bool g_in_render_pass;
extern float g_clear_color[4];
extern bool g_clear_depth;

typedef struct {
    R_Program program;
    char name[64];
    int offset;
} UniformMapping;

extern UniformMapping g_uniforms[32];
extern int g_uniform_count;
extern uint8_t g_push_constants[256];
extern bool g_push_dirty;

extern int g_active_texture_unit;
extern R_Texture g_bound_textures[16];

typedef struct {
    VkBuffer buffer;
    VkBuffer index_buffer;
} VAOState;

extern VAOState g_vao_state;
extern R_VAO g_current_vao;

/* Helper functions */
extern uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags props);
extern VkShaderModule create_shader_module(const char *code, size_t size);
extern VkBuffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkDeviceMemory *out_memory);

/* Instance/device */
extern bool create_instance(void);
extern bool select_physical_device(void);
extern bool find_queue_families(void);
extern bool create_device(void);
extern bool create_surface(void);

/* Swapchain */
extern bool create_swapchain(void);
extern bool create_swap_image_views(void);
extern bool create_depth_buffer(void);
extern void cleanup_swapchain(void);
extern bool recreate_swapchain(void);

/* Command/sync */
extern bool create_command_pool(void);
extern bool create_command_buffers(void);
extern bool create_sync_objects(void);

/* Resources */
extern bool create_descriptor_pool(void);
extern bool create_default_sampler(void);
extern void init_resource_arrays(void);

/* Pipeline */
typedef enum {
    VERTEX_FORMAT_TERRAIN,
    VERTEX_FORMAT_SKYBOX,
    VERTEX_FORMAT_OUTLINE,
    VERTEX_FORMAT_HUD,
    VERTEX_FORMAT_UI,
} VertexFormat;

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
    bool has_texture;
    VkBool32 depth_bias_enable;
    uint32_t push_constant_size;
} PipelineConfig;

extern char* load_spirv_file(const char *path, size_t *out_size);
extern VkShaderModule load_shader_module(const char *path);
extern VkDescriptorSetLayout create_texture_descriptor_layout(void);
extern VkPipelineLayout create_pipeline_layout(VkDescriptorSetLayout tex_layout);
extern void get_pipeline_config(const char *vert_path, const char *frag_path, PipelineConfig *cfg);
extern VkPipeline create_graphics_pipeline(VkShaderModule vert, VkShaderModule frag,
                                           VkPipelineLayout layout, const PipelineConfig *cfg);

/* Buffer helpers */
extern void copy_to_buffer(VkBuffer dst, VkDeviceSize dst_offset, VkDeviceSize size, const void *data);

/* Texture helpers */
extern VkImage create_image(uint32_t width, uint32_t height, uint32_t depth, uint32_t array_layers,
                             VkFormat format, VkImageUsageFlags usage, VkDeviceMemory *out_memory);
extern VkImageView create_image_view(VkImage image, VkFormat format, VkImageViewType view_type);
extern bool transition_image_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);
extern void upload_image_data(VkImage image, uint32_t width, uint32_t height, uint32_t depth,
                               const void *data, VkDeviceSize data_size);

#endif
