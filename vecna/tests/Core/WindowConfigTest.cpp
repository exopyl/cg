// Tests for Vecna::Core::Window configuration and constants
#include <gtest/gtest.h>

#include "Vecna/Core/Window.hpp"

using namespace Vecna::Core;

// ============================================================================
// Dimension Constants Tests
// ============================================================================

TEST(WindowConstantsTest, MinWidthIsReasonable) {
    EXPECT_GE(Window::MIN_WIDTH, 1u);
    EXPECT_LE(Window::MIN_WIDTH, 640u);  // Should allow small windows
}

TEST(WindowConstantsTest, MinHeightIsReasonable) {
    EXPECT_GE(Window::MIN_HEIGHT, 1u);
    EXPECT_LE(Window::MIN_HEIGHT, 480u);
}

TEST(WindowConstantsTest, MaxWidthIsLargeEnough) {
    EXPECT_GE(Window::MAX_WIDTH, 1920u);   // At least Full HD
    EXPECT_LE(Window::MAX_WIDTH, 65536u);  // Reasonable upper bound
}

TEST(WindowConstantsTest, MaxHeightIsLargeEnough) {
    EXPECT_GE(Window::MAX_HEIGHT, 1080u);
    EXPECT_LE(Window::MAX_HEIGHT, 65536u);
}

TEST(WindowConstantsTest, MinValuesAreLessThanMaxValues) {
    EXPECT_LT(Window::MIN_WIDTH, Window::MAX_WIDTH);
    EXPECT_LT(Window::MIN_HEIGHT, Window::MAX_HEIGHT);
}

// ============================================================================
// Config Default Values Tests
// ============================================================================

TEST(WindowConfigTest, DefaultWidthIs1280) {
    Window::Config config;
    EXPECT_EQ(config.width, 1280u);
}

TEST(WindowConfigTest, DefaultHeightIs720) {
    Window::Config config;
    EXPECT_EQ(config.height, 720u);
}

TEST(WindowConfigTest, DefaultTitleIsVecna) {
    Window::Config config;
    EXPECT_EQ(config.title, "Vecna");
}

TEST(WindowConfigTest, DefaultIsResizable) {
    Window::Config config;
    EXPECT_TRUE(config.resizable);
}

// ============================================================================
// Config Custom Values Tests
// ============================================================================

TEST(WindowConfigTest, CustomDimensions) {
    Window::Config config;
    config.width = 1920;
    config.height = 1080;

    EXPECT_EQ(config.width, 1920u);
    EXPECT_EQ(config.height, 1080u);
}

TEST(WindowConfigTest, CustomTitle) {
    Window::Config config;
    config.title = "Custom Window Title";

    EXPECT_EQ(config.title, "Custom Window Title");
}

TEST(WindowConfigTest, NonResizableWindow) {
    Window::Config config;
    config.resizable = false;

    EXPECT_FALSE(config.resizable);
}

// ============================================================================
// Dimension Validation Bounds Tests
// ============================================================================

TEST(WindowConfigTest, DefaultConfigIsWithinValidRange) {
    Window::Config config;

    EXPECT_GE(config.width, Window::MIN_WIDTH);
    EXPECT_LE(config.width, Window::MAX_WIDTH);
    EXPECT_GE(config.height, Window::MIN_HEIGHT);
    EXPECT_LE(config.height, Window::MAX_HEIGHT);
}

TEST(WindowConfigTest, Resolution720pIsWithinValidRange) {
    EXPECT_GE(1280u, Window::MIN_WIDTH);
    EXPECT_LE(1280u, Window::MAX_WIDTH);
    EXPECT_GE(720u, Window::MIN_HEIGHT);
    EXPECT_LE(720u, Window::MAX_HEIGHT);
}

TEST(WindowConfigTest, Resolution1080pIsWithinValidRange) {
    EXPECT_GE(1920u, Window::MIN_WIDTH);
    EXPECT_LE(1920u, Window::MAX_WIDTH);
    EXPECT_GE(1080u, Window::MIN_HEIGHT);
    EXPECT_LE(1080u, Window::MAX_HEIGHT);
}

TEST(WindowConfigTest, Resolution4KIsWithinValidRange) {
    EXPECT_GE(3840u, Window::MIN_WIDTH);
    EXPECT_LE(3840u, Window::MAX_WIDTH);
    EXPECT_GE(2160u, Window::MIN_HEIGHT);
    EXPECT_LE(2160u, Window::MAX_HEIGHT);
}

// ============================================================================
// Config Edge Cases
// ============================================================================

TEST(WindowConfigTest, EmptyTitleIsAllowed) {
    Window::Config config;
    config.title = "";

    EXPECT_TRUE(config.title.empty());
}

TEST(WindowConfigTest, LongTitleIsAllowed) {
    Window::Config config;
    config.title = std::string(1000, 'A');  // 1000 character title

    EXPECT_EQ(config.title.length(), 1000u);
}

TEST(WindowConfigTest, UnicodeTitle) {
    Window::Config config;
    config.title = "Fenêtre 日本語 🎮";

    EXPECT_FALSE(config.title.empty());
}

// Note: Actual window creation tests require GLFW initialization
// and a display, so they are integration tests rather than unit tests.
// The Window class throws std::invalid_argument for invalid dimensions,
// which could be tested with a proper test fixture that initializes GLFW.
