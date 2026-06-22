#include <cstdlib>

#include <gtest/gtest.h>

#include "../src/cgmath/minimization_simplex.h"

//
// Nelder-Mead (amoeba) was previously untested. Minimize a convex quadratic with
// a known minimum at (3, 5). amoeba uses 1-based arrays (Numerical Recipes
// convention): the simplex has ndim+1 vertices indexed 1..ndim+1, each a vector
// indexed 1..ndim.
//
static float quad_func(float p[])
{
	float dx = p[1] - 3.f;
	float dy = p[2] - 5.f;
	return dx * dx + dy * dy;
}

TEST(TEST_cgmath_minimization, AmoebaQuadratic)
{
	const int ndim = 2;
	float* p[ndim + 2]; // indices 1..ndim+1 used
	for (int i = 1; i <= ndim + 1; ++i)
		p[i] = (float*)malloc((ndim + 1) * sizeof(float));

	p[1][1] = 0.f; p[1][2] = 0.f;
	p[2][1] = 1.f; p[2][2] = 0.f;
	p[3][1] = 0.f; p[3][2] = 1.f;

	float y[ndim + 2];
	y[1] = quad_func(p[1]);
	y[2] = quad_func(p[2]);
	y[3] = quad_func(p[3]);

	int nfunk = 0;
	amoeba(p, y, ndim, 1e-8f, &quad_func, &nfunk);

	// every simplex vertex has converged near the minimum (3, 5)
	EXPECT_NEAR(p[1][1], 3.f, 1e-2f);
	EXPECT_NEAR(p[1][2], 5.f, 1e-2f);
	EXPECT_GT(nfunk, 0);

	for (int i = 1; i <= ndim + 1; ++i)
		free(p[i]);
}
