#ifndef GAME_INPUT_H
#define GAME_INPUT_H

#include <stdbool.h>

typedef struct Event Event;

typedef struct GameInput {
    bool keys[256];
    bool shift;
    float mouse_dx, mouse_dy;
    int mouse_x, mouse_y;
    bool mouse_left;
    bool mouse_right;
    bool mouse_middle;
} GameInput;

void game_input_handle_event(GameInput *gi, const Event *event);

#endif
