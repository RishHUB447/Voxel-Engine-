#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D screenTexture;

void main()
{
    vec4 color = texture(screenTexture, TexCoords);
    vec2 position = (TexCoords - 0.5) * 1.067;
    float len = length(position);
    float vignette = smoothstep(1.0, 0.5, len);
    FragColor = color * vignette;
}