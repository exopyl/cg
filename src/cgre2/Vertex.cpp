#include "Vecna/Renderer/Vertex.hpp"

namespace cgre2 {

namespace {

VkVertexInputBindingDescription makeBinding(uint32_t stride) {
    VkVertexInputBindingDescription b{};
    b.binding   = 0;
    b.stride    = stride;
    b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return b;
}

VkVertexInputAttributeDescription makeAttr(uint32_t location, VkFormat format, uint32_t offset) {
    VkVertexInputAttributeDescription a{};
    a.binding  = 0;
    a.location = location;
    a.format   = format;
    a.offset   = offset;
    return a;
}

} // namespace

// Vertex (basic: pos, normal, color)

VkVertexInputBindingDescription Vertex::getBindingDescription() {
    return makeBinding(sizeof(Vertex));
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::getAttributeDescriptions() {
    return {
        makeAttr(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)),
        makeAttr(1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)),
        makeAttr(2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)),
    };
}

VertexLayout Vertex::getLayout() {
    VertexLayout layout;
    layout.binding = getBindingDescription();
    auto attrs = getAttributeDescriptions();
    layout.attributes.assign(attrs.begin(), attrs.end());
    return layout;
}

// VertexPBR (pos, normal, color, uv, tangent)

VertexLayout VertexPBR::getLayout() {
    VertexLayout layout;
    layout.binding = makeBinding(sizeof(VertexPBR));
    layout.attributes = {
        makeAttr(0, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(VertexPBR, position)),
        makeAttr(1, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(VertexPBR, normal)),
        makeAttr(2, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(VertexPBR, color)),
        makeAttr(3, VK_FORMAT_R32G32_SFLOAT,       offsetof(VertexPBR, uv)),
        makeAttr(4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexPBR, tangent)),
        makeAttr(5, VK_FORMAT_R32G32_SFLOAT,       offsetof(VertexPBR, uv1)),
    };
    return layout;
}

} // namespace cgre2
