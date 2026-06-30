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

TEST(TEST_cgimg_io, png_rgb)
{
    // context
    Img img;

    // action: 256x256 RGB PNG
    auto res = img.load("./test/data/fallout_mask.png");

    // expectations
    EXPECT_EQ(res, 0);
    EXPECT_EQ(img.width(), 256);
    EXPECT_EQ(img.height(), 256);
    // RGB source: alpha is forced opaque on import
    EXPECT_EQ(img.get_a(0, 0), 255);
}

TEST(TEST_cgimg_io, png_rgba)
{
    // context
    Img img;

    // action: 512x512 RGBA PNG
    auto res = img.load("./test/data/fallout_mask2.png");

    // expectations
    EXPECT_EQ(res, 0);
    EXPECT_EQ(img.width(), 512);
    EXPECT_EQ(img.height(), 512);
}

TEST(TEST_cgimg_io, jpg_rgb)
{
    // context
    Img img;

    // action: JPEG RGB (décodage stb)
    auto res = img.load("./test/data/jpg/Nicolae_Grigorescu_005.jpg");

    // expectations
    EXPECT_EQ(res, 0);
    EXPECT_EQ(img.width(), 1576);
    EXPECT_EQ(img.height(), 2186);
    EXPECT_EQ(img.get_a(0, 0), 255);   // source RGB : alpha forcé opaque à l'import
}
