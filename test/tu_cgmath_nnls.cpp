#include <cstdlib>
#include <cmath>

#include <gtest/gtest.h>

#include "../src/cgmath/nnls.h"

//
// NNLS solves "A x = b with x >= 0". Whatever the input, the returned solution
// must be component-wise non-negative — that is the defining property of NNLS,
// and it holds regardless of the matrix storage convention. (Replaces a former
// no-assertion "ghost" test that also leaked all its buffers.)
//
TEST(TEST_cgmath_nnls, SolutionIsNonNegative)
{
	const int m = 51, n = 3;
	const double k[3] = {0.5, 0.6, 1.0};

	double* A = (double*)malloc(m * n * sizeof(double));
	double* b = (double*)malloc(m * sizeof(double));
	for (int j = 0; j < m; ++j)
	{
		double tj = 0.04 * j;
		for (int i = 0; i < n; ++i)
			A[j * n + i] = exp(-k[i] * tj);
		b[j] = tj; // arbitrary target
	}

	double X[n] = {0, 0, 0};
	nnls_solve(A, m, n, b, X);

	for (int i = 0; i < n; ++i)
		EXPECT_GE(X[i], -1e-9) << "X[" << i << "] must be non-negative";

	free(A);
	free(b);
}
