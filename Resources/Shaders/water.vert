#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;
layout (location = 2) in vec3 aNormal;
out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;   // World-space position
out vec3 ViewPos;   // View-space position
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time; // Add this uniform for animation

void main() {
    // Create a modified position with wave effect
    vec3 waterPos = aPos;
    
    // Base lowering of water level
    waterPos.y -= 0.07;
    
    // Get world position for consistent waves across chunks
    vec3 worldPos = vec3(model * vec4(aPos, 1.0));
    
    // Apply sine wave based on world position and time
    float waveHeight = 0.02; // Reduced amplitude for subtler effect
    float waveFreq = 0.4; // Lower frequency for more gradual waves
    float waveSpeed = 2.5; // Speed of the wave animation
    
    // Create waves using PI to make the pattern more continuous
    waterPos.y += waveHeight * sin(waveFreq * worldPos.x * 3.14159 + time * waveSpeed);
    waterPos.y += waveHeight * sin(waveFreq * worldPos.z * 3.14159 + time * waveSpeed * 0.7);
    
    // Add a third wave at a different angle for more variety
    waterPos.y += waveHeight * 0.5 * sin(waveFreq * (worldPos.x + worldPos.z) * 3.14159 * 0.8 + time * waveSpeed * 1.3);
    
    // World-space position (for lighting)
    FragPos = vec3(model * vec4(waterPos, 1.0));
    
    // View-space position (for specular)
    ViewPos = vec3(view * model * vec4(waterPos, 1.0));
    
    // Transform normal to world space
    Normal = mat3(transpose(inverse(model))) * aNormal;
    
    TexCoord = aTexCoords;
    gl_Position = projection * view * model * vec4(waterPos, 1.0);
}