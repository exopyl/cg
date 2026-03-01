#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;

// Output color
layout(location = 0) out vec4 outColor;

// Specialization constant: flat shading toggle
layout(constant_id = 0) const bool useFlat = false;

// Simple directional light for basic shading (Story 2-4)
// Light direction: normalized (1,1,1) = diagonal from top-right-front
// Ambient strength: 0.2 = 20% base illumination to prevent completely dark areas
// These values will be configurable via uniforms in future stories
const vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
const float ambientStrength = 0.2;

void main() {
    // Choose normal based on shading mode
    vec3 normal = useFlat
        ? normalize(cross(dFdx(fragWorldPos), dFdy(fragWorldPos)))
        : normalize(fragNormal);
    // For flat shading, dFdx/dFdy cross product orientation depends on triangle
    // winding in screen space, so the normal may point away from the light.
    // Use abs() to ensure consistent lighting on all faces.
    float diff = useFlat
        ? abs(dot(normal, lightDir))
        : max(dot(normal, lightDir), 0.0);

    // Combine ambient and diffuse
    float lighting = ambientStrength + diff * (1.0 - ambientStrength);
    vec3 result = fragColor * lighting;

    outColor = vec4(result, 1.0);
}
