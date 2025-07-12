#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec3 gPosition;
layout (location = 2) out vec3 gNormal;
layout (location = 3) out vec3 gAlbedo;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

uniform vec3 cameraPos;
uniform sampler2D texture_diffuse;

uniform vec3 lightDir;
uniform vec3 lightColor;
uniform float ambientStrength;
uniform float diffuseStrength;
uniform float specularStrength;
uniform float shininess;

void main() {
    vec4 textureColor = texture(texture_diffuse, TexCoord);
    vec3 norm = normalize(Normal);
    vec3 lightDirection = normalize(-lightDir);
    vec3 viewDir = normalize(cameraPos - FragPos);

    // Lighting calculations
    vec3 ambient = ambientStrength * lightColor;
    float diff = max(dot(norm, lightDirection), 0.0);
    vec3 diffuse = diffuseStrength * diff * lightColor;
    
    // Specular reflection
    vec3 halfwayDir = normalize(lightDirection + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), shininess);
    
    // Fresnel term for angle-dependent reflectivity (Schlick’s approximation)
    float fresnel = pow(1.0 - clamp(dot(viewDir, norm), 0.0, 1.0), 5.0);
    float fresnelBoost = mix(0.1, 1.0, fresnel); // Controls the reflection intensity at different angles
    
    vec3 specular = specularStrength * spec * lightColor * fresnelBoost;

    // Adjust transparency based on viewing angle
    float finalAlpha = mix(0.2, 0.85, fresnel); 

    // Adjust color appearance at grazing angles
    vec3 finalColor = (ambient + diffuse + specular) * textureColor.rgb;
    finalColor = mix(finalColor, finalColor * 0.7, fresnel); // Reduce brightness at sharp angles

    FragColor = vec4(finalColor, finalAlpha);
}
