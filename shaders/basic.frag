#version 330 core
out vec4 FragColor;

in vec3 ourColor;
in vec3 Normal;
in float AO;
in vec2 TexCoord;

uniform sampler2D uTexture;

void main() {
    // Simple directional lighting
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(Normal, lightDir), 0.0);
    
    float ambient = 0.3;
    float light = min(ambient + diff, 1.0);
    
    // Sample texture and combine with lighting and AO
    vec4 texColor = texture(uTexture, TexCoord);
    
    // Apply AO and lighting to the texture
    vec3 result = texColor.rgb * light * AO;
    
    FragColor = vec4(result, 1.0);
}
