#ifdef _WIN32

#include <gtest/gtest.h>

#include "cgnet/WsaError.h"

#define _WINSOCKAPI_
#include <winsock2.h>

TEST(TEST_cgnet_WsaError, zero_returns_no_error)
{
    EXPECT_EQ(cgnet::WsaErrorString(0), "no error");
}

TEST(TEST_cgnet_WsaError, known_code_includes_symbol_and_decimal)
{
    const std::string s = cgnet::WsaErrorString(WSAECONNREFUSED);
    EXPECT_NE(s.find("WSAECONNREFUSED"), std::string::npos);
    EXPECT_NE(s.find(std::to_string(WSAECONNREFUSED)), std::string::npos);
}

TEST(TEST_cgnet_WsaError, known_code_includes_gloss)
{
    // WSAEADDRINUSE has a debug gloss in the table.
    const std::string s = cgnet::WsaErrorString(WSAEADDRINUSE);
    EXPECT_NE(s.find("WSAEADDRINUSE"), std::string::npos);
    EXPECT_NE(s.find("SO_REUSEADDR"), std::string::npos);
}

TEST(TEST_cgnet_WsaError, unknown_code_still_produces_string)
{
    const std::string s = cgnet::WsaErrorString(987654);
    EXPECT_NE(s.find("987654"), std::string::npos);
    EXPECT_FALSE(s.empty());
}

#endif // _WIN32
