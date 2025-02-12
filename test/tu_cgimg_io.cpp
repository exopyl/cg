#include <gtest/gtest.h>

#include "../src/cgimg/cgimg.h"

TEST(TEST_cgimg_io, tga_ctc16)
{
    // context
    Img img;

    // action 1
    auto res = img.load("./test/data/tga/ctc16.tga");

    // expectations
    EXPECT_EQ(res, 0);
    EXPECT_EQ(img.width(), 128);
    EXPECT_EQ(img.height(), 128);
}

TEST(TEST_cgimg_io, tga_ctc24)
{
    // context
    Img img;

    // action 1
    auto res = img.load("./test/data/tga/ctc24.tga");

    // expectations
    EXPECT_EQ(res, 0);
    EXPECT_EQ(img.width(), 128);
    EXPECT_EQ(img.height(), 128);
}

TEST(TEST_cgimg_io, tga_ctc32)
{
    // context
    Img img;

    // action 1
    auto res = img.load("./test/data/tga/ctc32.tga");

    // expectations
    EXPECT_EQ(res, 0);
    EXPECT_EQ(img.width(), 128);
    EXPECT_EQ(img.height(), 128);
}
