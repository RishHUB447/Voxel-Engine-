#version 330 core
in vec2 TexCoord;       // Interpolated texture coordinate from vertex shader
out vec4 FragColor;     // Final fragment color

uniform sampler2D texture1;  // 2D texture sampler

void main() {
    FragColor = texture(texture1, TexCoord);  // Sample the texture
}
