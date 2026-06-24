#include "components.h"

component_id_t COMP_TRANSFORM;
component_id_t COMP_MOVEMENT;
component_id_t COMP_HEALTH;

world_t *g_ecs = NULL;

void components_init(world_t *world) {
    COMP_TRANSFORM = component_register(world, "transform", sizeof(C_Transform), _Alignof(C_Transform));
    COMP_MOVEMENT  = component_register(world, "movement",  sizeof(C_Movement),  _Alignof(C_Movement));
    COMP_HEALTH    = component_register(world, "health",    sizeof(C_Health),    _Alignof(C_Health));
}
