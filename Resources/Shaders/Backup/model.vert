#version 430 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;
layout(location = 2) in vec2 tex;
layout(location = 5) in ivec4 boneIds; 
layout(location = 6) in vec4 weights;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];

out vec2 TexCoords;
out vec3 FragNormal;

void main()
{
    vec4 totalPosition = vec4(0.0f);
    vec3 totalNormal = vec3(0.0f);
    float totalWeight = 0.0f;

    for (int i = 0; i < MAX_BONE_INFLUENCE; i++)
    {
        if (boneIds[i] < 0)  // -1 means no bone assigned
            continue;
        
        if (boneIds[i] >= MAX_BONES) 
            continue;

        vec4 localPosition = finalBonesMatrices[boneIds[i]] * vec4(pos, 1.0f);
        totalPosition += localPosition * weights[i];

        mat3 normalMatrix = mat3(finalBonesMatrices[boneIds[i]]);
        totalNormal += normalMatrix * norm * weights[i];

        totalWeight += weights[i];
    }

    // If no valid bone weights were applied, keep in bind pose
    if (totalWeight == 0.0)
    {
        totalPosition = vec4(pos, 1.0f);
        totalNormal = norm;
    }

    mat4 viewModel = view * model;
    gl_Position = projection * viewModel * totalPosition;
    
    TexCoords = tex;
    FragNormal = normalize(totalNormal);
}
