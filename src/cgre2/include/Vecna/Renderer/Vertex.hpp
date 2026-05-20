#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <vector>

namespace cgre2 {

/// Bundle of vertex input descriptors used to wire a VkPipeline's vertex
/// input state. A pipeline accepts one VertexLayout (single binding); for
/// pipelines that need multiple bindings (e.g. instanced data), callers
/// can construct a richer struct themselves and pass through Pipeline's
/// raw-Vulkan ctor overload.
struct VertexLayout {
    VkVertexInputBindingDescription                binding;
    std::vector<VkVertexInputAttributeDescription> attributes;
};

/// Compact vertex used for the default 3D rendering path: position,
/// normal, vertex color. Matches `basic.vert`'s layout (locations 0/1/2).
struct Vertex {
    float position[3];
    float normal[3];
    float color[3];

    /// Combined binding + attributes for a single-binding pipeline.
    [[nodiscard]] static VertexLayout getLayout();

    // Backwards-compatible accessors kept so callers that still talk
    // straight Vulkan don't have to switch to VertexLayout right away.
    [[nodiscard]] static VkVertexInputBindingDescription getBindingDescription();
    [[nodiscard]] static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};

/// PBR-ready vertex: adds two UV sets (texture sampling), a tangent (with
/// w storing handedness in the standard ±1 convention) so the fragment
/// shader can reconstruct the bitangent and sample normal maps, and a
/// second UV set (glTF TEXCOORD_1, often used for AO / lightmap on a
/// separate parameterization).
struct VertexPBR {
    float position[3];
    float normal[3];
    float color[3];
    float uv[2];        // TEXCOORD_0
    float tangent[4];   // .xyz tangent, .w handedness sign
    float uv1[2];       // TEXCOORD_1 (zero when source has only one set)

    [[nodiscard]] static VertexLayout getLayout();

    /// Promote a basic Vertex (position+normal+color only) to a PBR vertex
    /// by injecting harmless defaults for UV and tangent. Used when feeding
    /// OBJ/STL meshes through the PBR pipeline: those formats don't carry
    /// UVs or tangents, and the default material has no maps anyway, so
    /// the placeholders never affect shading.
    static VertexPBR fromBasic(const Vertex& v) noexcept {
        VertexPBR p{};
        p.position[0] = v.position[0]; p.position[1] = v.position[1]; p.position[2] = v.position[2];
        p.normal[0]   = v.normal[0];   p.normal[1]   = v.normal[1];   p.normal[2]   = v.normal[2];
        p.color[0]    = v.color[0];    p.color[1]    = v.color[1];    p.color[2]    = v.color[2];
        p.uv[0]       = 0.0f; p.uv[1]  = 0.0f;
        p.tangent[0]  = 1.0f; p.tangent[1] = 0.0f; p.tangent[2] = 0.0f; p.tangent[3] = 1.0f;
        p.uv1[0]      = 0.0f; p.uv1[1] = 0.0f;
        return p;
    }
};

} // namespace cgre2
