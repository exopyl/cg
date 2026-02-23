#include <gtest/gtest.h>
#include "Vecna/Renderer/Pipeline.hpp"

namespace Vecna::Renderer {

// Test Vertex structure size and layout

TEST(VertexTest, SizeIsCorrect) {
    // Vertex should be 9 floats (3 for position, 3 for normal, 3 for color)
    // 9 * 4 bytes = 36 bytes
    EXPECT_EQ(sizeof(Vertex), 36u);
}

TEST(VertexTest, PositionOffsetIsZero) {
    EXPECT_EQ(offsetof(Vertex, position), 0u);
}

TEST(VertexTest, NormalOffsetIsAfterPosition) {
    // Normal comes after position (3 floats = 12 bytes)
    EXPECT_EQ(offsetof(Vertex, normal), 12u);
}

TEST(VertexTest, ColorOffsetIsAfterNormal) {
    // Color comes after normal (6 floats = 24 bytes)
    EXPECT_EQ(offsetof(Vertex, color), 24u);
}

// Test Vertex binding description

TEST(VertexBindingTest, BindingIsZero) {
    auto binding = Vertex::getBindingDescription();
    EXPECT_EQ(binding.binding, 0u);
}

TEST(VertexBindingTest, StrideMatchesVertexSize) {
    auto binding = Vertex::getBindingDescription();
    EXPECT_EQ(binding.stride, sizeof(Vertex));
}

TEST(VertexBindingTest, InputRateIsPerVertex) {
    auto binding = Vertex::getBindingDescription();
    EXPECT_EQ(binding.inputRate, VK_VERTEX_INPUT_RATE_VERTEX);
}

// Test Vertex attribute descriptions

TEST(VertexAttributeTest, HasThreeAttributes) {
    auto attributes = Vertex::getAttributeDescriptions();
    EXPECT_EQ(attributes.size(), 3u);
}

TEST(VertexAttributeTest, PositionIsLocation0) {
    auto attributes = Vertex::getAttributeDescriptions();
    EXPECT_EQ(attributes[0].location, 0u);
    EXPECT_EQ(attributes[0].binding, 0u);
    EXPECT_EQ(attributes[0].format, VK_FORMAT_R32G32B32_SFLOAT);
    EXPECT_EQ(attributes[0].offset, offsetof(Vertex, position));
}

TEST(VertexAttributeTest, NormalIsLocation1) {
    auto attributes = Vertex::getAttributeDescriptions();
    EXPECT_EQ(attributes[1].location, 1u);
    EXPECT_EQ(attributes[1].binding, 0u);
    EXPECT_EQ(attributes[1].format, VK_FORMAT_R32G32B32_SFLOAT);
    EXPECT_EQ(attributes[1].offset, offsetof(Vertex, normal));
}

TEST(VertexAttributeTest, ColorIsLocation2) {
    auto attributes = Vertex::getAttributeDescriptions();
    EXPECT_EQ(attributes[2].location, 2u);
    EXPECT_EQ(attributes[2].binding, 0u);
    EXPECT_EQ(attributes[2].format, VK_FORMAT_R32G32B32_SFLOAT);
    EXPECT_EQ(attributes[2].offset, offsetof(Vertex, color));
}

// Test PushConstants structure

TEST(PushConstantsTest, SizeIs64Bytes) {
    // MVP matrix is 4x4 floats = 16 * 4 = 64 bytes
    EXPECT_EQ(sizeof(PushConstants), 64u);
}

TEST(PushConstantsTest, SizeMatchesConstant) {
    EXPECT_EQ(PUSH_CONSTANT_SIZE, sizeof(PushConstants));
}

TEST(PushConstantsTest, SizeIsWithinVulkanLimit) {
    // Vulkan guarantees at least 128 bytes for push constants
    constexpr uint32_t VULKAN_MIN_PUSH_CONSTANT_SIZE = 128;
    EXPECT_LE(PUSH_CONSTANT_SIZE, VULKAN_MIN_PUSH_CONSTANT_SIZE);
}

// Test Vertex data initialization

TEST(VertexDataTest, CanCreateVertex) {
    Vertex v{};
    v.position[0] = 1.0f;
    v.position[1] = 2.0f;
    v.position[2] = 3.0f;
    v.normal[0] = 0.0f;
    v.normal[1] = 1.0f;
    v.normal[2] = 0.0f;
    v.color[0] = 1.0f;
    v.color[1] = 0.0f;
    v.color[2] = 0.0f;

    EXPECT_FLOAT_EQ(v.position[0], 1.0f);
    EXPECT_FLOAT_EQ(v.position[1], 2.0f);
    EXPECT_FLOAT_EQ(v.position[2], 3.0f);
    EXPECT_FLOAT_EQ(v.normal[0], 0.0f);
    EXPECT_FLOAT_EQ(v.normal[1], 1.0f);
    EXPECT_FLOAT_EQ(v.normal[2], 0.0f);
    EXPECT_FLOAT_EQ(v.color[0], 1.0f);
    EXPECT_FLOAT_EQ(v.color[1], 0.0f);
    EXPECT_FLOAT_EQ(v.color[2], 0.0f);
}

TEST(VertexDataTest, DefaultInitializationIsZero) {
    Vertex v{};
    for (int i = 0; i < 3; ++i) {
        EXPECT_FLOAT_EQ(v.position[i], 0.0f);
        EXPECT_FLOAT_EQ(v.normal[i], 0.0f);
        EXPECT_FLOAT_EQ(v.color[i], 0.0f);
    }
}

} // namespace Vecna::Renderer
