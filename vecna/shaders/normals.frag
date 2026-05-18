#version 450

// Inputs from basic.vert (same vertex shader is reused — the cgre2
// ShaderManager caches it once and the second Pipeline ctor pulls the
// same VkShaderModule out of the cache).
layout(location = 0) in vec3 fragColor;     // unused here, but the vertex stage emits it
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;  // unused here

layout(location = 0) out vec4 outColor;

void main() {
    // Map the world-space normal from [-1, 1] to [0, 1] so it becomes a
    // visible RGB color. Smooth meshes produce a smooth rainbow; flat
    // surfaces produce a single color. Easy visual check that:
    //   - the vertex shader emits normals,
    //   - reflection picks them up (location 1 is wired correctly),
    //   - per-vertex normals are non-zero (they would be if the early
    //     `m_pVertexNormals.empty()` heuristic still skipped
    //     ComputeNormals for OBJ files).
    vec3 n = normalize(fragNormal);
    outColor = vec4(n * 0.5 + 0.5, 1.0);
}
