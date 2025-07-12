#version 330 core

// Output color
out vec4 FragColor;

// Texture sampler uniform
in vec2 TexCoords;
uniform sampler2D texture_diffuse;

void main()
{	
    vec4 texcol = texture(texture_diffuse, TexCoords);
	if (texcol.a < 1) discard;
    FragColor = texcol;
}
