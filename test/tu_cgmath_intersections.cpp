#include <gtest/gtest.h>

#include "../src/cgmath/cgmath.h"

//
// Characterization of the 2D intersection routines, pinning their results
// across the vec2 -> TVector2 migration of algebra_intersections. Written with
// plain float storage so it compiles against both the old (vec2) and new
// (TVector2 / float*) signatures.
//

TEST(TEST_cgmath_intersections, Seg2Seg2_Crossing)
{
	seg2 s1, s2;
	s1.vs[0] = 0; s1.vs[1] = 0; s1.ve[0] = 2; s1.ve[1] = 2; // diagonal
	s2.vs[0] = 0; s2.vs[1] = 2; s2.ve[0] = 2; s2.ve[1] = 0; // anti-diagonal
	float res[2];
	unsigned int code = seg2_seg2_intersection(s1, s2, res);
	EXPECT_EQ(code, 1u);
	EXPECT_NEAR(res[0], 1.f, 1e-5f);
	EXPECT_NEAR(res[1], 1.f, 1e-5f);
}

TEST(TEST_cgmath_intersections, Seg2Seg2_Disjoint)
{
	seg2 s1, s2;
	s1.vs[0] = 0; s1.vs[1] = 0; s1.ve[0] = 1; s1.ve[1] = 0;
	s2.vs[0] = 0; s2.vs[1] = 1; s2.ve[0] = 1; s2.ve[1] = 1; // parallel, not collinear
	float res[2];
	unsigned int code = seg2_seg2_intersection(s1, s2, res);
	EXPECT_EQ(code, 0u);
}

TEST(TEST_cgmath_intersections, LineEllipse_TwoPoints)
{
	// Horizontal line through the center of the unit circle: hits (-1,0) and (1,0).
	float ls[2] = {-2, 0}, le[2] = {2, 0}, c[2] = {0, 0}, r[2] = {1, 1};
	float res1[2], res2[2];
	unsigned int n = line_ellipse_intersection(ls, le, c, r, res1, res2);
	EXPECT_EQ(n, 2u);
	EXPECT_NEAR(res1[0], -1.f, 1e-5f);
	EXPECT_NEAR(res1[1], 0.f, 1e-5f);
	EXPECT_NEAR(res2[0], 1.f, 1e-5f);
	EXPECT_NEAR(res2[1], 0.f, 1e-5f);
}
