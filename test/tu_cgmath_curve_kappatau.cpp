#include <gtest/gtest.h>
#include <cmath>
#include "../src/cgmath/curve_kappatau.h"

static float constant_kappa(float s) { (void)s; return 1.f; }
static float zero_tau(float s) { (void)s; return 0.f; }
static float variable_kappa(float s) { return 8.f * s; }
static float sinusoidal_tau(float s) { return std::sin(s) + 3.f * std::sin(2.f * s); }

// Arc length spanned by t in [0,1] — must match kArcLength in curve_kappatau.cpp.
static const float kL = 3.f * 3.14f;

TEST(TEST_cgmath_curve_kappatau, Constructor)
{
	// A default curve has neither kappa nor tau: the queries return 0 and
	// eval() reports failure (nothing to integrate) rather than crashing.
	CurveKappaTau curve;

	EXPECT_FLOAT_EQ(curve.curvature(0.5f), 0.f);
	EXPECT_FLOAT_EQ(curve.torsion(0.5f), 0.f);
	Vector3f p(9.f, 9.f, 9.f);
	EXPECT_FALSE(curve.eval(0.5f, p));
}

TEST(TEST_cgmath_curve_kappatau, SetKappa)
{
	CurveKappaTau curve;
	curve.set_kappa(constant_kappa);

	// curvature(t) == kappa(t * L); constant_kappa == 1 everywhere.
	EXPECT_FLOAT_EQ(curve.curvature(0.0f), 1.f);
	EXPECT_FLOAT_EQ(curve.curvature(0.7f), 1.f);
	// tau still unset -> torsion is 0.
	EXPECT_FLOAT_EQ(curve.torsion(0.7f), 0.f);
}

TEST(TEST_cgmath_curve_kappatau, SetTau)
{
	CurveKappaTau curve;

	curve.set_tau(zero_tau);
	EXPECT_FLOAT_EQ(curve.torsion(0.1f), 0.f);

	// A non-trivial tau must be reported back at t mapped to arc length t*L.
	curve.set_tau(sinusoidal_tau);
	EXPECT_FLOAT_EQ(curve.torsion(0.25f), sinusoidal_tau(0.25f * kL));
	EXPECT_FLOAT_EQ(curve.torsion(0.60f), sinusoidal_tau(0.60f * kL));
}

TEST(TEST_cgmath_curve_kappatau, SetKappaAndTau)
{
	CurveKappaTau curve;
	curve.set_kappa(variable_kappa);
	curve.set_tau(sinusoidal_tau);

	// Both queries reflect the set functions under the t -> t*L mapping.
	EXPECT_FLOAT_EQ(curve.curvature(0.5f), variable_kappa(0.5f * kL));
	EXPECT_FLOAT_EQ(curve.torsion(0.5f), sinusoidal_tau(0.5f * kL));

	// eval() now succeeds and yields a finite point.
	Vector3f p;
	ASSERT_TRUE(curve.eval(0.5f, p));
	EXPECT_TRUE(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
}

TEST(TEST_cgmath_curve_kappatau, SetConstantCurvatureZeroTorsion)
{
	// Constant curvature 1 + zero torsion = planar circle of radius R = 1/kappa = 1.
	// The Frenet integration starts at the origin with T=(0,1,0), N=(-1,0,0),
	// so the osculating-circle centre is P + (1/kappa) N = (-1, 0, 0) and the
	// curve stays in the z=0 plane.
	CurveKappaTau curve;
	curve.set_kappa(constant_kappa);
	curve.set_tau(zero_tau);

	EXPECT_FLOAT_EQ(curve.curvature(0.3f), 1.f);
	EXPECT_FLOAT_EQ(curve.torsion(0.3f), 0.f);

	const float cx = -1.f, cy = 0.f, cz = 0.f;   // circle centre
	for (float t : {0.0f, 0.1f, 0.2f})
	{
		Vector3f p;
		ASSERT_TRUE(curve.eval(t, p));
		const float r = std::sqrt((p.x - cx) * (p.x - cx) +
					  (p.y - cy) * (p.y - cy) +
					  (p.z - cz) * (p.z - cz));
		EXPECT_NEAR(r, 1.f, 0.05f) << "point off the unit circle at t=" << t;
		EXPECT_NEAR(p.z, 0.f, 1e-3f) << "curve left the z=0 plane at t=" << t;
	}
}
