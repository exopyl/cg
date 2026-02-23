#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;

// Output color
layout(location = 0) out vec4 outColor;

// Simple directional light for basic shading (Story 2-4)
// Light direction: normalized (1,1,1) = diagonal from top-right-front
// Ambient strength: 0.2 = 20% base illumination to prevent completely dark areas
// These values will be configurable via uniforms in future stories
const vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
const float ambientStrength = 0.2;

void main() {
    // Basic diffuse lighting
    vec3 normal = normalize(fragNormal);
    float diff = max(dot(normal, lightDir), 0.0);

    // Combine ambient and diffuse
    float lighting = ambientStrength + diff * (1.0 - ambientStrength);
    vec3 result = fragColor * lighting;

    outColor = vec4(result, 1.0);
}
