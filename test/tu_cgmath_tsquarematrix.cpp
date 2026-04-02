#include <gtest/gtest.h>

#include "../src/cgmath/TSquareMatrix.h"

TEST(TEST_cgmath_tsquarematrix, SizedConstructorCreatesIdentity)
{
    SquareMatrixf matrix(4);
    float src[4] = {1.f, 2.f, 3.f, 4.f};
    float dst[4] = {0.f, 0.f, 0.f, 0.f};

    matrix.Multiply(src, dst);

    EXPECT_FLOAT_EQ(dst[0], 1.f);
    EXPECT_FLOAT_EQ(dst[1], 2.f);
    EXPECT_FLOAT_EQ(dst[2], 3.f);
    EXPECT_FLOAT_EQ(dst[3], 4.f);
}

TEST(TEST_cgmath_tsquarematrix, DeterminantOfDiagonalMatrix)
{
    float data[16] = {
        2.f, 0.f, 0.f, 0.f,
        0.f, 3.f, 0.f, 0.f,
        0.f, 0.f, 4.f, 0.f,
        0.f, 0.f, 0.f, 5.f
    };
    SquareMatrixf matrix(4, data);

    EXPECT_FLOAT_EQ(matrix.Determinant(), 120.f);
}

TEST(TEST_cgmath_tsquarematrix, SolveLinearSystemGaussJordanOnDiagonalMatrix)
{
    float data[16] = {
        2.f, 0.f, 0.f, 0.f,
        0.f, 3.f, 0.f, 0.f,
        0.f, 0.f, 4.f, 0.f,
        0.f, 0.f, 0.f, 5.f
    };
    float right[4] = {4.f, 9.f, 16.f, 25.f};
    float result[4] = {0.f, 0.f, 0.f, 0.f};

    SquareMatrixf matrix(4, data);

    ASSERT_EQ(matrix.SolveLinearSystem(right, result, SOLVE_LINEAR_SYSTEM_GAUSS_JORDAN), 1);
    EXPECT_FLOAT_EQ(result[0], 2.f);
    EXPECT_FLOAT_EQ(result[1], 3.f);
    EXPECT_FLOAT_EQ(result[2], 4.f);
    EXPECT_FLOAT_EQ(result[3], 5.f);
}

TEST(TEST_cgmath_tsquarematrix, GetInverseOfDiagonalMatrix)
{
    float data[16] = {
        2.f, 0.f, 0.f, 0.f,
        0.f, 4.f, 0.f, 0.f,
        0.f, 0.f, 5.f, 0.f,
        0.f, 0.f, 0.f, 10.f
    };
    SquareMatrixf matrix(4, data);
    SquareMatrixf inverse;
    float src[4] = {2.f, 4.f, 5.f, 10.f};
    float dst[4] = {0.f, 0.f, 0.f, 0.f};

    ASSERT_TRUE(matrix.GetInverse(inverse));
    inverse.Multiply(src, dst);

    EXPECT_FLOAT_EQ(dst[0], 1.f);
    EXPECT_FLOAT_EQ(dst[1], 1.f);
    EXPECT_FLOAT_EQ(dst[2], 1.f);
    EXPECT_FLOAT_EQ(dst[3], 1.f);
}

TEST(TEST_cgmath_tsquarematrix, SolveEigenSystemReturnsDiagonalEigenvalues)
{
    float data[16] = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 4.f, 0.f, 0.f,
        0.f, 0.f, 2.f, 0.f,
        0.f, 0.f, 0.f, 3.f
    };
    SquareMatrixf matrix(4, data);
    float eigenvalues[4] = {0.f, 0.f, 0.f, 0.f};

    ASSERT_TRUE(matrix.SolveEigenSystem());
    matrix.GetEigenValues(eigenvalues);

    EXPECT_FLOAT_EQ(eigenvalues[0], 4.f);
    EXPECT_FLOAT_EQ(eigenvalues[1], 3.f);
    EXPECT_FLOAT_EQ(eigenvalues[2], 2.f);
    EXPECT_FLOAT_EQ(eigenvalues[3], 1.f);
}
