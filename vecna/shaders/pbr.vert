#version 450

// PBR vertex shader. Consumes the VertexPBR layout (5 attributes) and
// emits world-space position / normal / tangent / bitangent + uv +
// vertex color. Camera view/proj come from a UBO; the model matrix
// stays as a push constant for cheap per-object updates.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec4 inTangent;   // .xyz = tangent, .w = bitangent sign
layout(location = 5) in vec2 inUV1;       // TEXCOORD_1 (glTF optional second UV set)

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPosTime;   // .xyz = world camera pos, .w = time
} u_camera;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) out vec3 v_worldPos;
layout(location = 1) out vec3 v_worldNormal;
layout(location = 2) out vec3 v_worldTangent;
layout(location = 3) out vec3 v_worldBitangent;
layout(location = 4) out vec2 v_uv;
layout(location = 5) out vec3 v_color;
layout(location = 6) out vec2 v_uv1;

void main() {
    vec4 worldPos4 = pc.model * vec4(inPosition, 1.0);
    v_worldPos = worldPos4.xyz;

    // Normal matrix = transpose(inverse(mat3(model))). For uniform-scale
    // models we could skip the inverse-transpose, but in the general
    // case (non-uniform scale, shear) it's required for correct normals.
    mat3 normalMat = transpose(inverse(mat3(pc.model)));

    v_worldNormal    = normalize(normalMat * inNormal);
    v_worldTangent   = normalize(normalMat * inTangent.xyz);
    v_worldBitangent = cross(v_worldNormal, v_worldTangent) * inTangent.w;

    v_uv    = inUV;
    v_uv1   = inUV1;
    v_color = inColor;

    gl_Position = u_camera.viewProj * worldPos4;
}
