#ifndef SYSTEMS_H
#define SYSTEMS_H

#include "ecs.h"
#include "world.h"

void sys_movement(ECS *ecs, World *world, float dt);

#endif // SYSTEMS_H
