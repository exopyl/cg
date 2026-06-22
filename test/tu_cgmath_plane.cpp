#include <cmath>

#include <gtest/gtest.h>

#include "../src/cgmath/cgmath.h"

//
// Plane construction & fitting. These tests previously did not exist, which is
// exactly why two bugs survived:
//   - plane.cpp:29 : typo in the normal cross-product (a wrong normal makes v2/v3
//     no longer satisfy the plane equation);
//   - plane.cpp:124 : the fitted distance was |centroid| instead of -(n.centroid).
//

// All three defining points must lie on the plane (signed distance == 0).
TEST(TEST_cgmath_plane, ThreePointsContainAllThree)
{
	Vector3f v1(0.f, 0.f, 0.f), v2(1.f, 2.f, 0.f), v3(2.f, 1.f, 3.f);
	Plane plane(v1, v2, v3);
	EXPECT_NEAR(plane.distance_point(v1), 0.f, 1e-4f);
	EXPECT_NEAR(plane.distance_point(v2), 0.f, 1e-4f);
	EXPECT_NEAR(plane.distance_point(v3), 0.f, 1e-4f);
}

// Least-squares fit of points lying on z = 2: the fitted plane must have a
// z-aligned normal and contain every input point.
TEST(TEST_cgmath_plane, FittingCoplanarPoints)
{
	Vector3f pts[4] = { Vector3f(0.f, 0.f, 2.f), Vector3f(1.f, 0.f, 2.f),
	                    Vector3f(0.f, 1.f, 2.f), Vector3f(1.f, 1.f, 2.f) };
	Plane plane;
	plane.fitting(pts, 4);

	Vector3f n;
	plane.get_normale(n);
	EXPECT_NEAR(std::fabs(n.z), 1.f, 1e-4f); // normal along z (sign is arbitrary)
	EXPECT_NEAR(n.x, 0.f, 1e-4f);
	EXPECT_NEAR(n.y, 0.f, 1e-4f);

	for (int i = 0; i < 4; ++i)
		EXPECT_NEAR(plane.distance_point(pts[i]), 0.f, 1e-4f);
}
