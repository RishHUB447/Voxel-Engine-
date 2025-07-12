#version 330 core
out vec4 FragColor;
in vec3 localPos;

uniform samplerCube environmentMap;
const float PI = 3.14159265359;

void main()
{		
   vec3 N = normalize(localPos);
   vec3 irradiance = vec3(0.0);
   vec3 right = cross(vec3(0.0, 1.0, 0.0), N);
   vec3 up = cross(N, right);

   float sampleDelta = 0.025;
   float nrSamples = 0.0;

   for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));

            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
            sampleVec = normalize(sampleVec);

            // The critical fix: clamp extremely high values before accumulation
            vec3 color = texture(environmentMap, sampleVec).rgb;
            // Using 100.0 as an arbitrary high value cap - you may need to adjust this
            color = min(color, vec3(100.0));
            
            irradiance += color * cos(theta) * sin(theta);
            nrSamples++;
        }
   }
   
   // Simple and direct approach to avoid division by zero
   if (nrSamples > 0.0)
       irradiance = PI * irradiance / float(nrSamples);
   
   FragColor = vec4(irradiance, 1.0);
}