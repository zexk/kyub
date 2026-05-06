#ifndef RENDERER_GL_H
#define RENDERER_GL_H

#include "renderer.h"

/* Backend-specific initialization */
void renderer_init(int width, int height);
void renderer_shutdown(void);

/* Platform display/window access for init */
void* renderer_gl_get_display(void);
void* renderer_gl_get_window(void);

#endif /* RENDERER_GL_H */
