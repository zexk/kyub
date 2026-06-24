#pragma once

#include "ecs.h"
#include "linalg.h"
#include <stdbool.h>

extern component_id_t COMP_TRANSFORM;
extern component_id_t COMP_MOVEMENT;
extern component_id_t COMP_HEALTH;

extern world_t *g_ecs;

typedef struct {
    vec3_t position;
    float  yaw;
    float  pitch;
} C_Transform;

typedef struct {
    vec3_t velocity;
    float  speed;
    bool   grounded;
} C_Movement;

typedef struct {
    float current;
    float max;
} C_Health;

void components_init(world_t *world);
