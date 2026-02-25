#include <gtest/gtest.h>
#include <math.h>

#include "../src/cgmath/polynomial.h"

// quartic: x^4 + a*x^3 + b*x^2 + c*x + d = 0

TEST(TEST_cgmath_polynomial_herbison_evans, QuarticKnownRoots)
{
	// context - (x-1)(x-2)(x-3)(x-4) = x^4 - 10x^3 + 35x^2 - 50x + 24
	double rts[4];
	double rterr[4];

	// action
	int n = quartic(-10.0, 35.0, -50.0, 24.0, rts, rterr);

	// expectations
	EXPECT_EQ(n, 4);
	// verify each root satisfies the equation
	for (int i = 0; i < n; i++)
	{
		double val = rts[i]*rts[i]*rts[i]*rts[i]
			- 10.0*rts[i]*rts[i]*rts[i]
			+ 35.0*rts[i]*rts[i]
			- 50.0*rts[i] + 24.0;
		EXPECT_NEAR(val, 0.0, 0.01);
	}
}

TEST(TEST_cgmath_polynomial_herbison_evans, QuarticTwoRoots)
{
	// context - x^4 - 5x^2 + 4 = (x-1)(x+1)(x-2)(x+2) => a=0, b=-5, c=0, d=4
	double rts[4];
	double rterr[4];

	// action
	int n = quartic(0.0, -5.0, 0.0, 4.0, rts, rterr);

	// expectations
	EXPECT_EQ(n, 4);
	for (int i = 0; i < n; i++)
	{
		double val = rts[i]*rts[i]*rts[i]*rts[i]
			- 5.0*rts[i]*rts[i] + 4.0;
		EXPECT_NEAR(val, 0.0, 0.01);
	}
}

TEST(TEST_cgmath_polynomial_herbison_evans, QuarticNoRealRoots)
{
	// context - x^4 + 1 = 0 => no real roots => a=0, b=0, c=0, d=1
	double rts[4];
	double rterr[4];

	// action
	int n = quartic(0.0, 0.0, 0.0, 1.0, rts, rterr);

	// expectations
	EXPECT_EQ(n, 0);
}

TEST(TEST_cgmath_polynomial_herbison_evans, QuarticDoubleRoots)
{
	// context - (x-1)^2*(x-2)^2 = x^4 - 6x^3 + 13x^2 - 12x + 4
	double rts[4];
	double rterr[4];

	// action
	int n = quartic(-6.0, 13.0, -12.0, 4.0, rts, rterr);

	// expectations - should find roots, each close to 1 or 2
	EXPECT_GE(n, 2);
	for (int i = 0; i < n; i++)
	{
		double val = (rts[i]-1.0)*(rts[i]-1.0)*(rts[i]-2.0)*(rts[i]-2.0);
		EXPECT_NEAR(val, 0.0, 0.1);
	}
}

TEST(TEST_cgmath_polynomial_herbison_evans, QuarticErrorsSmall)
{
	// context - well-conditioned quartic
	double rts[4];
	double rterr[4];

	// action
	int n = quartic(-10.0, 35.0, -50.0, 24.0, rts, rterr);

	// expectations - errors should be small
	EXPECT_EQ(n, 4);
	for (int i = 0; i < n; i++)
		EXPECT_LT(rterr[i], 1e-6);
}
