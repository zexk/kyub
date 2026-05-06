#include "camera.h"
#include "input.h"
#include <math.h>

static bool position_is_safe(World *world, vec3 pos) {
    int head_y = (int)floorf(pos.y + (PLAYER_HEIGHT - PLAYER_EYES_HEIGHT));
    int feet_y = (int)floorf(pos.y - PLAYER_EYES_HEIGHT);

    float hw = PLAYER_HALF_WIDTH;
    float min_x = pos.x - hw;
    float max_x = pos.x + hw;
    float min_z = pos.z - hw;
    float max_z = pos.z + hw;

    for (int y = feet_y; y <= head_y; y++) {
        int xi1 = (int)floorf(min_x);
        int xi2 = (int)floorf(max_x);
        int zi1 = (int)floorf(min_z);
        int zi2 = (int)floorf(max_z);

        if (world_is_solid(world, xi1, y, zi1)) return false;
        if (world_is_solid(world, xi1, y, zi2)) return false;
        if (world_is_solid(world, xi2, y, zi1)) return false;
        if (world_is_solid(world, xi2, y, zi2)) return false;
    }
    return true;
}

void camera_init(Camera *cam) {
    cam->pos = (vec3){8.0f, 20.0f, 8.0f};
    cam->front = (vec3){0.0f, -1.0f, 0.0f};
    cam->up = (vec3){0.0f, 1.0f, 0.0f};
    cam->yaw = -90.0f;
    cam->pitch = -45.0f;
    cam->speed = 5.0f;
    cam->sensitivity = 0.1f;
    cam->velocity = (vec3){0.0f, 0.0f, 0.0f};
    cam->grounded = false;
}

static void update_vectors(Camera *cam) {
    vec3 front;
    front.x = cosf(cam->yaw * PI / 180.0f) * cosf(cam->pitch * PI / 180.0f);
    front.y = sinf(cam->pitch * PI / 180.0f);
    front.z = sinf(cam->yaw * PI / 180.0f) * cosf(cam->pitch * PI / 180.0f);
    cam->front = vec3_normalize(front);
}

void camera_update(Camera *cam, float dt, World *world) {
    // Rotation
    cam->yaw += g_input.mouse_dx * cam->sensitivity;
    cam->pitch -= g_input.mouse_dy * cam->sensitivity;

    if (cam->pitch > 89.0f) cam->pitch = 89.0f;
    if (cam->pitch < -89.0f) cam->pitch = -89.0f;

    update_vectors(cam);

    float max_step = 0.3f;

    // Movement (horizontal only - vertical handled by physics)
    float velocity = cam->speed * dt;
    if (g_input.shift) velocity *= 6.0f;

    vec3 move_dir = {0};
    if (g_input.keys['w']) move_dir = vec3_add(move_dir, cam->front);
    if (g_input.keys['s']) move_dir = vec3_sub(move_dir, cam->front);

    vec3 right = vec3_normalize(vec3_cross(cam->front, cam->up));
    if (g_input.keys['a']) move_dir = vec3_sub(move_dir, right);
    if (g_input.keys['d']) move_dir = vec3_add(move_dir, right);

    // Apply horizontal movement with collision
    if (velocity > max_step) velocity = max_step;

    if (move_dir.x != 0 || move_dir.z != 0) {
        move_dir = vec3_normalize(move_dir);

        float new_x = cam->pos.x + move_dir.x * velocity;
        float new_z = cam->pos.z + move_dir.z * velocity;

        float orig_x = cam->pos.x;
        float orig_z = cam->pos.z;

        // Check X separately for sliding
        vec3 test_pos = {new_x, cam->pos.y, orig_z};
        if (position_is_safe(world, test_pos)) {
            cam->pos.x = new_x;
        }

        // Check Z separately for sliding (using original X)
        test_pos.x = orig_x;
        test_pos.z = new_z;
        if (position_is_safe(world, test_pos)) {
            cam->pos.z = new_z;
        }
    }

    // Physics: gravity
    cam->velocity.y -= GRAVITY * dt;
    cam->pos.y += cam->velocity.y * dt;

    // Ground collision - only check feet level
    if (cam->velocity.y < 0) {
        float feet_y = cam->pos.y - PLAYER_EYES_HEIGHT;
        int feet_cell = (int)floorf(feet_y);
        float hw = PLAYER_HALF_WIDTH;

        if (world_is_solid(world, (int)floorf(cam->pos.x - hw), feet_cell, (int)floorf(cam->pos.z - hw)) ||
            world_is_solid(world, (int)floorf(cam->pos.x + hw), feet_cell, (int)floorf(cam->pos.z - hw)) ||
            world_is_solid(world, (int)floorf(cam->pos.x - hw), feet_cell, (int)floorf(cam->pos.z + hw)) ||
            world_is_solid(world, (int)floorf(cam->pos.x + hw), feet_cell, (int)floorf(cam->pos.z + hw))) {

            cam->pos.y = (float)(feet_cell + 1) + PLAYER_EYES_HEIGHT;
            cam->velocity.y = 0.0f;
            cam->grounded = true;
        } else {
            cam->grounded = false;
        }
    } else {
        cam->grounded = false;
    }

    // Check if fell below world
    if (cam->pos.y < 0) {
        cam->pos.y = 20.0f;
        cam->pos.x = 8.0f;
        cam->pos.z = 8.0f;
        cam->velocity.y = 0.0f;
        cam->grounded = false;
    }

    // Jump
    if (g_input.keys[' '] && cam->grounded) {
        cam->velocity.y = JUMP_VELOCITY;
        cam->grounded = false;
    }

    // Reset mouse delta after processing
    g_input.mouse_dx = 0;
    g_input.mouse_dy = 0;
}

mat4 camera_get_view_matrix(Camera *cam) {
    return mat4_lookat(cam->pos, vec3_add(cam->pos, cam->front), cam->up);
}
