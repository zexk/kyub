#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

typedef struct {
    bool keys[256];
    bool shift;
    float mouse_dx, mouse_dy;
    bool mouse_captured;
} Input;

extern Input g_input;

void input_init(void);
void input_set_key(int key, bool pressed);
void input_set_shift(bool pressed);
void input_set_mouse_delta(float dx, float dy);

#endif // INPUT_H
