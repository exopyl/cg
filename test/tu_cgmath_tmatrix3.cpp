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

// Each returned pair must satisfy M e = lambda e with |e| = 1, the vectors must
// be mutually orthogonal, and the eigenvalues sorted in decreasing order.
template <typename T>
static void expect_eigensystem (const TMatrix3<T>& M, T tol)
{
	TMatrix3<T> copy (M);
	TVector3<T> e[3], v;
	ASSERT_TRUE (copy.SolveEigensystem (e[0], e[1], e[2], v));
	EXPECT_GE (v[0], v[1]);
	EXPECT_GE (v[1], v[2]);
	for (int k = 0; k < 3; ++k)
	{
		SCOPED_TRACE (k);
		EXPECT_NEAR (e[k].getLength (), T (1), tol);
		const TVector3<T> r = M * e[k] - e[k] * v[k];
		EXPECT_NEAR (r.getLength (), T (0), tol);
		EXPECT_NEAR (e[k] * e[(k + 1) % 3], T (0), tol);
	}
}

// Off-diagonal entries of the first row exactly zero: the solver must still
// diagonalize the remaining 2x2 block. Eigenvalues 3, 1, 1.
TEST(TEST_cgmath_TMatrix3, SolveEigensystemDecoupledBlock)
{
	const Matrix3d M (1., 0., 0.,
	                  0., 2., 1.,
	                  0., 1., 2.);
	Matrix3d copy (M);
	Vector3d e1, e2, e3, v;
	ASSERT_TRUE (copy.SolveEigensystem (e1, e2, e3, v));
	EXPECT_NEAR (v[0], 3., 1e-12);
	EXPECT_NEAR (v[1], 1., 1e-12);
	EXPECT_NEAR (v[2], 1., 1e-12);
	expect_eigensystem (M, 1e-12);
}

// Dense symmetric matrix: one QL sweep per index does not converge.
TEST(TEST_cgmath_TMatrix3, SolveEigensystemDense)
{
	expect_eigensystem (Matrix3d (4.,  1.,  2.,
	                              1.,  3., 0.5,
	                              2., 0.5,  5.), 1e-12);
	expect_eigensystem (Matrix3d (-2., 3., -1.,
	                               3., 1.,  4.,
	                              -1., 4.,  0.), 1e-12);
}

// Taubin matrix of an icosphere vertex lying on the plane x = 0, where the
// coupling of the x axis is rounding noise (either exactly 0 or ~1e-9 of either
// sign depending on the compiler). The eigenvalues must not depend on it.
TEST(TEST_cgmath_TMatrix3, SolveEigensystemNoisyZeros)
{
	const float noise[] = {0.f, 5.3e-10f, -5.3e-10f};
	for (float a : noise)
		for (float b : noise)
		{
			SCOPED_TRACE (testing::Message () << a << ' ' << b);
			const Matrix3f M (-0.263772219f, a, b,
			                  a, -0.123459697f, 0.118518427f,
			                  b, 0.118518427f, -0.113774918f);
			Matrix3f copy (M);
			Vector3f e1, e2, e3, v;
			ASSERT_TRUE (copy.SolveEigensystem (e1, e2, e3, v));
			EXPECT_NEAR (v[0], 0.f, 1e-6f);
			EXPECT_NEAR (v[1], -0.2372346f, 1e-6f);
			EXPECT_NEAR (v[2], -0.2637722f, 1e-6f);
			expect_eigensystem (M, 1e-5f);
		}
}
