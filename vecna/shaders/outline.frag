#version 450

// Solid outline color. Hard-coded black for now; promotable to a
// specialization constant once the UI exposes a color picker.
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(0.0, 0.0, 0.0, 1.0);
}
