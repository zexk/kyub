#include "platform/game_input.h"
#include "platform/platform.h"

void game_input_handle_event(GameInput *gi, const Event *event) {
    switch (event->type) {
        case EVENT_KEY_DOWN:
            if (event->key.key >= 0 && event->key.key < 256)
                gi->keys[event->key.key] = true;
            if (event->key.key == 0x10) gi->shift = true;
            break;
        case EVENT_KEY_UP:
            if (event->key.key >= 0 && event->key.key < 256)
                gi->keys[event->key.key] = false;
            if (event->key.key == 0x10) gi->shift = false;
            break;
        case EVENT_MOUSE_BUTTON:
            if (event->mouse_button.button == MOUSE_BUTTON_LEFT)
                gi->mouse_left = event->mouse_button.down;
            if (event->mouse_button.button == MOUSE_BUTTON_RIGHT)
                gi->mouse_right = event->mouse_button.down;
            if (event->mouse_button.button == MOUSE_BUTTON_MIDDLE)
                gi->mouse_middle = event->mouse_button.down;
            break;
        case EVENT_MOUSE_MOTION:
            gi->mouse_x = event->mouse_motion.x;
            gi->mouse_y = event->mouse_motion.y;
            break;
        default:
            break;
    }
}
