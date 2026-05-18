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

/// PBR-ready vertex: adds UV (texture sampling) and a tangent (with w
/// storing handedness in the standard ±1 convention) so the fragment
/// shader can reconstruct the bitangent and sample normal maps.
struct VertexPBR {
    float position[3];
    float normal[3];
    float color[3];
    float uv[2];
    float tangent[4];   // .xyz tangent, .w handedness sign

    [[nodiscard]] static VertexLayout getLayout();
};

} // namespace cgre2
