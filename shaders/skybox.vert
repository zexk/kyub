#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;

out vec3 pos;

void main() {
    pos = aPos;
    mat4 view_rotation = mat4(mat3(view));
    vec4 clip_pos = projection * view_rotation * vec4(aPos, 1.0);
    gl_Position = clip_pos.xyww;
}