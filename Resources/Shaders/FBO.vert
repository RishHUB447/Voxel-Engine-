#version 330 core
layout (location = 0) in vec2 aPos;      // Changed to vec2 since we only need x,y
layout (location = 1) in vec2 aTexCoords;
out vec2 TexCoords;

void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);  // Simply pass through the position
    TexCoords = aTexCoords;
}