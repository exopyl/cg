#version 450

// Unlit pass for OBJ line ('l') and point ('p') primitives. Consumes the same
// VertexPBR buffer layout as the surface pipeline but only reads position and
// color (normal/uv/tangent attributes are described by the pipeline layout and
// simply ignored here). No lighting: segments/points show their flat color.
layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec3 inColor;

// Only the MVP is needed (no lighting → no model matrix). This is the first
// 64 bytes of the host-side PushConstants struct, so the draw pushes offset 0,
// size 64. Keeping the block minimal makes the reflected push-constant range
// unambiguous (exactly one mat4).
layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position  = pc.mvp * vec4(inPosition, 1.0);
    // Point size for POINT_LIST topology. Clamped to 1 unless the device has
    // the largePoints feature, which is harmless (defined size either way).
    gl_PointSize = 4.0;
    fragColor    = inColor;
}
