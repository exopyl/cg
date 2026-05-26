#pragma once

#include <cstdint>

// C++ mirrors of the GLSL std140 uniform blocks used by the PBR shaders.
// Every member offset matches what the shader expects; tweaking either
// side requires updating the other. `alignas(16)` on the outermost
// struct + padding fields keep the layout right under std140 rules:
//   - mat4 / vec4 → 16-byte aligned
//   - vec3 → padded to 16 bytes (same as vec4)
//   - vec2 / float / int → 4-byte (unless in a struct that itself is
//     aligned to 16)
//
// Static asserts pin sizes so a careless edit triggers a compile error
// before producing a silent runtime mismatch.

namespace cgre2 {

struct alignas(16) CameraUBO {
    float view[16];        //  64
    float proj[16];        //  64
    float viewProj[16];    //  64
    float cameraPos[3];    //  12  — .xyz = world-space camera position
    float time;            //   4  — seconds since start, free for shaders
};
static_assert(sizeof(CameraUBO) == 208, "CameraUBO must be 208 bytes (std140)");

struct alignas(16) DirectionalLightStd140 {
    float direction[3];        //  12  — .xyz = vector FROM surface TO light
    float intensity;           //   4
    float color[3];            //  12
    float _padColor;           //   4  — pad to next vec4 boundary
};
static_assert(sizeof(DirectionalLightStd140) == 32, "DirectionalLight must be 32 bytes (std140)");

struct alignas(16) LightsUBO {
    DirectionalLightStd140 sun;    //  32
    float                  ambientColor[3]; // 12  — used as flat-ambient fallback until IBL lands
    float                  _padAmbient;     //  4
};
static_assert(sizeof(LightsUBO) == 48, "LightsUBO must be 48 bytes (std140)");

// Material flags packed in MaterialUBO::flags (a uint). One bit per
// optional texture map — when a bit is OFF, the shader uses the
// corresponding factor unmodulated (and the bound sampler should be
// the TextureManager's fallback 1×1 pixel).
constexpr uint32_t MAT_HAS_ALBEDO_MAP    = 1u << 0;
constexpr uint32_t MAT_HAS_NORMAL_MAP    = 1u << 1;
constexpr uint32_t MAT_HAS_MR_MAP        = 1u << 2;  // metallic-roughness (B=metallic, G=roughness)
constexpr uint32_t MAT_HAS_AO_MAP        = 1u << 3;
constexpr uint32_t MAT_HAS_EMISSIVE_MAP  = 1u << 4;
constexpr uint32_t MAT_ALPHA_MASK        = 1u << 5;  // glTF alphaMode=MASK (discard at alphaCutoff)
// Per-texture UV set selector — when bit is set, the texture samples
// TEXCOORD_1 instead of TEXCOORD_0. glTF allows each map to nominate
// its own UV set; the fallback is set 0.
constexpr uint32_t MAT_UV1_ALBEDO        = 1u << 6;
constexpr uint32_t MAT_UV1_NORMAL        = 1u << 7;
constexpr uint32_t MAT_UV1_MR            = 1u << 8;
constexpr uint32_t MAT_UV1_AO            = 1u << 9;
constexpr uint32_t MAT_UV1_EMISSIVE      = 1u << 10;

struct alignas(16) MaterialUBO {
    float    baseColorFactor[4];   // 16  — .rgba; .a doubles as opacity/alphaCutoff input
    float    emissiveFactor[3];    // 12
    float    metallicFactor;       //  4
    float    roughnessFactor;      //  4
    float    normalScale;          //  4
    float    occlusionStrength;    //  4
    float    alphaCutoff;          //  4
    uint32_t flags;                //  4  — bitwise OR of MAT_HAS_*
    float    _pad[3];              // 12  — pad to next vec4 boundary
};
static_assert(sizeof(MaterialUBO) == 64, "MaterialUBO must be 64 bytes (std140)");

// Helper to fill a default material when no GLB material is available
// (OBJ / STL / failed material extraction).
inline MaterialUBO makeDefaultMaterial()
{
    MaterialUBO m{};
    m.baseColorFactor[0]  = 0.7f;
    m.baseColorFactor[1]  = 0.7f;
    m.baseColorFactor[2]  = 0.7f;
    m.baseColorFactor[3]  = 1.0f;
    m.metallicFactor      = 0.0f;
    m.roughnessFactor     = 0.5f;
    m.normalScale         = 1.0f;
    m.occlusionStrength   = 1.0f;
    m.alphaCutoff         = 0.5f;
    m.flags               = 0u;
    return m;
}

} // namespace cgre2
