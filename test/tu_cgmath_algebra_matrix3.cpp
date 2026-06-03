#include <gtest/gtest.h>
#include "../cgmath/cgmath.h"

TEST(TEST_cgmath_matrix3, DeterminantAndTranspose) {
    mat3 m;
    mat3_init(m, 1, 2, 3, 4, 5, 6, 7, 8, 10);
    EXPECT_NEAR(mat3_determinant(m), -3.0f, 1e-5f);
    
    mat3_transpose(m);
    EXPECT_FLOAT_EQ(m[0][1], 4.0f);
    EXPECT_FLOAT_EQ(m[1][0], 2.0f);
}
