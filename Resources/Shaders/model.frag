#version 330 core
in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
in vec3 Tangent;    // Coming from the vertex shader
in vec3 Bitangent;  // Coming from the vertex shader
out vec4 FragColor;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_metallicRoughness1;
uniform sampler2D texture_normal1;

// IBL maps
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;

uniform vec3 camPos;
uniform vec3 lightPos;
uniform vec3 lightColor;

// Material properties
uniform float metallic = 0.65;
uniform float roughness = 0.55;
uniform float ao = 0.8;
uniform int debugMode;

const float PI = 3.14159265359;

// --- Core PBR Functions ---

vec3 getNormal() {
    vec3 tangentNormal = texture(texture_normal1, TexCoords).rgb * 2.0 - 1.0;
    
    // Create TBN matrix: use Gram-Schmidt orthogonalization for T
    vec3 N = normalize(Normal);
    vec3 T = normalize(Tangent - dot(Tangent, N) * N);
    vec3 B = normalize(Bitangent);
    mat3 TBN = mat3(T, B, N);
    
    // Transform tangent-space normal to world-space
    return normalize(TBN * tangentNormal);
}

// Disney's GGX/Trowbridge-Reitz microfacet distribution
float DisneyGGX(vec3 N, vec3 H, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float denom = NdotH2 * (alpha2 - 1.0) + 1.0;
    return alpha2 / (PI * denom * denom);
}

// Smith Joint Masking-Shadowing function 
float GeometrySchlickGGX(float NdotV, float roughness) {
    // Disney modification: more accurate remapping
    float alpha = roughness * roughness;
    float k = alpha / 2.0; // Disney's modification to k
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Disney's modified Fresnel-Schlick approximation with improved transitions
vec3 DisneyFresnelSchlick(float cosTheta, vec3 F0, float roughness) {
    // Disney's addition: account for roughness in Fresnel
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// --- Main Shader ---
void main() {
    // Retrieve material textures (gamma-correct the albedo)
    vec3 albedo = pow(texture(texture_diffuse1, TexCoords).rgb, vec3(2.2));
    vec4 mrSample = texture(texture_metallicRoughness1, TexCoords);
    float metallicFinal = mrSample.b;
    float roughnessFinal = mrSample.g;
    
    // Geometry vectors
    vec3 N = getNormal();
    vec3 V = normalize(camPos - FragPos);
    vec3 L = normalize(lightPos - FragPos);
    vec3 H = normalize(V + L);
    
    // Calculate key dot products needed throughout the shader
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float LdotH = max(dot(L, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);
    
    // Direct light radiance
    float distance = length(lightPos - FragPos);
    vec3 radiance = lightColor / (distance * distance);
    
    // BRDF parameters (specular reflectance at normal incidence)
    vec3 F0 = mix(vec3(0.04), albedo, metallicFinal);
    
    // Disney's modified Fresnel calculation
    vec3 F = DisneyFresnelSchlick(VdotH, F0, roughnessFinal);

    // Disney GGX Normal Distribution Function
    float D = DisneyGGX(N, H, roughnessFinal);
    
    // Smith Joint Geometry function
    float G = GeometrySmith(N, V, L, roughnessFinal);
    
    // Cook-Torrance BRDF specular term
    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL;
    vec3 specularDirect = numerator / max(denominator, 0.001);
    
    // Diffuse term - Disney's approach uses energy conservation
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallicFinal);
    
    // Compose direct lighting
    vec3 Lo = (kD * albedo / PI + specularDirect) * radiance * NdotL;
    
    // --- IBL: Ambient Lighting using Image-Based Lighting ---
    // Diffuse irradiance from the environment
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuseIBL = irradiance * albedo;
    
    // Specular IBL: sample from prefiltered map using the reflection vector
    vec3 R = reflect(-V, N);
    vec3 prefilteredColor = textureLod(prefilterMap, R, roughnessFinal * 4.0).rgb;
    
    // BRDF integration lookup
    vec2 brdf = texture(brdfLUT, vec2(NdotV, roughnessFinal)).rg;
    
    // Fresnel for IBL
    vec3 F_IBL = DisneyFresnelSchlick(NdotV, F0, roughnessFinal);
    vec3 specularIBL = prefilteredColor * (F_IBL * brdf.x + brdf.y);
    
    // Combine diffuse and specular IBL and modulate with ambient occlusion
    vec3 kS_IBL = F_IBL;
    vec3 kD_IBL = (vec3(1.0) - kS_IBL) * (1.0 - metallicFinal);
    vec3 ambientIBL = (kD_IBL * diffuseIBL + specularIBL) * ao;
    
    // Final color composition: add direct and ambient contributions
    vec3 color = ambientIBL + Lo;
    
    // HDR tonemapping (Reinhard tone mapping) and gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    
    // Debug views: you can output different intermediate components for debugging
    if(debugMode == 1)
        FragColor = vec4(vec3(metallicFinal), 1.0);
    else if(debugMode == 2)
        FragColor = vec4(vec3(roughnessFinal), 1.0);
    else if(debugMode == 3)
        FragColor = vec4(metallicFinal, roughnessFinal, 0.0, 1.0);
    else if(debugMode == 4)
        FragColor = texture(texture_normal1, TexCoords);
    else if(debugMode == 5)
        FragColor = texture(texture_diffuse1, TexCoords);
    // In your fragment shader
else if (debugMode == 6) {
    vec2 brdfDebug = texture(brdfLUT, vec2(TexCoords)).rg;
    FragColor = vec4(brdfDebug.x, brdfDebug.y, 0.0, 1.0);
    return;
}
    else
        FragColor = vec4(color, 1.0);
}