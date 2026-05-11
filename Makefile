CC ?= gcc
CFLAGS = -std=c99 -Wall -Wextra -g -Iinclude -DENABLE_COMPUTE -DENABLE_LOGGER -DRENDERER_VULKAN -DENABLE_VALIDATION $(CFLAGS_EXTRA)
LDFLAGS = -lX11 -lvulkan -lm -lrt

SRC_DIR = src
BUILD_DIR = build
SHADER_SRC_DIR = shaders
SHADER_OUT_DIR = $(BUILD_DIR)/shaders
TARGET = $(BUILD_DIR)/kyub

GLSL_VERT = $(wildcard $(SHADER_SRC_DIR)/*.vert)
GLSL_FRAG = $(wildcard $(SHADER_SRC_DIR)/*.frag)
GLSL_COMP = $(wildcard $(SHADER_SRC_DIR)/*.comp)
SPIRV_VERT = $(patsubst $(SHADER_SRC_DIR)/%.vert,$(SHADER_OUT_DIR)/%.spv,$(GLSL_VERT))
SPIRV_FRAG = $(patsubst $(SHADER_SRC_DIR)/%.frag,$(SHADER_OUT_DIR)/%.spv,$(GLSL_FRAG))
SPIRV_COMP = $(patsubst $(SHADER_SRC_DIR)/%.comp,$(SHADER_OUT_DIR)/%.spv,$(GLSL_COMP))
SPIRV_ALL = $(SPIRV_VERT) $(SPIRV_FRAG) $(SPIRV_COMP)

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/renderer_vulkan.c $(SRC_DIR)/voxel.c $(SRC_DIR)/mesh.c $(SRC_DIR)/math3d.c $(SRC_DIR)/camera.c $(SRC_DIR)/input.c $(SRC_DIR)/noise.c $(SRC_DIR)/world.c $(SRC_DIR)/texture.c $(SRC_DIR)/ui.c $(SRC_DIR)/platform.c $(SRC_DIR)/platform_x11.c $(SRC_DIR)/logger.c
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean shaders release

all: shaders $(TARGET)

release: CFLAGS = -std=c99 -Wall -Wextra -O2 -DNDEBUG -Iinclude -DENABLE_COMPUTE -DRENDERER_VULKAN $(CFLAGS_EXTRA)
release: LDFLAGS = -lX11 -lvulkan -lm -lrt
release: clean all

shaders: $(SPIRV_ALL)

$(SHADER_OUT_DIR)/%.spv: $(SHADER_SRC_DIR)/%.vert | $(SHADER_OUT_DIR)
	glslangValidator -V --target-env vulkan1.2 -S vert $< -o $@

$(SHADER_OUT_DIR)/%.spv: $(SHADER_SRC_DIR)/%.frag | $(SHADER_OUT_DIR)
	glslangValidator -V --target-env vulkan1.2 -S frag $< -o $@

$(SHADER_OUT_DIR)/%.spv: $(SHADER_SRC_DIR)/%.comp | $(SHADER_OUT_DIR)
	glslangValidator -V --target-env vulkan1.2 -S comp $< -o $@

$(SHADER_OUT_DIR):
	mkdir -p $@

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)
