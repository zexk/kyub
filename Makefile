CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -g -Iinclude -DENABLE_COMPUTE -DENABLE_LOGGER $(CFLAGS_EXTRA)

# Renderer backend selection (opengl or vulkan)
RENDERER ?= vulkan

ifeq ($(RENDERER),vulkan)
    CFLAGS += -DRENDERER_VULKAN
    LDFLAGS = -lX11 -lvulkan -lm -lrt
    RENDERER_SRC = $(SRC_DIR)/renderer_vulkan.c
else
    CFLAGS +=
    LDFLAGS = -lX11 -lGL -lm -lrt
    RENDERER_SRC = $(SRC_DIR)/renderer_gl.c
endif

SRC_DIR = src
BUILD_DIR = build
SHADER_DIR = shaders
TARGET = $(BUILD_DIR)/kyub

# Shader source files (exclude .gl.* OpenGL-only shaders)
GLSL_VERT = $(filter-out %.gl.vert,$(wildcard $(SHADER_DIR)/*.vert))
GLSL_FRAG = $(filter-out %.gl.frag,$(wildcard $(SHADER_DIR)/*.frag))
GLSL_COMP = $(wildcard $(SHADER_DIR)/*.comp)
SPIRV_VERT = $(GLSL_VERT:.vert=.vert.spv)
SPIRV_FRAG = $(GLSL_FRAG:.frag=.frag.spv)
SPIRV_COMP = $(GLSL_COMP:.comp=.comp.spv)
SPIRV_ALL = $(SPIRV_VERT) $(SPIRV_FRAG) $(SPIRV_COMP)

SRCS = $(SRC_DIR)/main.c $(RENDERER_SRC) $(SRC_DIR)/voxel.c $(SRC_DIR)/mesh.c $(SRC_DIR)/math3d.c $(SRC_DIR)/camera.c $(SRC_DIR)/input.c $(SRC_DIR)/noise.c $(SRC_DIR)/world.c $(SRC_DIR)/texture.c $(SRC_DIR)/ui.c $(SRC_DIR)/platform.c $(SRC_DIR)/platform_x11.c $(SRC_DIR)/logger.c
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean shaders

all: shaders $(TARGET)

# SPIR-V shader compilation (only needed for Vulkan)
ifeq ($(RENDERER),vulkan)
shaders: $(SPIRV_ALL)

$(SHADER_DIR)/%.vert.spv: $(SHADER_DIR)/%.vert
	glslc $< -o $@

$(SHADER_DIR)/%.frag.spv: $(SHADER_DIR)/%.frag
	glslc $< -o $@

$(SHADER_DIR)/%.comp.spv: $(SHADER_DIR)/%.comp
	glslc $< -o $@
else
shaders:
endif

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(SHADER_DIR)/*.spv
