#include <gtest/gtest.h>
#include <cmath>
#include "../src/cgmath/curve_kappatau.h"

static float constant_kappa(float s) { (void)s; return 1.f; }
static float zero_tau(float s) { (void)s; return 0.f; }
static float variable_kappa(float s) { return 8.f * s; }
static float sinusoidal_tau(float s) { return std::sin(s) + 3.f * std::sin(2.f * s); }

TEST(TEST_cgmath_curve_kappatau, Constructor)
{
	// context & action
	CurveKappaTau curve;

	// expectations - just verifying construction doesn't crash
	EXPECT_TRUE(true);
}

TEST(TEST_cgmath_curve_kappatau, SetKappa)
{
	// context
	CurveKappaTau curve;

	// action
	curve.set_kappa(constant_kappa);

	// expectations - no crash
	EXPECT_TRUE(true);
}

TEST(TEST_cgmath_curve_kappatau, SetTau)
{
	// context
	CurveKappaTau curve;

	// action
	curve.set_tau(zero_tau);

	// expectations - no crash
	EXPECT_TRUE(true);
}

TEST(TEST_cgmath_curve_kappatau, SetKappaAndTau)
{
	// context
	CurveKappaTau curve;

	// action
	curve.set_kappa(variable_kappa);
	curve.set_tau(sinusoidal_tau);

	// expectations - no crash
	EXPECT_TRUE(true);
}

TEST(TEST_cgmath_curve_kappatau, SetConstantCurvatureZeroTorsion)
{
	// context - constant kappa, zero tau = circle
	CurveKappaTau curve;

	// action
	curve.set_kappa(constant_kappa);
	curve.set_tau(zero_tau);

	// expectations - no crash
	EXPECT_TRUE(true);
}
