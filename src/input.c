#include "input.h"
#include "platform.h"
#include "logger.h"
#include <string.h>

Input g_input;

void input_init(void) {
    memset(&g_input, 0, sizeof(Input));
    g_input.mouse_captured = true;
}

void input_set_key(int key, bool pressed) {
    if (key >= 0 && key < 256) {
        g_input.keys[key] = pressed;
        if (key == 'w' || key == 'a' || key == 's' || key == 'd' || key == ' ') {
            LOG_DEBUG(CAT_INPUT, "input_set_key: key=%c pressed=%d", key, pressed);
        }
    }
}

void input_set_shift(bool pressed) {
    g_input.shift = pressed;
}

void input_set_mouse_delta(float dx, float dy) {
    g_input.mouse_dx += dx;
    g_input.mouse_dy += dy;
}

void input_set_mouse_pos(int x, int y) {
    g_input.mouse_x = x;
    g_input.mouse_y = y;
}

void input_set_mouse_button(int button, bool pressed) {
    if (button == MOUSE_BUTTON_LEFT) g_input.mouse_left = pressed;
    if (button == MOUSE_BUTTON_RIGHT) g_input.mouse_right = pressed;
}
