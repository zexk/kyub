#version 450 core

layout(location = 0) out vec4 FragColor;
layout(location = 0) in  vec3 vTexCoord;

layout(set = 0, binding = 0) uniform sampler2DArray uTexture;

layout(push_constant) uniform PushConstants {
    layout(offset = 192) vec3  uColor;
    layout(offset = 208) float uAlpha;
} pc;

void main() {
    vec4 tex  = texture(uTexture, vTexCoord);
    FragColor = vec4(tex.rgb * pc.uColor, tex.a * pc.uAlpha);
}
