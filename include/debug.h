#ifndef DEBUG_H
#define DEBUG_H

#include <stdbool.h>

void debug_init(void);
void debug_toggle(void);
bool debug_is_enabled(void);
void debug_render(unsigned int shader_program, float fps, int chunks, float cam_x, float cam_y, float cam_z, float yaw, float pitch, int look_x, int look_y, int look_z);

#endif // DEBUG_H