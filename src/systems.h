#pragma once
#include "ecs.h"
#include "kv.h"

#define GRAVITY            25.0f
#define JUMP_VELOCITY      10.0f
#define PLAYER_HEIGHT       1.6f
#define PLAYER_EYES_HEIGHT  1.4f
#define PLAYER_HALF_WIDTH   0.3f

void sys_movement(world_t *ecs, kv_world_t *world, float dt);
