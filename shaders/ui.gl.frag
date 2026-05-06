#version 330 core
in vec2 FragUV;
in vec4 FragColor;
out vec4 OutColor;

uniform sampler2D font_tex;

void main() {
    vec4 texColor = texture(font_tex, FragUV);
    OutColor = FragColor * texColor;
}
