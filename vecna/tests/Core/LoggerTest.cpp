// Tests for Vecna::Core::Logger
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Vecna/Core/Logger.hpp"

#include <sstream>
#include <iostream>

using namespace Vecna::Core;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

// Helper to capture stdout/stderr
class OutputCapture {
public:
    OutputCapture()
        : m_oldCout(std::cout.rdbuf(m_coutBuffer.rdbuf()))
        , m_oldCerr(std::cerr.rdbuf(m_cerrBuffer.rdbuf()))
    {}

    ~OutputCapture() {
        std::cout.rdbuf(m_oldCout);
        std::cerr.rdbuf(m_oldCerr);
    }

    std::string getCout() const { return m_coutBuffer.str(); }
    std::string getCerr() const { return m_cerrBuffer.str(); }

private:
    std::stringstream m_coutBuffer;
    std::stringstream m_cerrBuffer;
    std::streambuf* m_oldCout;
    std::streambuf* m_oldCerr;
};

// Test fixture that saves and restores logger level
class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_originalLevel = Logger::getMinLevel();
    }

    void TearDown() override {
        Logger::setMinLevel(m_originalLevel);
    }

    Logger::Level m_originalLevel;
};

// ============================================================================
// Level Enum Tests
// ============================================================================

TEST(LoggerLevelTest, DebugIsLowestLevel) {
    EXPECT_LT(static_cast<int>(Logger::Level::Debug), static_cast<int>(Logger::Level::Info));
}

TEST(LoggerLevelTest, InfoIsBelowWarn) {
    EXPECT_LT(static_cast<int>(Logger::Level::Info), static_cast<int>(Logger::Level::Warn));
}

TEST(LoggerLevelTest, WarnIsBelowError) {
    EXPECT_LT(static_cast<int>(Logger::Level::Warn), static_cast<int>(Logger::Level::Error));
}

// ============================================================================
// Min Level Filtering Tests
// ============================================================================

TEST_F(LoggerTest, SetAndGetMinLevel) {
    Logger::setMinLevel(Logger::Level::Warn);
    EXPECT_EQ(Logger::getMinLevel(), Logger::Level::Warn);

    Logger::setMinLevel(Logger::Level::Debug);
    EXPECT_EQ(Logger::getMinLevel(), Logger::Level::Debug);
}

TEST_F(LoggerTest, MessagesBelowMinLevelAreFiltered) {
    Logger::setMinLevel(Logger::Level::Warn);

    OutputCapture capture;
    Logger::debug("Test", "This should not appear");
    Logger::info("Test", "This should not appear either");

    EXPECT_THAT(capture.getCout(), IsEmpty());
    EXPECT_THAT(capture.getCerr(), IsEmpty());
}

TEST_F(LoggerTest, MessagesAtOrAboveMinLevelAreShown) {
    Logger::setMinLevel(Logger::Level::Warn);

    OutputCapture capture;
    Logger::warn("Test", "Warning message");
    Logger::error("Test", "Error message");

    // Warn and Error go to stderr
    EXPECT_THAT(capture.getCerr(), HasSubstr("Warning message"));
    EXPECT_THAT(capture.getCerr(), HasSubstr("Error message"));
}

// ============================================================================
// Output Format Tests
// ============================================================================

TEST_F(LoggerTest, InfoMessagesGoToStdoutWithModulePrefix) {
    Logger::setMinLevel(Logger::Level::Debug);

    OutputCapture capture;
    Logger::info("MyModule", "Test message");

    EXPECT_THAT(capture.getCout(), HasSubstr("[MyModule]"));
    EXPECT_THAT(capture.getCout(), HasSubstr("Test message"));
}

TEST_F(LoggerTest, ErrorMessagesGoToStderr) {
    Logger::setMinLevel(Logger::Level::Debug);

    OutputCapture capture;
    Logger::error("ErrorModule", "Error occurred");

    EXPECT_THAT(capture.getCout(), IsEmpty());
    EXPECT_THAT(capture.getCerr(), HasSubstr("[ErrorModule]"));
    EXPECT_THAT(capture.getCerr(), HasSubstr("Error occurred"));
}

TEST_F(LoggerTest, WarnMessagesGoToStderr) {
    Logger::setMinLevel(Logger::Level::Debug);

    OutputCapture capture;
    Logger::warn("WarnModule", "Warning issued");

    EXPECT_THAT(capture.getCout(), IsEmpty());
    EXPECT_THAT(capture.getCerr(), HasSubstr("[WarnModule]"));
}

TEST_F(LoggerTest, DebugMessagesGoToStdout) {
    Logger::setMinLevel(Logger::Level::Debug);

    OutputCapture capture;
    Logger::debug("DebugModule", "Debug info");

    EXPECT_THAT(capture.getCout(), HasSubstr("[DebugModule]"));
    EXPECT_THAT(capture.getCerr(), IsEmpty());
}

// ============================================================================
// Special Characters Tests
// ============================================================================

TEST_F(LoggerTest, HandlesEmptyModuleName) {
    Logger::setMinLevel(Logger::Level::Debug);

    OutputCapture capture;
    Logger::info("", "Message with empty module");

    EXPECT_THAT(capture.getCout(), HasSubstr("[]"));
}

TEST_F(LoggerTest, HandlesEmptyMessage) {
    Logger::setMinLevel(Logger::Level::Debug);

    OutputCapture capture;
    Logger::info("Module", "");

    EXPECT_THAT(capture.getCout(), HasSubstr("[Module]"));
}

TEST_F(LoggerTest, HandlesSpecialCharactersInMessage) {
    Logger::setMinLevel(Logger::Level::Debug);

    OutputCapture capture;
    Logger::info("Test", "Path: C:\\Users\\test\\file.txt");

    EXPECT_THAT(capture.getCout(), HasSubstr("C:\\Users\\test\\file.txt"));
}

TEST_F(LoggerTest, HandlesNewlinesInMessage) {
    Logger::setMinLevel(Logger::Level::Debug);

    OutputCapture capture;
    Logger::info("Test", "Line1\nLine2");

    EXPECT_THAT(capture.getCout(), HasSubstr("Line1\nLine2"));
}
