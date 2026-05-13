#ifdef _WIN32

#include <gtest/gtest.h>

#include "cgnet/NetworkInfo.h"

#include <cctype>

namespace {

bool isValidIPv4String(const std::string& s)
{
    int dots = 0;
    int digitsInOctet = 0;
    int octetValue = 0;
    for (char c : s)
    {
        if (c == '.')
        {
            if (digitsInOctet == 0) return false;
            if (octetValue > 255) return false;
            ++dots;
            digitsInOctet = 0;
            octetValue = 0;
        }
        else if (std::isdigit(static_cast<unsigned char>(c)))
        {
            octetValue = octetValue * 10 + (c - '0');
            ++digitsInOctet;
        }
        else
        {
            return false;
        }
    }
    if (digitsInOctet == 0) return false;
    if (octetValue > 255) return false;
    return dots == 3;
}

} // namespace

TEST(TEST_cgnet_NetworkInfo, ListAdapters_returns_something)
{
    // Any host running this test has at least one non-loopback adapter
    // (the GitHub runners and dev boxes both do). If this ever fails on a
    // minimal VM, downgrade to "does not crash".
    const auto adapters = cgnet::ListAdapters();
    EXPECT_FALSE(adapters.empty());
}

TEST(TEST_cgnet_NetworkInfo, adapter_ipv4_strings_are_well_formed)
{
    for (const auto& a : cgnet::ListAdapters())
    {
        for (const auto& ip : a.ipv4)
            EXPECT_TRUE(isValidIPv4String(ip)) << "Bad IPv4: " << ip;
    }
}

TEST(TEST_cgnet_NetworkInfo, mac_format_when_present)
{
    for (const auto& a : cgnet::ListAdapters())
    {
        if (a.mac.empty()) continue;
        EXPECT_EQ(a.mac.size(), 17u) << a.mac;
        for (size_t i = 2; i < 17; i += 3)
            EXPECT_EQ(a.mac[i], ':') << a.mac;
    }
}

TEST(TEST_cgnet_NetworkInfo, EnumerateSmbShares_does_not_crash)
{
    (void)cgnet::EnumerateSmbShares();
    SUCCEED();
}

#endif // _WIN32
