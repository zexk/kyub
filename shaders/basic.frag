#version 330 core
out vec4 FragColor;

in vec3 ourColor;
in vec3 Normal;
in float AO;

void main() {
    // Simple directional lighting
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(Normal, lightDir), 0.0);
    
    float ambient = 0.3;
    float light = min(ambient + diff, 1.0);
    
    // Combine lighting and AO
    vec3 result = ourColor * light * AO;
    
    FragColor = vec4(result, 1.0);
}
