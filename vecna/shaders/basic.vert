#version 450

// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

// Push constants for MVP and model matrices
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
} pc;

// Outputs to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragNormal = mat3(pc.model) * inNormal;
    fragWorldPos = vec3(pc.model * vec4(inPosition, 1.0));
}
