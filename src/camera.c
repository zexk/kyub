#include "camera.h"
#include "platform/game_input.h"
#include "world.h"
#include "logger.h"
#include "components.h"
#include <math.h>

static bool position_is_safe(const World *world, vec3 pos) {
    float hw = PLAYER_HALF_WIDTH;
    float min_x = pos.x - hw;
    float max_x = pos.x + hw;
    float min_z = pos.z - hw;
    float max_z = pos.z + hw;

    for (int y = (int)(pos.y - PLAYER_EYES_HEIGHT); y <= (int)(pos.y - PLAYER_EYES_HEIGHT) + (int)PLAYER_HEIGHT + 1; y++) {
        int xi1 = (int)floorf(min_x);
        int xi2 = (int)floorf(max_x);
        int zi1 = (int)floorf(min_z);
        int zi2 = (int)floorf(max_z);

        if (world_is_solid(world, xi1, y, zi1)) return false;
        if (world_is_solid(world, xi1, y, zi2)) return false;
        if (world_is_solid(world, xi2, y, zi1)) return false;
        if (world_is_solid(world, xi2, y, zi2)) return false;

        float center_x = pos.x;
        float center_z = pos.z;
        if (world_is_solid(world, (int)floorf(center_x), y, (int)floorf(center_z))) return false;
    }
    return true;
}

void camera_init(Camera *cam, ECS *ecs) {
    cam->front = (vec3){0.0f, -1.0f, 0.0f};
    cam->up = (vec3){0.0f, 1.0f, 0.0f};
    cam->yaw = -90.0f;
    cam->pitch = -45.0f;
    cam->speed = 5.0f;
    cam->sensitivity = 0.1f;

    cam->player = ecs_spawn(ecs);
    C_Transform *transform = ecs_add(ecs, cam->player, COMP_TRANSFORM);
    transform->position = (vec3){8.0f, 20.0f, 8.0f};
    transform->yaw = cam->yaw;
    transform->pitch = cam->pitch;

    C_Movement *movement = ecs_add(ecs, cam->player, COMP_MOVEMENT);
    movement->velocity = (vec3){0.0f, 0.0f, 0.0f};
    movement->speed = cam->speed;
    movement->grounded = false;

    C_Health *health = ecs_add(ecs, cam->player, COMP_HEALTH);
    health->current = 20.0f;
    health->max = 20.0f;
}

static void update_vectors(Camera *cam) {
    vec3 front;
    front.x = cosf(cam->yaw * PI / 180.0f) * cosf(cam->pitch * PI / 180.0f);
    front.y = sinf(cam->pitch * PI / 180.0f);
    front.z = sinf(cam->yaw * PI / 180.0f) * cosf(cam->pitch * PI / 180.0f);
    cam->front = vec3_normalize(front);
}

void camera_update(Camera *cam, float dt, World *world, GameInput *gi, ECS *ecs) {
    static bool prev_w = false, prev_a = false, prev_s = false, prev_d = false;

    C_Transform *transform = ecs_get(ecs, cam->player, COMP_TRANSFORM);
    C_Movement *movement = ecs_get(ecs, cam->player, COMP_MOVEMENT);
    if (!transform || !movement) return;

    bool keys_w = gi->keys['w'];
    bool keys_a = gi->keys['a'];
    bool keys_s = gi->keys['s'];
    bool keys_d = gi->keys['d'];
    bool key_space = gi->keys[' '];
    bool key_shift = gi->shift;
    float mouse_dx = gi->mouse_dx;
    float mouse_dy = gi->mouse_dy;

    bool curr_w = keys_w;
    bool curr_a = keys_a;
    bool curr_s = keys_s;
    bool curr_d = keys_d;
    if (curr_w != prev_w || curr_a != prev_a || curr_s != prev_s || curr_d != prev_d) {
        LOG_DEBUG(CAT_INPUT, "camera_update key change: w=%d a=%d s=%d d=%d", curr_w, curr_a, curr_s, curr_d);
        prev_w = curr_w; prev_a = curr_a; prev_s = curr_s; prev_d = curr_d;
    }

    // Rotation
    cam->yaw += mouse_dx * cam->sensitivity;
    cam->pitch -= mouse_dy * cam->sensitivity;

    if (cam->pitch > 89.0f) cam->pitch = 89.0f;
    if (cam->pitch < -89.0f) cam->pitch = -89.0f;

    transform->yaw = cam->yaw;
    transform->pitch = cam->pitch;

    update_vectors(cam);

    float max_step = 0.3f;

    // Movement (horizontal only - vertical handled by sys_movement)
    float velocity = cam->speed * dt;
    if (key_shift) velocity *= 6.0f;

    vec3 move_dir = {0};
    if (keys_w) move_dir = vec3_add(move_dir, cam->front);
    if (keys_s) move_dir = vec3_sub(move_dir, cam->front);

    vec3 right = vec3_normalize(vec3_cross(cam->front, cam->up));
    if (keys_a) move_dir = vec3_sub(move_dir, right);
    if (keys_d) move_dir = vec3_add(move_dir, right);

    // Apply horizontal movement with collision
    if (velocity > max_step) velocity = max_step;

    if (move_dir.x != 0 || move_dir.z != 0) {
        move_dir = vec3_normalize(move_dir);

        float new_x = transform->position.x + move_dir.x * velocity;
        float new_z = transform->position.z + move_dir.z * velocity;

        float orig_x = transform->position.x;
        float orig_z = transform->position.z;

    // Check X separately for sliding
        vec3 test_pos = {new_x, transform->position.y, orig_z};
        if (position_is_safe(world, test_pos)) {
            transform->position.x = new_x;
        }

        // Check Z separately for sliding (using original X)
        test_pos.x = orig_x;
        test_pos.z = new_z;
        if (position_is_safe(world, test_pos)) {
            transform->position.z = new_z;
        }
    }

    // Jump
    if (key_space && movement->grounded) {
        movement->velocity.y = JUMP_VELOCITY;
        movement->grounded = false;
    }

    gi->mouse_dx = 0;
    gi->mouse_dy = 0;
}

mat4 camera_get_view_matrix(const Camera *cam, ECS *ecs) {
    C_Transform *transform = ecs_get(ecs, cam->player, COMP_TRANSFORM);
    if (!transform) {
        LOG_WARN(CAT_WORLD, "camera_get_view_matrix: player transform missing!");
        return mat4_identity();
    }
    return mat4_lookat(transform->position, vec3_add(transform->position, cam->front), cam->up);
}
