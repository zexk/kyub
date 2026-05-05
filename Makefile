CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -g -Iinclude
LDFLAGS = -lX11 -lGL -lm -lrt 

SRC_DIR = src
BUILD_DIR = build
TARGET = $(BUILD_DIR)/kyub

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/gl_ext.c $(SRC_DIR)/shader.c $(SRC_DIR)/voxel.c $(SRC_DIR)/mesh.c $(SRC_DIR)/math3d.c $(SRC_DIR)/camera.c $(SRC_DIR)/input.c $(SRC_DIR)/noise.c $(SRC_DIR)/world.c $(SRC_DIR)/texture.c
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
