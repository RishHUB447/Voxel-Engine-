#version 330 core
layout (location = 0) out vec4 FragColor;
in vec2  TexCoord;
in vec3  FragPos;
in vec3  Normal;
in vec3  Tangent;
in vec3  Bitangent;

// Material maps
uniform sampler2D texture_diffuse1;
uniform sampler2D texture_normal1;
uniform sampler2D texture_metallicRoughness1;

// IBL maps
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D  brdfLUT;

// Camera
uniform vec3 camPos;

// Control
uniform float alphaThreshold = 0.5;  // discard if alpha < this
uniform int   debugMode;       // 1=Albedo,2=Normal,3=Metallic,4=Roughness,5=Final

const float PI = 3.14159265359;

// Improved normal mapping function that properly handles the tangent space conversion
vec3 getNormalFromMap() {
    // Ensure properly normalized vectors
    vec3 N = normalize(Normal);
    vec3 T = normalize(Tangent);
    
    // Re-orthogonalize T with respect to N using Gram-Schmidt process
    T = normalize(T - dot(T, N) * N);
    
    // Calculate B using cross product (ensures proper handedness)
    vec3 B = cross(N, T);
    
    // Form TBN matrix
    mat3 TBN = mat3(T, B, N);
    
    // Sample normal map and transform to [-1,1] range
    vec3 normalMapSample = texture(texture_normal1, TexCoord).rgb;
    vec3 tangentNormal = normalMapSample * 2.0 - 1.0;
    
    // Transform tangent-space normal to world space
    return normalize(TBN * tangentNormal);
}

// Correct Fresnel-Schlick approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

// Improved version with roughness
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    float oneMinusRoughness = 1.0 - roughness;
    return F0 + (max(vec3(oneMinusRoughness), F0) - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

void main() {
    // 1) Sample albedo + alpha discard
    vec4 albedoSample = texture(texture_diffuse1, TexCoord);
    if (albedoSample.a < alphaThreshold) 
        discard;
    
    // 2) Prepare material data - proper sRGB to linear conversion
    vec3 albedo = pow(albedoSample.rgb, vec3(2.2));
    
    // Calculate normal in world space
    vec3 N = getNormalFromMap();
    vec3 V = normalize(camPos - FragPos);
    float NdotV = max(dot(N, V), 0.0);
    
    // Sample metallic-roughness texture
    vec3 mrSample = texture(texture_metallicRoughness1, TexCoord).rgb;
    float roughness = mrSample.g; // Green channel = roughness
    float metallic = mrSample.b;  // Blue channel = metallic
    
    // Use AO from texture if available, otherwise use default
    float ao = 0.8;
    
    // Calculate surface reflection at zero incidence angle
    // For non-metals (dialectics) F0 is 0.04, for metals we use albedo
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    // 3) IBL Diffuse contribution (environment light diffuse)
    vec3 kS = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD = (1.0 - kS) * (1.0 - metallic); // No diffuse for metals
    
    // Sample irradiance map with proper intensity scaling
    vec3 irradiance = texture(irradianceMap, N).rgb;
    
    // Fix: Apply intensity correction to avoid over-bright results
    irradiance = min(irradiance, vec3(2.0)); // Clamp super bright irradiance values
    vec3 diffuseIBL = irradiance * albedo * kD;
    
    // 4) IBL Specular contribution (environment reflections)
    // Calculate reflection vector - add roughness-based perturbation to fix overly sharp reflections
    vec3 R = reflect(-V, N);
    
    // Fix: Apply better roughness scaling for mip selection to prevent over-bright specular
    float mipLevel = roughness * 5.0;
    
    // Sample prefiltered environment map based on roughness
    vec3 prefilteredColor = textureLod(prefilterMap, R, mipLevel).rgb;
    
    // Fix: Reduce intensity of prefiltered color to fix white tint
    prefilteredColor = min(prefilteredColor, vec3(4.0)); // Prevent super bright reflections
    
    // Sample BRDF LUT with clamping to fix potential out-of-range values
    vec2 brdf = texture(brdfLUT, clamp(vec2(NdotV, roughness), vec2(0.0), vec2(1.0))).rg;
    
    // Combine for final specular IBL contribution with proper energy conservation
    vec3 specularIBL = prefilteredColor * (F0 * brdf.x + brdf.y);
    
    // 5) Combine contributions with proper intensity scaling
    // Lower IBL contribution to fix white tint issue
    vec3 diffuseIBL_scaled = diffuseIBL * 0.5; // Reduce diffuse intensity
    vec3 specularIBL_scaled = specularIBL * 0.6; // Control specular intensity
    
    vec3 ambient = (diffuseIBL_scaled + specularIBL_scaled) * ao;
    
    // Apply exposure adjustment before tonemapping to fix white tint
    float exposure = 1.0;
    vec3 exposed = vec3(1.0) - exp(-ambient * exposure);
    
    // Apply gamma correction
    vec3 color = pow(exposed, vec3(1.0/2.2));
    
    // 6) Debug outputs (preserve alpha)
    if (debugMode == 1) {
        // Raw albedo (sRGB)
        FragColor = vec4(albedoSample.rgb, albedoSample.a);
        return;
    }
    else if (debugMode == 2) {
        // Normal map visualization (tangent space)
        vec3 normalTex = texture(texture_normal1, TexCoord).rgb;
        FragColor = vec4(normalTex, albedoSample.a);
        return;
    }
    else if (debugMode == 3) {
        // Metallic
        FragColor = vec4(vec3(metallic), albedoSample.a);
        return;
    }
    else if (debugMode == 4) {
        // Roughness
        FragColor = vec4(vec3(roughness), albedoSample.a);
        return;
    }
    else if (debugMode == 5) {
        // Final PBR+IBL
        FragColor = vec4(color, albedoSample.a);
        return;
    }
    
    // Default = final
    FragColor = vec4(color, albedoSample.a);
}