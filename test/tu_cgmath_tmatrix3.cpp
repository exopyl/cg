#include <gtest/gtest.h>

#include "../src/cgmath/TMatrix3.h"

// Diagonal system M x = b with known solution x = (1, 2, 3).
TEST(TEST_cgmath_TMatrix3, SolveLinearSystem)
{
	Matrix3f M(2.f, 0.f, 0.f,
		   0.f, 3.f, 0.f,
		   0.f, 0.f, 4.f);
	Vector3f b(2.f, 6.f, 12.f), x;

	ASSERT_TRUE(M.SolveLinearSystem(b, x));
	EXPECT_FLOAT_EQ(x.x, 1.f);
	EXPECT_FLOAT_EQ(x.y, 2.f);
	EXPECT_FLOAT_EQ(x.z, 3.f);
}

// Singular matrix (row 1 = 2 * row 0) -> no solution.
TEST(TEST_cgmath_TMatrix3, SolveLinearSystemSingular)
{
	Matrix3f M(1.f, 2.f, 3.f,
		   2.f, 4.f, 6.f,
		   1.f, 1.f, 1.f);
	Vector3f b(1.f, 2.f, 3.f), x;

	EXPECT_FALSE(M.SolveLinearSystem(b, x));
}

// General system: the solution must reproduce b when substituted back (M x = b).
TEST(TEST_cgmath_TMatrix3, SolveLinearSystemResidual)
{
	Matrix3f M(3.f, -1.f, 2.f,
		   1.f,  5.f, 1.f,
		   4.f,  2.f, -3.f);
	Vector3f b(4.f, 7.f, 1.f), x;
	ASSERT_TRUE(M.SolveLinearSystem(b, x));

	Vector3f r = M * x;   // substitute back
	EXPECT_NEAR(r.x, b.x, 1e-4f);
	EXPECT_NEAR(r.y, b.y, 1e-4f);
	EXPECT_NEAR(r.z, b.z, 1e-4f);
}
