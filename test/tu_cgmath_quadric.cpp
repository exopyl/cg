#include <gtest/gtest.h>
#include <math.h>
#include <string.h>

#include "../src/cgmath/quadric.h"

TEST(TEST_cgmath_quadric, Zero)
{
	// context & action
	quadric_t q;
	quadric_zero(q);

	// expectations
	for (int i = 0; i < 10; i++)
		EXPECT_DOUBLE_EQ(q[i], 0.0);
}

TEST(TEST_cgmath_quadric, Copy)
{
	// context
	quadric_t src;
	for (int i = 0; i < 10; i++)
		src[i] = (double)(i + 1);

	// action
	quadric_t dst;
	quadric_copy(dst, src);

	// expectations
	for (int i = 0; i < 10; i++)
		EXPECT_DOUBLE_EQ(dst[i], src[i]);
}

TEST(TEST_cgmath_quadric, Add)
{
	// context
	quadric_t q1, q2, qr;
	for (int i = 0; i < 10; i++)
	{
		q1[i] = (double)i;
		q2[i] = (double)(i * 2);
	}

	// action
	quadric_add(qr, q1, q2);

	// expectations
	for (int i = 0; i < 10; i++)
		EXPECT_DOUBLE_EQ(qr[i], (double)(i * 3));
}

TEST(TEST_cgmath_quadric, Scale)
{
	// context
	quadric_t q, qr;
	for (int i = 0; i < 10; i++)
		q[i] = (double)(i + 1);

	// action
	quadric_scale(qr, q, 2.f);

	// expectations
	for (int i = 0; i < 10; i++)
		EXPECT_DOUBLE_EQ(qr[i], (double)((i + 1) * 2));
}

TEST(TEST_cgmath_quadric, ScaleZero)
{
	// context
	quadric_t q, qr;
	for (int i = 0; i < 10; i++)
		q[i] = (double)(i + 1);

	// action
	quadric_scale(qr, q, 0.f);

	// expectations
	for (int i = 0; i < 10; i++)
		EXPECT_DOUBLE_EQ(qr[i], 0.0);
}

TEST(TEST_cgmath_quadric, PlaneQuadric)
{
	// context - plane equation (1, 0, 0, 0) => normal along x
	vec4 plane = {1.f, 0.f, 0.f, 0.f};

	// action
	quadric_t q;
	plane_quadric(plane, q);

	// expectations - only q[0] should be 1, rest 0
	EXPECT_DOUBLE_EQ(q[0], 1.0);
	EXPECT_DOUBLE_EQ(q[1], 0.0);
	EXPECT_DOUBLE_EQ(q[2], 0.0);
	EXPECT_DOUBLE_EQ(q[3], 0.0);
}

TEST(TEST_cgmath_quadric, PlaneQuadricGeneral)
{
	// context - plane (1, 2, 3, 4)
	vec4 plane = {1.f, 2.f, 3.f, 4.f};

	// action
	quadric_t q;
	plane_quadric(plane, q);

	// expectations
	EXPECT_DOUBLE_EQ(q[0], 1.0);   // 1*1
	EXPECT_DOUBLE_EQ(q[1], 4.0);   // 2*2
	EXPECT_DOUBLE_EQ(q[2], 9.0);   // 3*3
	EXPECT_DOUBLE_EQ(q[3], 16.0);  // 4*4
	EXPECT_DOUBLE_EQ(q[4], 2.0);   // 1*2
	EXPECT_DOUBLE_EQ(q[5], 6.0);   // 2*3
	EXPECT_DOUBLE_EQ(q[6], 12.0);  // 3*4
	EXPECT_DOUBLE_EQ(q[7], 3.0);   // 1*3
	EXPECT_DOUBLE_EQ(q[8], 8.0);   // 2*4
	EXPECT_DOUBLE_EQ(q[9], 4.0);   // 1*4
}

TEST(TEST_cgmath_quadric, EvalAtOrigin)
{
	// context - quadric from plane (0, 0, 1, -5) evaluated at origin
	vec4 plane = {0.f, 0.f, 1.f, -5.f};
	quadric_t q;
	plane_quadric(plane, q);
	vec3 v = {0.f, 0.f, 0.f};

	// action
	double val = quadric_eval(q, v);

	// expectations - should be d^2 = 25
	EXPECT_NEAR(val, 25.0, 1e-6);
}

TEST(TEST_cgmath_quadric, EvalOnPlane)
{
	// context - quadric from plane z=0 evaluated at point on plane
	vec4 plane = {0.f, 0.f, 1.f, 0.f};
	quadric_t q;
	plane_quadric(plane, q);
	vec3 v = {3.f, 4.f, 0.f};

	// action
	double val = quadric_eval(q, v);

	// expectations - point is on plane, distance should be 0
	EXPECT_NEAR(val, 0.0, 1e-6);
}

TEST(TEST_cgmath_quadric, MinimizeSingular)
{
	// context - zero quadric => singular matrix
	quadric_t q;
	quadric_zero(q);
	vec3 vnew;
	float error;

	// action
	int res = quadric_minimize(q, vnew, &error);

	// expectations
	EXPECT_EQ(res, -1);
}

TEST(TEST_cgmath_quadric, Minimize2Fallback)
{
	// context - two vertices, quadric from single plane
	vec4 plane = {0.f, 0.f, 1.f, 0.f};
	quadric_t q;
	plane_quadric(plane, q);
	vec3 v0 = {0.f, 0.f, 1.f};
	vec3 v1 = {0.f, 0.f, -1.f};
	vec3 vnew;
	float error;

	// action
	int res = quadric_minimize2(q, vnew, &error, v0, v1);

	// expectations - should succeed
	EXPECT_EQ(res, 0);
	// the optimal point on the edge should be at z=0
	EXPECT_NEAR(vnew[2], 0.f, 0.1f);
}

TEST(TEST_cgmath_quadric, MinimizeEdge)
{
	// context - three orthogonal planes, minimize along edge
	quadric_t q1, q2, q3, qtotal, qtmp;
	vec4 p1 = {1.f, 0.f, 0.f, -1.f};
	vec4 p2 = {0.f, 1.f, 0.f, -2.f};
	vec4 p3 = {0.f, 0.f, 1.f, -3.f};
	plane_quadric(p1, q1);
	plane_quadric(p2, q2);
	plane_quadric(p3, q3);
	quadric_add(qtmp, q1, q2);
	quadric_add(qtotal, qtmp, q3);

	vec3 vnew;
	float error;

	// action
	int res = quadric_minimize(qtotal, vnew, &error);

	// expectations - optimal point is intersection (1, 2, 3)
	EXPECT_EQ(res, 0);
	EXPECT_NEAR(vnew[0], 1.f, 0.1f);
	EXPECT_NEAR(vnew[1], 2.f, 0.1f);
	EXPECT_NEAR(vnew[2], 3.f, 0.1f);
}
