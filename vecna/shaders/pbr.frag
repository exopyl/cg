#version 450

// PBR fragment shader — Cook-Torrance metallic-roughness BRDF per the
// glTF 2.0 reference implementation. No IBL yet (tier 2): the ambient
// term is a flat constant in LightsUBO. One directional light.
//
// Bindings (must match Application's descriptor set updates):
//   set 0: 0 = CameraUBO, 1 = LightsUBO  (scene-wide, bound once per frame)
//   set 1: 0 = MaterialUBO, 1..5 = samplers (albedo, normal, MR, AO, emissive)

layout(location = 0) in vec3 v_worldPos;
layout(location = 1) in vec3 v_worldNormal;
layout(location = 2) in vec3 v_worldTangent;
layout(location = 3) in vec3 v_worldBitangent;
layout(location = 4) in vec2 v_uv;
layout(location = 5) in vec3 v_color;
layout(location = 6) in vec2 v_uv1;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPosTime;
} u_camera;

struct DirLight {
    vec3  direction;     // surface -> light, normalized
    float intensity;
    vec3  color;
    float _padColor;
};

layout(set = 0, binding = 1) uniform LightsUBO {
    DirLight sun;
    vec3     ambientColor;
    float    _padAmbient;
} u_lights;

const uint MAT_HAS_ALBEDO_MAP   = 1u << 0;
const uint MAT_HAS_NORMAL_MAP   = 1u << 1;
const uint MAT_HAS_MR_MAP       = 1u << 2;
const uint MAT_HAS_AO_MAP       = 1u << 3;
const uint MAT_HAS_EMISSIVE_MAP = 1u << 4;
const uint MAT_ALPHA_MASK       = 1u << 5;
const uint MAT_UV1_ALBEDO       = 1u << 6;
const uint MAT_UV1_NORMAL       = 1u << 7;
const uint MAT_UV1_MR           = 1u << 8;
const uint MAT_UV1_AO           = 1u << 9;
const uint MAT_UV1_EMISSIVE     = 1u << 10;

layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4  baseColorFactor;
    vec3  emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    float alphaCutoff;
    uint  flags;
} u_material;

layout(set = 1, binding = 1) uniform sampler2D u_albedoMap;
layout(set = 1, binding = 2) uniform sampler2D u_normalMap;
layout(set = 1, binding = 3) uniform sampler2D u_mrMap;
layout(set = 1, binding = 4) uniform sampler2D u_aoMap;
layout(set = 1, binding = 5) uniform sampler2D u_emissiveMap;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// Cook-Torrance / glTF reference helpers.

float D_GGX(float NoH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float NoH2 = NoH * NoH;
    float denom = (NoH2 * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denom * denom, 1e-6);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float lambdaV = NoL * sqrt((NoV - NoV * a2) * NoV + a2);
    float lambdaL = NoV * sqrt((NoL - NoL * a2) * NoL + a2);
    return 0.5 / max(lambdaV + lambdaL, 1e-6);
}

vec3 F_Schlick(vec3 F0, float VoH)
{
    float f = pow(1.0 - VoH, 5.0);
    return F0 + (vec3(1.0) - F0) * f;
}

// Pick the UV set used by a given texture map. glTF allows each map to
// nominate either TEXCOORD_0 or TEXCOORD_1; the loader packs the choice
// into MAT_UV1_* flags. Fallback: TEXCOORD_0.
vec2 uvFor(uint uv1Flag) {
    return ((u_material.flags & uv1Flag) != 0u) ? v_uv1 : v_uv;
}

void main()
{
    // ---- Material lookups -----------------------------------------------
    vec4 albedoSample = ((u_material.flags & MAT_HAS_ALBEDO_MAP) != 0u)
                          ? texture(u_albedoMap, uvFor(MAT_UV1_ALBEDO))
                          : vec4(1.0);
    vec4 baseColor    = albedoSample * u_material.baseColorFactor * vec4(v_color, 1.0);

    // glTF alphaMode = MASK: any pixel below alphaCutoff is fully
    // discarded. BLEND is currently not supported by the pipeline state
    // (no alpha blending enabled) — the loader logs a warning when a
    // BLEND material is encountered, and we still draw it as opaque.
    if ((u_material.flags & MAT_ALPHA_MASK) != 0u && baseColor.a < u_material.alphaCutoff) {
        discard;
    }

    float metallic  = u_material.metallicFactor;
    float roughness = u_material.roughnessFactor;
    if ((u_material.flags & MAT_HAS_MR_MAP) != 0u) {
        // glTF convention: B channel = metallic, G channel = roughness.
        vec4 mr = texture(u_mrMap, uvFor(MAT_UV1_MR));
        metallic  *= mr.b;
        roughness *= mr.g;
    }
    roughness = clamp(roughness, 0.045, 1.0);  // avoid singular NDF at r=0

    float ao = 1.0;
    if ((u_material.flags & MAT_HAS_AO_MAP) != 0u) {
        ao = mix(1.0, texture(u_aoMap, uvFor(MAT_UV1_AO)).r, u_material.occlusionStrength);
    }

    vec3 emissive = u_material.emissiveFactor;
    if ((u_material.flags & MAT_HAS_EMISSIVE_MAP) != 0u) {
        emissive *= texture(u_emissiveMap, uvFor(MAT_UV1_EMISSIVE)).rgb;
    }

    // ---- Normal --------------------------------------------------------
    vec3 N = normalize(v_worldNormal);
    if ((u_material.flags & MAT_HAS_NORMAL_MAP) != 0u) {
        // Tangent-space normal map sampled in [0, 1], remapped to [-1, 1].
        // Then transformed to world via the TBN built in the vertex stage.
        vec3 tangentN = texture(u_normalMap, uvFor(MAT_UV1_NORMAL)).rgb * 2.0 - 1.0;
        tangentN.xy *= u_material.normalScale;
        mat3 TBN = mat3(normalize(v_worldTangent),
                        normalize(v_worldBitangent),
                        N);
        N = normalize(TBN * tangentN);
    }

    // ---- Lighting ------------------------------------------------------
    vec3 V = normalize(u_camera.cameraPosTime.xyz - v_worldPos);
    vec3 L = normalize(u_lights.sun.direction);
    vec3 H = normalize(V + L);

    float NoV = clamp(dot(N, V), 0.0, 1.0);
    float NoL = clamp(dot(N, L), 0.0, 1.0);
    float NoH = clamp(dot(N, H), 0.0, 1.0);
    float VoH = clamp(dot(V, H), 0.0, 1.0);

    vec3 F0       = mix(vec3(0.04), baseColor.rgb, metallic);
    vec3 F        = F_Schlick(F0, VoH);
    float D       = D_GGX(NoH, roughness);
    float Vvis    = V_SmithGGXCorrelated(NoV, NoL, roughness);

    // kS already encoded in F (Schlick); diffuse term is (1-F) * (1-metallic)
    // per the metallic-roughness convention (metals have no Lambertian).
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 diffuse  = kD * baseColor.rgb / PI;
    vec3 specular = F * D * Vvis;

    vec3 radiance = u_lights.sun.color * u_lights.sun.intensity;
    vec3 Lo = (diffuse + specular) * radiance * NoL;

    // Ambient placeholder until IBL replaces it.
    vec3 ambient = u_lights.ambientColor * baseColor.rgb * ao;

    vec3 color = Lo + ambient + emissive;

    outColor = vec4(color, baseColor.a);
}
