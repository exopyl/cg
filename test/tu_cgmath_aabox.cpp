#include <gtest/gtest.h>

#include "../src/cgmath/aabox.h"
#include "../src/cgmath/geometry.h"

TEST(TEST_cgmath_aabox, contains_point)
{
    // context
    Vector3 min(0.f, 0.f, 0.f);
    Vector3 max(1.f, 1.f, 1.f);
    AABox aab(min, max);

    // action 1
    bool bIsInside = aab.contains(.1f, .2f, .3f);

    // expectations
    EXPECT_TRUE(bIsInside);

    // action 1
    bIsInside = aab.contains(.1f, -.2f, .3f);

    // expectations
    EXPECT_FALSE(bIsInside);
}

TEST(TEST_cgmath_aabox, contains_triangle)
{
    // context
    Vector3 min(0.f, 0.f, 0.f);
    Vector3 max(1.f, 1.f, 1.f);
    AABox aab(min, max);

    Triangle tri;
    tri.SetVertex(0, .1f, .1f, .1f);
    tri.SetVertex(1, .1f, .2f, .8f);
    tri.SetVertex(2, .2f, .3f, .4f);

    // action
    bool bIsInside = aab.contains(tri);

    // expectations
    EXPECT_TRUE(bIsInside);
}
