#ifndef CAMERA_H
#define CAMERA_H

#include "math3d.h"

typedef struct {
    vec3 pos;
    vec3 front;
    vec3 up;
    float yaw;
    float pitch;
    float speed;
    float sensitivity;
} Camera;

void camera_init(Camera *cam);
void camera_update(Camera *cam, float dt);
mat4 camera_get_view_matrix(Camera *cam);

#endif // CAMERA_H
