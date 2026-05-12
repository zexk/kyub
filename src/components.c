#include "components.h"
#include <string.h>

int COMP_TRANSFORM;
int COMP_BLOCK_DEF;
int COMP_MOVEMENT;
int COMP_HEALTH;

ECS g_ecs;
Entity g_block_entities[256];

void components_init(ECS *ecs) {
    COMP_TRANSFORM = ecs_register(ecs, sizeof(C_Transform));
    COMP_BLOCK_DEF  = ecs_register(ecs, sizeof(C_BlockDef));
    COMP_MOVEMENT   = ecs_register(ecs, sizeof(C_Movement));
    COMP_HEALTH     = ecs_register(ecs, sizeof(C_Health));
}

Entity register_block_type(ECS *ecs, BlockType type, const char *name, bool solid, bool opaque,
                            float u, float v, float w, float h, float hardness) {
    Entity e = ecs_spawn(ecs);
    C_BlockDef *def = ecs_add(ecs, e, COMP_BLOCK_DEF);
    def->name     = name;
    def->solid    = solid;
    def->opaque   = opaque;
    def->uv_u     = u;
    def->uv_v     = v;
    def->uv_w     = w;
    def->uv_h     = h;
    def->hardness = hardness;
    g_block_entities[type] = e;
    return e;
}
