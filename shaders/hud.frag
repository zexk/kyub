#version 450 core

layout(location = 0) out vec4 FragColor;

layout(push_constant) uniform PushConstants {
    vec3 uColor;
    float uAlpha;
} pc;

void main() {
    FragColor = vec4(pc.uColor, pc.uAlpha);
}
