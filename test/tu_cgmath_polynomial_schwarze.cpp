#include <gtest/gtest.h>
#include <math.h>

#include "../src/cgmath/polynomial.h"

// Schwarze convention: c[0] + c[1]*x + c[2]*x^2 + ... = 0

// =====================
// SolveQuadric
// =====================

TEST(TEST_cgmath_polynomial_schwarze, QuadricTwoRoots)
{
	// context - x^2 - 5x + 6 = 0 => roots 2, 3
	// c[0]=6, c[1]=-5, c[2]=1
	double c[3] = {6.0, -5.0, 1.0};
	double s[2];

	// action
	int n = SolveQuadric(c, s);

	// expectations
	EXPECT_EQ(n, 2);
	for (int i = 0; i < n; i++)
	{
		double val = c[0] + c[1]*s[i] + c[2]*s[i]*s[i];
		EXPECT_NEAR(val, 0.0, 1e-8);
	}
}

TEST(TEST_cgmath_polynomial_schwarze, QuadricOneRoot)
{
	// context - x^2 - 2x + 1 = 0 => root 1 (double)
	double c[3] = {1.0, -2.0, 1.0};
	double s[2];

	// action
	int n = SolveQuadric(c, s);

	// expectations
	EXPECT_EQ(n, 1);
	EXPECT_NEAR(s[0], 1.0, 1e-8);
}

TEST(TEST_cgmath_polynomial_schwarze, QuadricNoRoots)
{
	// context - x^2 + 1 = 0 => no real roots
	double c[3] = {1.0, 0.0, 1.0};
	double s[2];

	// action
	int n = SolveQuadric(c, s);

	// expectations
	EXPECT_EQ(n, 0);
}

// =====================
// SolveCubic
// =====================

TEST(TEST_cgmath_polynomial_schwarze, CubicThreeRoots)
{
	// context - (x-1)(x-2)(x-3) = x^3 - 6x^2 + 11x - 6
	// c[0]=-6, c[1]=11, c[2]=-6, c[3]=1
	double c[4] = {-6.0, 11.0, -6.0, 1.0};
	double s[3];

	// action
	int n = SolveCubic(c, s);

	// expectations
	EXPECT_EQ(n, 3);
	for (int i = 0; i < n; i++)
	{
		double val = c[0] + c[1]*s[i] + c[2]*s[i]*s[i] + c[3]*s[i]*s[i]*s[i];
		EXPECT_NEAR(val, 0.0, 1e-6);
	}
}

TEST(TEST_cgmath_polynomial_schwarze, CubicOneRoot)
{
	// context - x^3 + 1 = 0 => one real root at -1
	double c[4] = {1.0, 0.0, 0.0, 1.0};
	double s[3];

	// action
	int n = SolveCubic(c, s);

	// expectations
	EXPECT_GE(n, 1);
	bool found = false;
	for (int i = 0; i < n; i++)
	{
		double val = c[0] + c[1]*s[i] + c[2]*s[i]*s[i] + c[3]*s[i]*s[i]*s[i];
		if (fabs(val) < 1e-6)
			found = true;
	}
	EXPECT_TRUE(found);
}

TEST(TEST_cgmath_polynomial_schwarze, CubicDoubleRoot)
{
	// context - (x-1)^2*(x+2) = x^3 - 3x + 2 => c[0]=2, c[1]=-3, c[2]=0, c[3]=1
	// Note: disc == 0 case
	double c[4] = {2.0, -3.0, 0.0, 1.0};
	double s[3];

	// action
	int n = SolveCubic(c, s);

	// expectations
	EXPECT_GE(n, 1);
	for (int i = 0; i < n; i++)
	{
		double val = c[0] + c[1]*s[i] + c[2]*s[i]*s[i] + c[3]*s[i]*s[i]*s[i];
		EXPECT_NEAR(val, 0.0, 1e-4);
	}
}

// =====================
// SolveQuartic
// =====================

TEST(TEST_cgmath_polynomial_schwarze, QuarticFourRoots)
{
	// context - (x-1)(x-2)(x-3)(x-4) = x^4 - 10x^3 + 35x^2 - 50x + 24
	// c[0]=24, c[1]=-50, c[2]=35, c[3]=-10, c[4]=1
	double c[5] = {24.0, -50.0, 35.0, -10.0, 1.0};
	double s[4];

	// action
	int n = SolveQuartic(c, s);

	// expectations
	EXPECT_EQ(n, 4);
	for (int i = 0; i < n; i++)
	{
		double val = c[0] + c[1]*s[i] + c[2]*s[i]*s[i]
			+ c[3]*s[i]*s[i]*s[i] + c[4]*s[i]*s[i]*s[i]*s[i];
		EXPECT_NEAR(val, 0.0, 1e-6);
	}
}

TEST(TEST_cgmath_polynomial_schwarze, QuarticNoRealRoots)
{
	// context - x^4 + 1 = 0 => no real roots
	double c[5] = {1.0, 0.0, 0.0, 0.0, 1.0};
	double s[4];

	// action
	int n = SolveQuartic(c, s);

	// expectations
	EXPECT_EQ(n, 0);
}

TEST(TEST_cgmath_polynomial_schwarze, QuarticSymmetric)
{
	// context - x^4 - 5x^2 + 4 = (x-1)(x+1)(x-2)(x+2)
	// c[0]=4, c[1]=0, c[2]=-5, c[3]=0, c[4]=1
	double c[5] = {4.0, 0.0, -5.0, 0.0, 1.0};
	double s[4];

	// action
	int n = SolveQuartic(c, s);

	// expectations
	EXPECT_EQ(n, 4);
	for (int i = 0; i < n; i++)
	{
		double val = c[0] + c[1]*s[i] + c[2]*s[i]*s[i]
			+ c[3]*s[i]*s[i]*s[i] + c[4]*s[i]*s[i]*s[i]*s[i];
		EXPECT_NEAR(val, 0.0, 1e-6);
	}
}
