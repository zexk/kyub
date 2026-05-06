#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in float aAO;
layout (location = 4) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 ourColor;
out vec3 Normal;
out float AO;
out vec2 TexCoord;
out vec3 view_pos;

void main() {
    vec4 world_pos = model * vec4(aPos, 1.0);
    gl_Position = projection * view * world_pos;
    ourColor = aColor;
    Normal = aNormal;
    AO = aAO;
    TexCoord = aTexCoord;
    view_pos = (view * world_pos).xyz;
}
