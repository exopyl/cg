#include <gtest/gtest.h>
#include <math.h>

#include "../src/cgmath/polynomial.h"

// =====================
// polynomial2_eval
// =====================

TEST(TEST_cgmath_polynomial, Poly2EvalZero)
{
	// context - 2x^2 + 3x + 1 at x=0
	// action
	float val = polynomial2_eval(2.f, 3.f, 1.f, 0.f);

	// expectations
	EXPECT_FLOAT_EQ(val, 1.f);
}

TEST(TEST_cgmath_polynomial, Poly2EvalGeneral)
{
	// context - x^2 - 1 at x=2
	// action
	float val = polynomial2_eval(1.f, 0.f, -1.f, 2.f);

	// expectations
	EXPECT_FLOAT_EQ(val, 3.f);
}

// =====================
// polynomial2_find_roots
// =====================

TEST(TEST_cgmath_polynomial, Poly2TwoRoots)
{
	// context - x^2 - 5x + 6 = 0 => roots 2, 3
	float roots[2];

	// action
	int n = polynomial2_find_roots(1.f, -5.f, 6.f, roots);

	// expectations
	EXPECT_EQ(n, 2);
	EXPECT_FLOAT_EQ(roots[0], 2.f);
	EXPECT_FLOAT_EQ(roots[1], 3.f);
}

TEST(TEST_cgmath_polynomial, Poly2OneRoot)
{
	// context - x^2 - 2x + 1 = 0 => root 1 (double)
	float roots[2];

	// action
	int n = polynomial2_find_roots(1.f, -2.f, 1.f, roots);

	// expectations
	EXPECT_EQ(n, 1);
	EXPECT_FLOAT_EQ(roots[0], 1.f);
}

TEST(TEST_cgmath_polynomial, Poly2NoRoots)
{
	// context - x^2 + 1 = 0 => no real roots
	float roots[2];

	// action
	int n = polynomial2_find_roots(1.f, 0.f, 1.f, roots);

	// expectations
	EXPECT_EQ(n, 0);
}

TEST(TEST_cgmath_polynomial, Poly2RootsVerify)
{
	// context - x^2 - 3x + 2 = 0 => roots 1, 2
	float roots[2];

	// action
	int n = polynomial2_find_roots(1.f, -3.f, 2.f, roots);

	// expectations - verify roots by substitution
	EXPECT_EQ(n, 2);
	for (int i = 0; i < n; i++)
		EXPECT_NEAR(polynomial2_eval(1.f, -3.f, 2.f, roots[i]), 0.f, 1e-4f);
}

// =====================
// polynomial3_eval
// =====================

TEST(TEST_cgmath_polynomial, Poly3EvalZero)
{
	// context - x^3 + 2x^2 + 3x + 4 at x=0
	// action
	float val = polynomial3_eval(1.f, 2.f, 3.f, 4.f, 0.f);

	// expectations
	EXPECT_FLOAT_EQ(val, 4.f);
}

TEST(TEST_cgmath_polynomial, Poly3EvalGeneral)
{
	// context - x^3 at x=3
	// action
	float val = polynomial3_eval(1.f, 0.f, 0.f, 0.f, 3.f);

	// expectations
	EXPECT_FLOAT_EQ(val, 27.f);
}

// =====================
// polynomial3_find_roots
// =====================

TEST(TEST_cgmath_polynomial, Poly3OneRoot)
{
	// context - x^3 + 1 = 0 => one real root at x=-1
	float roots[3];

	// action
	int n = polynomial3_find_roots(1.f, 0.f, 0.f, 1.f, roots);

	// expectations
	EXPECT_GE(n, 1);
	// at least one root should satisfy the equation
	bool found = false;
	for (int i = 0; i < n; i++)
	{
		if (fabsf(polynomial3_eval(1.f, 0.f, 0.f, 1.f, roots[i])) < 0.01f)
			found = true;
	}
	EXPECT_TRUE(found);
}

TEST(TEST_cgmath_polynomial, Poly3ThreeRoots)
{
	// context - x^3 - 6x^2 + 11x - 6 = 0 => roots 1, 2, 3
	float roots[3];

	// action
	int n = polynomial3_find_roots(1.f, -6.f, 11.f, -6.f, roots);

	// expectations
	EXPECT_EQ(n, 3);
	for (int i = 0; i < n; i++)
		EXPECT_NEAR(polynomial3_eval(1.f, -6.f, 11.f, -6.f, roots[i]), 0.f, 0.01f);
}

TEST(TEST_cgmath_polynomial, Poly3FallsBackToPoly2)
{
	// context - a=0 => degenerate to quadratic: 0*x^3 + x^2 - 3x + 2 = 0
	float roots[3];

	// action
	int n = polynomial3_find_roots(0.f, 1.f, -3.f, 2.f, roots);

	// expectations
	EXPECT_EQ(n, 2);
}
