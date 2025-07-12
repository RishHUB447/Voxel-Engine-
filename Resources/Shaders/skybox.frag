#version 330 core
out vec4 FragColor;

in vec3 texCoords;  // Make sure texCoords is correctly passed from the vertex shader

uniform samplerCube skybox;

void main() {
    FragColor = texture(skybox, texCoords);
}
