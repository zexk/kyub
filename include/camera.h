#ifndef CAMERA_H
#define CAMERA_H

#include "math3d.h"

typedef struct World World;
bool world_is_solid(World *world, int x, int y, int z);

#define GRAVITY 20.0f
#define JUMP_VELOCITY 8.94f  // 2 blocks with gravity 20
#define PLAYER_HEIGHT 2.0f
#define PLAYER_EYES_HEIGHT 1.6f
#define PLAYER_HALF_WIDTH 0.3f

typedef struct {
    vec3 pos;
    vec3 front;
    vec3 up;
    float yaw;
    float pitch;
    float speed;
    float sensitivity;
    vec3 velocity;
    bool grounded;
} Camera;

void camera_init(Camera *cam);
void camera_update(Camera *cam, float dt, World *world);
mat4 camera_get_view_matrix(Camera *cam);

#endif // CAMERA_H
