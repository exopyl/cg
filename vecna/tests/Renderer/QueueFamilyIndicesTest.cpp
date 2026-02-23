// tests/Renderer/QueueFamilyIndicesTest.cpp
// Unit tests for QueueFamilyIndices struct

#include <gtest/gtest.h>
#include "Vecna/Renderer/VulkanDevice.hpp"

using namespace Vecna::Renderer;

// =============================================================================
// QueueFamilyIndices Tests
// =============================================================================

TEST(QueueFamilyIndicesTest, DefaultConstructedIsIncomplete) {
    QueueFamilyIndices indices;

    EXPECT_FALSE(indices.isComplete());
    EXPECT_FALSE(indices.graphicsFamily.has_value());
    EXPECT_FALSE(indices.presentFamily.has_value());
}

TEST(QueueFamilyIndicesTest, WithOnlyGraphicsFamilyIsIncomplete) {
    QueueFamilyIndices indices;
    indices.graphicsFamily = 0;

    EXPECT_FALSE(indices.isComplete());
    EXPECT_TRUE(indices.graphicsFamily.has_value());
    EXPECT_FALSE(indices.presentFamily.has_value());
}

TEST(QueueFamilyIndicesTest, WithOnlyPresentFamilyIsIncomplete) {
    QueueFamilyIndices indices;
    indices.presentFamily = 0;

    EXPECT_FALSE(indices.isComplete());
    EXPECT_FALSE(indices.graphicsFamily.has_value());
    EXPECT_TRUE(indices.presentFamily.has_value());
}

TEST(QueueFamilyIndicesTest, WithBothFamiliesIsComplete) {
    QueueFamilyIndices indices;
    indices.graphicsFamily = 0;
    indices.presentFamily = 0;

    EXPECT_TRUE(indices.isComplete());
    EXPECT_TRUE(indices.graphicsFamily.has_value());
    EXPECT_TRUE(indices.presentFamily.has_value());
    EXPECT_EQ(indices.graphicsFamily.value(), 0);
    EXPECT_EQ(indices.presentFamily.value(), 0);
}

TEST(QueueFamilyIndicesTest, FamiliesCanBeAnyValidIndex) {
    QueueFamilyIndices indices;

    // Test with index 0 for both
    indices.graphicsFamily = 0;
    indices.presentFamily = 0;
    EXPECT_TRUE(indices.isComplete());
    EXPECT_EQ(indices.graphicsFamily.value(), 0);
    EXPECT_EQ(indices.presentFamily.value(), 0);

    // Test with different indices
    indices.graphicsFamily = 5;
    indices.presentFamily = 3;
    EXPECT_TRUE(indices.isComplete());
    EXPECT_EQ(indices.graphicsFamily.value(), 5);
    EXPECT_EQ(indices.presentFamily.value(), 3);

    // Test with max uint32_t value
    indices.graphicsFamily = UINT32_MAX;
    indices.presentFamily = UINT32_MAX;
    EXPECT_TRUE(indices.isComplete());
    EXPECT_EQ(indices.graphicsFamily.value(), UINT32_MAX);
    EXPECT_EQ(indices.presentFamily.value(), UINT32_MAX);
}

TEST(QueueFamilyIndicesTest, ResetToIncomplete) {
    QueueFamilyIndices indices;
    indices.graphicsFamily = 2;
    indices.presentFamily = 1;

    EXPECT_TRUE(indices.isComplete());

    // Reset graphics by assigning nullopt
    indices.graphicsFamily = std::nullopt;
    EXPECT_FALSE(indices.isComplete());
    EXPECT_FALSE(indices.graphicsFamily.has_value());
    EXPECT_TRUE(indices.presentFamily.has_value());

    // Reset present too
    indices.presentFamily = std::nullopt;
    EXPECT_FALSE(indices.isComplete());
    EXPECT_FALSE(indices.presentFamily.has_value());
}

TEST(QueueFamilyIndicesTest, CopyBehavior) {
    QueueFamilyIndices original;
    original.graphicsFamily = 3;
    original.presentFamily = 4;

    QueueFamilyIndices copy = original;

    EXPECT_TRUE(copy.isComplete());
    EXPECT_EQ(copy.graphicsFamily.value(), 3);
    EXPECT_EQ(copy.presentFamily.value(), 4);

    // Modifying copy doesn't affect original
    copy.graphicsFamily = 7;
    copy.presentFamily = 8;
    EXPECT_EQ(original.graphicsFamily.value(), 3);
    EXPECT_EQ(original.presentFamily.value(), 4);
    EXPECT_EQ(copy.graphicsFamily.value(), 7);
    EXPECT_EQ(copy.presentFamily.value(), 8);
}

TEST(QueueFamilyIndicesTest, DefaultConstructedFamiliesAreNullopt) {
    QueueFamilyIndices indices;

    EXPECT_EQ(indices.graphicsFamily, std::nullopt);
    EXPECT_EQ(indices.presentFamily, std::nullopt);
}
