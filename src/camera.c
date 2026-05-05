#include "camera.h"
#include "input.h"
#include <math.h>

void camera_init(Camera *cam) {
    cam->pos = (vec3){8.0f, 20.0f, 8.0f};
    cam->front = (vec3){0.0f, -1.0f, 0.0f};
    cam->up = (vec3){0.0f, 1.0f, 0.0f};
    cam->yaw = -90.0f;
    cam->pitch = -45.0f;
    cam->speed = 5.0f;
    cam->sensitivity = 0.1f;
}

static void update_vectors(Camera *cam) {
    vec3 front;
    front.x = cosf(cam->yaw * PI / 180.0f) * cosf(cam->pitch * PI / 180.0f);
    front.y = sinf(cam->pitch * PI / 180.0f);
    front.z = sinf(cam->yaw * PI / 180.0f) * cosf(cam->pitch * PI / 180.0f);
    cam->front = vec3_normalize(front);
}

void camera_update(Camera *cam, float dt) {
    // Rotation
    cam->yaw += g_input.mouse_dx * cam->sensitivity;
    cam->pitch -= g_input.mouse_dy * cam->sensitivity;

    if (cam->pitch > 89.0f) cam->pitch = 89.0f;
    if (cam->pitch < -89.0f) cam->pitch = -89.0f;

    update_vectors(cam);

    // Movement
    float velocity = cam->speed * dt;
    if (g_input.shift) velocity *= 6.0f;
    
    // TEMPORARY: Just for logic. In Phase 4 we will use correct X11 KeySyms.
    if (g_input.keys['w']) cam->pos = vec3_add(cam->pos, vec3_mul(cam->front, velocity));
    if (g_input.keys['s']) cam->pos = vec3_sub(cam->pos, vec3_mul(cam->front, velocity));
    
    vec3 right = vec3_normalize(vec3_cross(cam->front, cam->up));
    if (g_input.keys['a']) cam->pos = vec3_sub(cam->pos, vec3_mul(right, velocity));
    if (g_input.keys['d']) cam->pos = vec3_add(cam->pos, vec3_mul(right, velocity));
    
    // Reset mouse delta after processing
    g_input.mouse_dx = 0;
    g_input.mouse_dy = 0;
}

mat4 camera_get_view_matrix(Camera *cam) {
    return mat4_lookat(cam->pos, vec3_add(cam->pos, cam->front), cam->up);
}
