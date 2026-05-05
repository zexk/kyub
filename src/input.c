#include "input.h"
#include <string.h>

Input g_input;

void input_init(void) {
    memset(&g_input, 0, sizeof(Input));
    g_input.mouse_captured = true;
}

void input_set_key(int key, bool pressed) {
    if (key >= 0 && key < 256) {
        g_input.keys[key] = pressed;
    }
}

void input_set_shift(bool pressed) {
    g_input.shift = pressed;
}

void input_set_mouse_delta(float dx, float dy) {
    g_input.mouse_dx = dx;
    g_input.mouse_dy = dy;
}
