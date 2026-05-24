CC ?= gcc
RENDERER ?= vulkan

ifeq ($(OS),Windows_NT)
  EXEEXT ?= .exe
  DEFAULT_PLATFORM = win32
else
  DEFAULT_PLATFORM = x11
endif
PLATFORM ?= $(DEFAULT_PLATFORM)

# ---------------------------------------------------------------------------
# Renderer selection
# ---------------------------------------------------------------------------
ifeq ($(RENDERER),opengl)
  RENDERER_DEF = -DRENDERER_OPENGL
  RENDERER_SRCS = \
    $(SRC_DIR)/renderer/renderer_gl.c \
    $(SRC_DIR)/glad.c
  GL_SHADERS = $(wildcard $(SHADER_SRC_DIR)/*.gl.vert $(SHADER_SRC_DIR)/*.gl.frag)
  GL_SHADER_OUT = $(patsubst $(SHADER_SRC_DIR)/%,$(SHADER_OUT_DIR)/%,$(GL_SHADERS))
  SHADER_TARGETS = gl-shaders
else
  RENDERER_DEF = -DRENDERER_VULKAN
  RENDERER_SRCS = \
    $(SRC_DIR)/renderer/renderer.c \
    $(SRC_DIR)/renderer/instance.c \
    $(SRC_DIR)/renderer/swapchain.c \
    $(SRC_DIR)/renderer/command.c \
    $(SRC_DIR)/renderer/pipeline.c \
    $(SRC_DIR)/renderer/buffer.c \
    $(SRC_DIR)/renderer/texture.c \
    $(SRC_DIR)/renderer/draw.c
  SHADER_TARGETS = shaders
endif

ifeq ($(PLATFORM),win32)
  PLATFORM_DEF = -DPLATFORM_WIN32
  PLATFORM_SRCS = $(SRC_DIR)/platform/platform_win.c
  PLATFORM_LIBS = -luser32 -lgdi32 -lshell32
  ifeq ($(RENDERER),opengl)
    RENDERER_LIBS = -lopengl32
  else
    RENDERER_LIBS = -lvulkan-1
  endif
  LDFLAGS_PLATFORM =
else
  PLATFORM_DEF = -DPLATFORM_X11
  PLATFORM_SRCS = $(SRC_DIR)/platform/platform_x11.c
  PLATFORM_LIBS = -lX11 -lrt
  ifeq ($(RENDERER),opengl)
    RENDERER_LIBS = -lGL
  else
    RENDERER_LIBS = -lvulkan
  endif
endif

CFLAGS = -std=c99 -Wall -Wextra -g -Iinclude $(RENDERER_DEF) $(PLATFORM_DEF) -DENABLE_LOGGER $(CFLAGS_EXTRA)
LDFLAGS = $(PLATFORM_LIBS) $(RENDERER_LIBS) -lm

SRC_DIR = src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj/$(PLATFORM)-$(RENDERER)
SHADER_SRC_DIR = shaders
SHADER_OUT_DIR = $(BUILD_DIR)/shaders
TARGET = $(BUILD_DIR)/kyub$(EXEEXT)

# ---------------------------------------------------------------------------
# SPIR-V shaders (Vulkan only)
# ---------------------------------------------------------------------------
GLSL_VERT = $(filter-out %.gl.vert,$(wildcard $(SHADER_SRC_DIR)/*.vert))
GLSL_FRAG = $(filter-out %.gl.frag,$(wildcard $(SHADER_SRC_DIR)/*.frag))
GLSL_COMP = $(wildcard $(SHADER_SRC_DIR)/*.comp)
SPIRV_VERT = $(patsubst $(SHADER_SRC_DIR)/%.vert,$(SHADER_OUT_DIR)/%.spv,$(GLSL_VERT))
SPIRV_FRAG = $(patsubst $(SHADER_SRC_DIR)/%.frag,$(SHADER_OUT_DIR)/%.spv,$(GLSL_FRAG))
SPIRV_COMP = $(patsubst $(SHADER_SRC_DIR)/%.comp,$(SHADER_OUT_DIR)/%.spv,$(GLSL_COMP))
SPIRV_ALL = $(SPIRV_VERT) $(SPIRV_FRAG) $(SPIRV_COMP)

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
COMMON_SRCS = \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/voxel.c \
	$(SRC_DIR)/mesh.c \
	$(SRC_DIR)/math3d.c \
	$(SRC_DIR)/camera.c \
	$(SRC_DIR)/noise.c \
	$(SRC_DIR)/world.c \
	$(SRC_DIR)/texture.c \
	$(SRC_DIR)/gui.c \
	$(SRC_DIR)/ecs.c \
	$(SRC_DIR)/components.c \
	$(SRC_DIR)/systems.c \
	$(SRC_DIR)/logger.c \
	$(SRC_DIR)/platform/platform.c \
	$(SRC_DIR)/platform/game_input.c \
	$(PLATFORM_SRCS)

SRCS = $(COMMON_SRCS) $(RENDERER_SRCS)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

.PHONY: all clean shaders release

all: $(SHADER_TARGETS) $(TARGET)

release: CFLAGS = -std=c99 -Wall -Wextra -O2 -DNDEBUG -Iinclude $(RENDERER_DEF) $(PLATFORM_DEF) $(CFLAGS_EXTRA)
release: LDFLAGS = $(PLATFORM_LIBS) $(RENDERER_LIBS) -lm
release: clean all

shaders: $(SPIRV_ALL)

gl-shaders: $(GL_SHADER_OUT)

$(SHADER_OUT_DIR)/%.gl.vert: $(SHADER_SRC_DIR)/%.gl.vert | $(SHADER_OUT_DIR)
	cp $< $@

$(SHADER_OUT_DIR)/%.gl.frag: $(SHADER_SRC_DIR)/%.gl.frag | $(SHADER_OUT_DIR)
	cp $< $@

$(SHADER_OUT_DIR)/%.spv: $(SHADER_SRC_DIR)/%.vert | $(SHADER_OUT_DIR)
	glslangValidator -V --target-env vulkan1.2 -S vert $< -o $@

$(SHADER_OUT_DIR)/%.spv: $(SHADER_SRC_DIR)/%.frag | $(SHADER_OUT_DIR)
	glslangValidator -V --target-env vulkan1.2 -S frag $< -o $@

$(SHADER_OUT_DIR)/%.spv: $(SHADER_SRC_DIR)/%.comp | $(SHADER_OUT_DIR)
	glslangValidator -V --target-env vulkan1.2 -S comp $< -o $@

$(SHADER_OUT_DIR):
	mkdir -p $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
