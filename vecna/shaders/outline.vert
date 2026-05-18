#version 450

// Inverted-hull outline vertex shader. Inflates each vertex along its
// normal by `inflate` world units, before MVP transform. Used in a
// separate pipeline that culls front faces (so only the inflated back
// faces are drawn) and renders BEFORE the main mesh — depth testing
// then trims everything except the silhouette band.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;   // unused, kept to match the vertex layout

// Same PushConstants block as basic.vert so cgre2 can fuse the ranges
// when a single layout drives both pipelines.
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
} pc;

// Tunable from C++ via cgre2::SpecializationConstants without recompiling.
layout(constant_id = 0) const float inflate = 0.02;

void main() {
    // Inflate along the model-space normal, then apply MVP.
    vec3 inflated = inPosition + normalize(inNormal) * inflate;
    gl_Position = pc.mvp * vec4(inflated, 1.0);
}
