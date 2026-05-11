#version 450 core

layout(location = 0) in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

layout(location = 0) out vec3 pos;

void main() {
    pos = aPos;
    mat4 view_rotation = mat4(mat3(uView));
    vec4 clip_pos = uProjection * view_rotation * vec4(aPos, 1.0);
    gl_Position = clip_pos.xyww;
}
