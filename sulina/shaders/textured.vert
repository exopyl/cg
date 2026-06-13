#version 450

// VertexPBR layout (cgre2::VertexPBR). Step 1 of material/texture support
// only consumes locations 0-3; tangent (loc 4) and uv1 (loc 5) are described
// by the pipeline's vertex layout but left unread until normal mapping.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
} pc;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec2 fragUV;

void main() {
    gl_Position  = pc.mvp * vec4(inPosition, 1.0);
    fragColor    = inColor;
    fragNormal   = mat3(pc.model) * inNormal;
    fragWorldPos = vec3(pc.model * vec4(inPosition, 1.0));
    fragUV       = inUV;
}
