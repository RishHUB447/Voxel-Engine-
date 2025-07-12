#version 330 core
layout(location = 0) in vec2 aPos;       // Vertex position (2D)
layout(location = 1) in vec2 aTexCoord;  // Texture coordinate

out vec2 TexCoord;  // Pass texture coordinate to fragment shader

uniform mat4 projection;  // Projection matrix for screen-space rendering

void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);  // Transform vertex position
    TexCoord = aTexCoord;                             // Pass texture coordinates
}
