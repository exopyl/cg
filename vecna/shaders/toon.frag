#version 450

// Same per-vertex inputs as basic.vert emits. We only use color + normal.
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;  // unused

layout(location = 0) out vec4 outColor;

// Light direction shared with basic.frag so the result is visually
// comparable.
const vec3  lightDir       = normalize(vec3(1.0, 1.0, 1.0));
const float ambientStrength = 0.2;

// Number of discrete diffuse bands. Could be promoted to a
// specialization constant once we want to expose it from the UI.
const float kBands = 4.0;

void main() {
    vec3  n    = normalize(fragNormal);
    float diff = max(dot(n, lightDir), 0.0);

    // Posterize: snap `diff` to one of kBands evenly-spaced values in
    // [0, 1]. `ceil(diff * kBands) / kBands` produces 1/N, 2/N, … N/N
    // and never 0 unless `diff` is exactly 0 (handled below).
    float banded = (diff <= 0.0)
                       ? 0.0
                       : ceil(diff * kBands) / kBands;

    float lighting = ambientStrength + banded * (1.0 - ambientStrength);
    outColor = vec4(fragColor * lighting, 1.0);
}
