#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

typedef struct {
    bool keys[256];
    bool shift;
    float mouse_dx, mouse_dy;
    int mouse_x, mouse_y;
    bool mouse_captured;
    bool mouse_left;
    bool mouse_right;
    bool key_f3;
} Input;

extern Input g_input;

void input_init(void);
void input_set_key(int key, bool pressed);
void input_set_shift(bool pressed);
void input_set_mouse_delta(float dx, float dy);
void input_set_mouse_pos(int x, int y);
void input_set_mouse_button(int button, bool pressed);

#endif // INPUT_H
