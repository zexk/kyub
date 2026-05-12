#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "ecs.h"
#include "math3d.h"
#include "voxel.h"
#include <stdbool.h>

/* Component type IDs — assigned at runtime during ecs_register */
extern int COMP_TRANSFORM;
extern int COMP_BLOCK_DEF;
extern int COMP_MOVEMENT;
extern int COMP_HEALTH;

/* Global ECS instance and block entity lookup table */
extern ECS g_ecs;
extern Entity g_block_entities[256];

/* --- Transform: spatial position + orientation --- */
typedef struct {
    vec3 position;
    float yaw;
    float pitch;
} C_Transform;

/* --- Block Definition: static properties of a block type --- */
typedef struct {
    const char *name;
    bool  solid;
    bool  opaque;
    float uv_u, uv_v, uv_w, uv_h;
    float hardness;
} C_BlockDef;

/* --- Movement: physics state --- */
typedef struct {
    vec3 velocity;
    float speed;
    bool  grounded;
} C_Movement;

/* --- Health: hit points --- */
typedef struct {
    float current;
    float max;
} C_Health;

/* Register all component types */
void components_init(ECS *ecs);

/* Register one block type as a static entity */
Entity register_block_type(ECS *ecs, BlockType type, const char *name, bool solid, bool opaque,
                            float u, float v, float w, float h, float hardness);

#endif // COMPONENTS_H
