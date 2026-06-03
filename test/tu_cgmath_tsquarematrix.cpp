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

TEST(TEST_cgmath_tsquarematrix, SetIdentity)
{
    SquareMatrixf matrix(4); // Starts as identity if initialized that way or just set
    matrix.SetIdentity();
    
    float src[4] = {1.f, 2.f, 3.f, 4.f};
    float dst[4] = {0.f, 0.f, 0.f, 0.f};
    matrix.Multiply(src, dst);

    EXPECT_FLOAT_EQ(dst[0], 1.f);
    EXPECT_FLOAT_EQ(dst[1], 2.f);
    EXPECT_FLOAT_EQ(dst[2], 3.f);
    EXPECT_FLOAT_EQ(dst[3], 4.f);
}

TEST(TEST_cgmath_tsquarematrix, Transpose)
{
    float data[16] = {
        1.f, 2.f, 3.f, 4.f,
        5.f, 6.f, 7.f, 8.f,
        9.f, 10.f, 11.f, 12.f,
        13.f, 14.f, 15.f, 16.f
    };
    SquareMatrixf matrix(4, data);
    matrix.Transpose();

    // Accès correct via les indices [ligne][colonne]
    EXPECT_FLOAT_EQ(matrix.a[0][1], 5.f);  // [0][1] devient 5.f
    EXPECT_FLOAT_EQ(matrix.a[1][0], 2.f);  // [1][0] devient 2.f
}

TEST(TEST_cgmath_tsquarematrix, Set)
{
    float data[16] = {
        1.f, 2.f, 3.f, 4.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    };
    SquareMatrixf matrix;
    matrix.Set(4, data);
    
    float src[4] = {1.f, 2.f, 3.f, 4.f};
    float dst[4] = {0.f, 0.f, 0.f, 0.f};
    matrix.Multiply(src, dst);

    // M*src = {1*1+2*2+3*3+4*4, 2, 3, 4} = {30, 2, 3, 4}
    EXPECT_FLOAT_EQ(dst[0], 30.f);
    EXPECT_FLOAT_EQ(dst[1], 2.f);
    EXPECT_FLOAT_EQ(dst[2], 3.f);
    EXPECT_FLOAT_EQ(dst[3], 4.f);
}

TEST(TEST_cgmath_tsquarematrix, MultiplyGeneric)
{
    float data[16] = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 2.f, 0.f, 0.f,
        0.f, 0.f, 3.f, 0.f,
        0.f, 0.f, 0.f, 4.f
    };
    SquareMatrixf matrix(4, data);
    float src[4] = {1.f, 1.f, 1.f, 1.f};
    float dst[4] = {0.f, 0.f, 0.f, 0.f};
    
    matrix.Multiply(src, dst);

    EXPECT_FLOAT_EQ(dst[0], 1.f);
    EXPECT_FLOAT_EQ(dst[1], 2.f);
    EXPECT_FLOAT_EQ(dst[2], 3.f);
    EXPECT_FLOAT_EQ(dst[3], 4.f);
}
