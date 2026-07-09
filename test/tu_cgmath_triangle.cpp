#include <gtest/gtest.h>

#include "../src/cgmath/geometry.h"

TEST(TEST_cgmath_triangle, GetIntersectionWithRay)
{
    // context
    Triangle tri;
    tri.SetVertex(0, 0.f, 0.f, 0.f);
    tri.SetVertex(1, 1.f, 1.f, 0.f);
    tri.SetVertex(2, 1.f, 1.f, 2.f);
    Vector3f o = {.5f, 0.f, .5f};
    Vector3f d = {0.f, 1.f, 0.f};

    // action
    float t = 0.f;
    Vector3f i = {0.f, 0.f, 0.f};
    Vector3f n = {0.f, 0.f, 0.f};
    tri.GetIntersectionWithRay(o, d, &t, i, n);

    // expectations
    EXPECT_EQ(t, .5f);
    EXPECT_EQ(i[0], .5f);
    EXPECT_EQ(i[1], .5f);
    EXPECT_EQ(i[2], .5f);
    EXPECT_FLOAT_EQ(n[0], .707107f);
    EXPECT_FLOAT_EQ(n[1], -.707107f);
    EXPECT_EQ(n[2], 0.f);
}
