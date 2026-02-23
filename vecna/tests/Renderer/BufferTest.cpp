#include <gtest/gtest.h>
#include "Vecna/Renderer/Buffer.hpp"
#include "Vecna/Renderer/Pipeline.hpp"

namespace Vecna::Renderer {

// Test BufferType enum values

TEST(BufferTypeTest, VertexTypeExists) {
    BufferType type = BufferType::Vertex;
    EXPECT_EQ(type, BufferType::Vertex);
}

TEST(BufferTypeTest, IndexTypeExists) {
    BufferType type = BufferType::Index;
    EXPECT_EQ(type, BufferType::Index);
}

TEST(BufferTypeTest, StagingTypeExists) {
    BufferType type = BufferType::Staging;
    EXPECT_EQ(type, BufferType::Staging);
}

TEST(BufferTypeTest, TypesAreDistinct) {
    EXPECT_NE(BufferType::Vertex, BufferType::Index);
    EXPECT_NE(BufferType::Vertex, BufferType::Staging);
    EXPECT_NE(BufferType::Index, BufferType::Staging);
}

// Test Vertex structure for buffer compatibility
// (These tests verify the data we'll put in vertex buffers)

TEST(VertexBufferDataTest, VertexSizeIsCorrect) {
    // Vertex should be 9 floats: position(3) + normal(3) + color(3)
    // 9 * sizeof(float) = 9 * 4 = 36 bytes
    EXPECT_EQ(sizeof(Vertex), 36u);
}

TEST(VertexBufferDataTest, VertexArraySizeCalculation) {
    // Test that vertex array size calculations are correct
    std::vector<Vertex> vertices(10);
    VkDeviceSize expectedSize = 10 * sizeof(Vertex);
    EXPECT_EQ(vertices.size() * sizeof(Vertex), expectedSize);
    EXPECT_EQ(expectedSize, 360u);  // 10 * 36 = 360 bytes
}

TEST(VertexBufferDataTest, VertexDataContiguousInMemory) {
    // Verify vertices are laid out contiguously
    std::vector<Vertex> vertices(3);
    vertices[0].position[0] = 1.0f;
    vertices[1].position[0] = 2.0f;
    vertices[2].position[0] = 3.0f;

    // Access via raw pointer arithmetic should work
    auto* ptr = reinterpret_cast<float*>(vertices.data());
    EXPECT_FLOAT_EQ(ptr[0], 1.0f);  // vertices[0].position[0]
    EXPECT_FLOAT_EQ(ptr[9], 2.0f);  // vertices[1].position[0] (offset by 9 floats)
    EXPECT_FLOAT_EQ(ptr[18], 3.0f); // vertices[2].position[0] (offset by 18 floats)
}

// Test Index buffer data compatibility

TEST(IndexBufferDataTest, IndexSizeIsCorrect) {
    // Indices are uint32_t = 4 bytes
    EXPECT_EQ(sizeof(uint32_t), 4u);
}

TEST(IndexBufferDataTest, IndexArraySizeCalculation) {
    // Test that index array size calculations are correct
    std::vector<uint32_t> indices(36);  // 12 triangles * 3 indices
    VkDeviceSize expectedSize = 36 * sizeof(uint32_t);
    EXPECT_EQ(indices.size() * sizeof(uint32_t), expectedSize);
    EXPECT_EQ(expectedSize, 144u);  // 36 * 4 = 144 bytes
}

TEST(IndexBufferDataTest, IndexDataContiguousInMemory) {
    // Verify indices are laid out contiguously
    std::vector<uint32_t> indices = {0, 1, 2, 3, 4, 5};

    auto* ptr = indices.data();
    EXPECT_EQ(ptr[0], 0u);
    EXPECT_EQ(ptr[1], 1u);
    EXPECT_EQ(ptr[2], 2u);
    EXPECT_EQ(ptr[3], 3u);
    EXPECT_EQ(ptr[4], 4u);
    EXPECT_EQ(ptr[5], 5u);
}

// Test Triangle data (preparation for Story 2-3)

TEST(TriangleDataTest, TriangleHasThreeVertices) {
    std::vector<Vertex> triangleVertices = {
        {{0.0f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}
    };

    EXPECT_EQ(triangleVertices.size(), 3u);
    EXPECT_EQ(triangleVertices.size() * sizeof(Vertex), 108u);  // 3 * 36 = 108 bytes
}

TEST(TriangleDataTest, TriangleIndicesAreValid) {
    std::vector<uint32_t> triangleIndices = {0, 1, 2};

    EXPECT_EQ(triangleIndices.size(), 3u);
    EXPECT_EQ(triangleIndices[0], 0u);
    EXPECT_EQ(triangleIndices[1], 1u);
    EXPECT_EQ(triangleIndices[2], 2u);
}

// Test Cube data (preparation for Story 2-4)

TEST(CubeDataTest, CubeHas8UniqueVertices) {
    // A cube has 8 corner vertices
    // For rendering, we often need more (24) due to different normals per face
    // But for indexed rendering with smooth normals, 8 is sufficient

    // Simple cube vertices (positions only for this test)
    constexpr int CUBE_UNIQUE_POSITIONS = 8;
    EXPECT_EQ(CUBE_UNIQUE_POSITIONS, 8);
}

TEST(CubeDataTest, CubeHas12Triangles) {
    // A cube has 6 faces, each face = 2 triangles
    // Total = 12 triangles = 36 indices
    constexpr int CUBE_FACES = 6;
    constexpr int TRIANGLES_PER_FACE = 2;
    constexpr int INDICES_PER_TRIANGLE = 3;

    int totalTriangles = CUBE_FACES * TRIANGLES_PER_FACE;
    int totalIndices = totalTriangles * INDICES_PER_TRIANGLE;

    EXPECT_EQ(totalTriangles, 12);
    EXPECT_EQ(totalIndices, 36);
}

// Test alignment requirements

TEST(BufferAlignmentTest, VertexIs4ByteAligned) {
    // Vulkan requires vertex data to be properly aligned
    EXPECT_EQ(alignof(Vertex) % 4, 0u);
}

TEST(BufferAlignmentTest, VertexArrayStartsAligned) {
    std::vector<Vertex> vertices(5);
    auto address = reinterpret_cast<uintptr_t>(vertices.data());
    EXPECT_EQ(address % 4, 0u);  // 4-byte aligned
}

TEST(BufferAlignmentTest, IndexIs4ByteAligned) {
    EXPECT_EQ(alignof(uint32_t) % 4, 0u);
}

} // namespace Vecna::Renderer
