#version 450 core

layout(location = 0) in vec2 aPos;

void main() {
    /* Vulkan native: NDC Y is down, flip input Y which is in OpenGL-style coords */
    gl_Position = vec4(aPos.x, -aPos.y, 0.0, 1.0);
}
