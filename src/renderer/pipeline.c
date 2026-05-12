#include "renderer/renderer_internal.h"
/* ============================================================================
 * Shader loading
 * ============================================================================ */

char* load_spirv_file(const char *path, size_t *out_size) {
    char spv_path[256];
    snprintf(spv_path, sizeof(spv_path), "%s.spv", path);

    char *resolved = platform_resolve_path(spv_path);
    if (!resolved) return NULL;

    FILE *f = fopen(resolved, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open shader: %s\n", resolved);
        free(resolved);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fprintf(stderr, "Invalid shader file size: %s (size=%ld)\n", resolved, size);
        fclose(f);
        free(resolved);
        return NULL;
    }

    char *buf = malloc((size_t)size);
    if (!buf) {
        fprintf(stderr, "Failed to allocate shader buffer: %s\n", resolved);
        fclose(f);
        free(resolved);
        return NULL;
    }

    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "Failed to read shader: %s\n", resolved);
        free(buf);
        fclose(f);
        free(resolved);
        return NULL;
    }
    fclose(f);
    free(resolved);
    *out_size = (size_t)size;
    return buf;
}

/* ============================================================================
 * Pipeline creation
 * ============================================================================ */

VkShaderModule load_shader_module(const char *path) {
    size_t size;
    char *code = load_spirv_file(path, &size);
    if (!code) return VK_NULL_HANDLE;
    
    VkShaderModule module = create_shader_module(code, size);
    free(code);
    return module;
}

VkDescriptorSetLayout create_texture_descriptor_layout(void) {
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
    if (vkCreateDescriptorSetLayout(g_vk.device, &create_info, NULL, &layout) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return layout;
}

VkPipelineLayout create_pipeline_layout(VkDescriptorSetLayout tex_layout) {
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
    if (vkCreatePipelineLayout(g_vk.device, &create_info, NULL, &layout) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return layout;
}



void get_pipeline_config(const char *vert_path, const char *frag_path, PipelineConfig *cfg) {
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
        cfg->cull_mode = VK_CULL_MODE_NONE; /* Disable culling - cube has mixed winding, depth test handles visibility */
        cfg->has_texture = false;
        cfg->push_constant_size = 128;
    } else if (strstr(vert_path, "basic")) {
        cfg->cull_mode = VK_CULL_MODE_NONE; /* Disable culling to match OpenGL behavior */
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
        cfg->topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        cfg->depth_test_enable = VK_FALSE;
        cfg->depth_write_enable = VK_FALSE;
        cfg->cull_mode = VK_CULL_MODE_NONE;
        cfg->blend_enable = VK_TRUE;
        cfg->has_texture = false;
        cfg->push_constant_size = 16; /* uColor + uAlpha */
    }
}

VkPipeline create_graphics_pipeline(VkShaderModule vert, VkShaderModule frag,
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
            /* UI: pos2, uv2, color4 - stride=20 */
            binding.stride = 20;

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

            /* Location 2: aColor (u8 x 4) */
            attrs[2].location = 2;
            attrs[2].binding = 0;
            attrs[2].format = VK_FORMAT_R8G8B8A8_UNORM;
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
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
    VkPipelineRenderingCreateInfo rendering_create_info = {0};
    rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering_create_info.colorAttachmentCount = 1;
    rendering_create_info.pColorAttachmentFormats = &g_vk.swap_format;
    rendering_create_info.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

    create_info.layout = layout;
    create_info.pNext = &rendering_create_info;
    create_info.renderPass = VK_NULL_HANDLE;

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(g_vk.device, g_pipeline_cache, 1, &create_info, NULL, &pipeline) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return pipeline;
}

/* ============================================================================
 * Public API: Programs and uniforms
 * ============================================================================ */


R_Program renderer_create_program(const char *vert_path, const char *frag_path) {
    CHECK_DEVICE_RET(R_INVALID_HANDLE);
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
        if (vkCreateDescriptorSetLayout(g_vk.device, &layout_info, NULL, &pipe->desc_set_layout) != VK_SUCCESS) {
            pipe->desc_set_layout = VK_NULL_HANDLE;
        }
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
        if (vkAllocateDescriptorSets(g_vk.device, &alloc_info, &pipe->desc_set) != VK_SUCCESS) {
            pipe->desc_set = VK_NULL_HANDLE;
        }
    } else {
        pipe->desc_set = VK_NULL_HANDLE;
    }

    return idx;
}

R_Program renderer_create_compute(const char *comp_path) {
    (void)comp_path;
    return R_INVALID_HANDLE;
}

void renderer_destroy_program(R_Program program) {
    CHECK_DEVICE();
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
    CHECK_DEVICE();
    g_vk.active_pipeline = program;
    g_push_dirty = false;
}

int renderer_uniform_location(R_Program program, const char *name) {
    if (program == R_INVALID_HANDLE || !name) return -1;
    /* Map uniform names to push constant offsets */
    int base_offset = 192; /* After 3 mat4 matrices (3 * 64 = 192) */
    
    for (int i = 0; i < g_uniform_count; i++) {
        if (g_uniforms[i].program == program && strcmp(g_uniforms[i].name, name) == 0) {
            return i;
        }
    }
    
    if (g_uniform_count >= 32) return -1;
    
    int offset = base_offset + (g_uniform_count * 16); /* 16 bytes per uniform vec4 slot */
    g_uniforms[g_uniform_count].program = program;
    strncpy(g_uniforms[g_uniform_count].name, name, 63);
    g_uniforms[g_uniform_count].name[63] = '\0';
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
