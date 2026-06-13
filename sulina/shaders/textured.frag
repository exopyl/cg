#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec2 fragUV;

// Albedo map. In the minimal scope every sub-mesh binds a 1x1 white
// fallback, so this multiply is a no-op until real material textures are
// uploaded; the visible color then comes entirely from the vertex color.
layout(set = 0, binding = 0) uniform sampler2D albedo;

layout(location = 0) out vec4 outColor;

// Flat-shading toggle (specialization constant), kept from basic.frag.
layout(constant_id = 0) const bool useFlat = false;

const vec3  lightDir        = normalize(vec3(1.0, 1.0, 1.0));
const float ambientStrength = 0.2;

void main() {
    vec3 normal = useFlat
        ? normalize(cross(dFdx(fragWorldPos), dFdy(fragWorldPos)))
        : normalize(fragNormal);
    float diff = useFlat
        ? abs(dot(normal, lightDir))
        : max(dot(normal, lightDir), 0.0);

    float lighting = ambientStrength + diff * (1.0 - ambientStrength);

    vec3 base   = texture(albedo, fragUV).rgb * fragColor;
    vec3 result = base * lighting;

    outColor = vec4(result, 1.0);
}
