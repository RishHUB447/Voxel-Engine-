#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out vec2    TexCoord;
out vec3    FragPos;    // world-space position
out vec3    Normal;     // world-space normal
out vec3    Tangent;    // world-space tangent
out vec3    Bitangent;  // world-space bitangent

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    // positions
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    gl_Position = projection * view * worldPos;

    // normals & tangent basis in world‐space
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    Normal    = normalize(normalMatrix * aNormal);
    Tangent   = normalize(normalMatrix * aTangent);
    Bitangent = normalize(normalMatrix * aBitangent);

    TexCoord = aTexCoords;
}
