#include <gtest/gtest.h>

#include "../src/cgmath/algebra_vector3.h"

TEST(TEST_cgmath, init) {
    int i = 0;
    vec3 a;
    vec3_init(a, 1.f, 1.f, 0.f);
    EXPECT_TRUE(a[0] == 1.f);
    EXPECT_TRUE(a[1] == 1.f);
    EXPECT_TRUE(a[2] == 0.f);
}
