#include <gtest/gtest.h>
#include <math.h>

#include "../src/cgmath/algebra_quaternion.h"

TEST(TEST_cgmath_algebra_quaternion, Init)
{
	// context & action
	quaternion q;
	quaternion_init(q, 1.f, 0.f, 0.f, 0.f);

	// expectations - q = (x=0, y=0, z=0, w=1)
	EXPECT_FLOAT_EQ(q[0], 0.f);
	EXPECT_FLOAT_EQ(q[1], 0.f);
	EXPECT_FLOAT_EQ(q[2], 0.f);
	EXPECT_FLOAT_EQ(q[3], 1.f);
}

TEST(TEST_cgmath_algebra_quaternion, InitValues)
{
	// context & action
	quaternion q;
	quaternion_init(q, 4.f, 1.f, 2.f, 3.f);

	// expectations - x=1, y=2, z=3, w=4
	EXPECT_FLOAT_EQ(q[0], 1.f);
	EXPECT_FLOAT_EQ(q[1], 2.f);
	EXPECT_FLOAT_EQ(q[2], 3.f);
	EXPECT_FLOAT_EQ(q[3], 4.f);
}

TEST(TEST_cgmath_algebra_quaternion, Copy)
{
	// context
	quaternion src;
	quaternion_init(src, 4.f, 1.f, 2.f, 3.f);

	// action
	quaternion dst;
	quaternion_copy(dst, src);

	// expectations
	EXPECT_FLOAT_EQ(dst[0], 1.f);
	EXPECT_FLOAT_EQ(dst[1], 2.f);
	EXPECT_FLOAT_EQ(dst[2], 3.f);
	EXPECT_FLOAT_EQ(dst[3], 4.f);
}

TEST(TEST_cgmath_algebra_quaternion, NormUnit)
{
	// context - unit quaternion (0,0,0,1)
	quaternion q;
	quaternion_init(q, 1.f, 0.f, 0.f, 0.f);

	// action
	float norm = quaternion_norm(q);

	// expectations
	EXPECT_FLOAT_EQ(norm, 1.f);
}

TEST(TEST_cgmath_algebra_quaternion, NormGeneral)
{
	// context
	quaternion q;
	quaternion_init(q, 0.f, 1.f, 2.f, 3.f);

	// action
	float norm = quaternion_norm(q);

	// expectations - sqrt(1+4+9+0) = sqrt(14)
	EXPECT_FLOAT_EQ(norm, sqrtf(14.f));
}

TEST(TEST_cgmath_algebra_quaternion, Normalize)
{
	// context
	quaternion q;
	quaternion_init(q, 0.f, 2.f, 0.f, 0.f);

	// action
	int res = quaternion_normalize(q);

	// expectations
	EXPECT_EQ(res, 1);
	EXPECT_FLOAT_EQ(quaternion_norm(q), 1.f);
}

TEST(TEST_cgmath_algebra_quaternion, NormalizeZero)
{
	// context
	quaternion q;
	quaternion_init(q, 0.f, 0.f, 0.f, 0.f);

	// action
	int res = quaternion_normalize(q);

	// expectations
	EXPECT_EQ(res, 0);
}

TEST(TEST_cgmath_algebra_quaternion, InitAxisAngle)
{
	// context - rotation of PI/2 around Z axis
	quaternion q;
	vec3 axis = {0.f, 0.f, 1.f};
	float angle = (float)(M_PI / 2.0);

	// action
	quaternion_init_axis_angle(q, axis, angle);

	// expectations - q = (0, 0, sin(pi/4), cos(pi/4))
	float s = sinf(angle / 2.f);
	float c = cosf(angle / 2.f);
	EXPECT_NEAR(q[0], 0.f, 1e-5f);
	EXPECT_NEAR(q[1], 0.f, 1e-5f);
	EXPECT_NEAR(q[2], s, 1e-5f);
	EXPECT_NEAR(q[3], c, 1e-5f);
}

TEST(TEST_cgmath_algebra_quaternion, InitAxisAngleNorm)
{
	// context - any axis-angle rotation should produce a unit quaternion
	quaternion q;
	vec3 axis = {1.f, 1.f, 1.f};
	float angle = 1.2f;

	// action
	quaternion_init_axis_angle(q, axis, angle);

	// expectations
	EXPECT_NEAR(quaternion_norm(q), 1.f, 1e-5f);
}

TEST(TEST_cgmath_algebra_quaternion, ConvertToMatrix3Identity)
{
	// context - identity quaternion (no rotation)
	quaternion q;
	quaternion_init(q, 1.f, 0.f, 0.f, 0.f);

	// action
	mat3 m;
	quaternion_convert_to_matrix3(q, m);

	// expectations - should be identity matrix
	EXPECT_NEAR(m[0][0], 1.f, 1e-5f);
	EXPECT_NEAR(m[1][1], 1.f, 1e-5f);
	EXPECT_NEAR(m[2][2], 1.f, 1e-5f);
	EXPECT_NEAR(m[0][1], 0.f, 1e-5f);
	EXPECT_NEAR(m[0][2], 0.f, 1e-5f);
	EXPECT_NEAR(m[1][0], 0.f, 1e-5f);
	EXPECT_NEAR(m[1][2], 0.f, 1e-5f);
	EXPECT_NEAR(m[2][0], 0.f, 1e-5f);
	EXPECT_NEAR(m[2][1], 0.f, 1e-5f);
}

TEST(TEST_cgmath_algebra_quaternion, InitMatrix3Roundtrip)
{
	// context - create quaternion from axis-angle, convert to matrix, back to quaternion
	quaternion q1;
	vec3 axis = {0.f, 0.f, 1.f};
	quaternion_init_axis_angle(q1, axis, (float)(M_PI / 4.0));

	// action
	mat3 m;
	quaternion_convert_to_matrix3(q1, m);
	quaternion q2;
	quaternion_init_matrix3(q2, m);

	// expectations - both quaternions should represent the same rotation
	EXPECT_NEAR(fabsf(q1[0]), fabsf(q2[0]), 1e-4f);
	EXPECT_NEAR(fabsf(q1[1]), fabsf(q2[1]), 1e-4f);
	EXPECT_NEAR(fabsf(q1[2]), fabsf(q2[2]), 1e-4f);
	EXPECT_NEAR(fabsf(q1[3]), fabsf(q2[3]), 1e-4f);
}

TEST(TEST_cgmath_algebra_quaternion, RotateVector)
{
	// context - rotation of PI/2 around Z axis applied to (1, 0, 0)
	quaternion q;
	vec3 axis = {0.f, 0.f, 1.f};
	quaternion_init_axis_angle(q, axis, (float)(M_PI / 2.0));
	vec3 v = {1.f, 0.f, 0.f};

	// action
	vec3 res;
	quaternion_rotate(q, res, v);

	// expectations - should give (0, 1, 0)
	EXPECT_NEAR(res[0], 0.f, 1e-5f);
	EXPECT_NEAR(res[1], 1.f, 1e-5f);
	EXPECT_NEAR(res[2], 0.f, 1e-5f);
}

TEST(TEST_cgmath_algebra_quaternion, RotateVectorIdentity)
{
	// context - identity quaternion should not change vector
	quaternion q;
	quaternion_init(q, 1.f, 0.f, 0.f, 0.f);
	vec3 v = {3.f, 4.f, 5.f};

	// action
	vec3 res;
	quaternion_rotate(q, res, v);

	// expectations
	EXPECT_NEAR(res[0], 3.f, 1e-5f);
	EXPECT_NEAR(res[1], 4.f, 1e-5f);
	EXPECT_NEAR(res[2], 5.f, 1e-5f);
}

TEST(TEST_cgmath_algebra_quaternion, SlerpIdentical)
{
	// context - slerp between identical quaternions
	quaternion p, q, res;
	quaternion_init(p, 1.f, 0.f, 0.f, 0.f);
	quaternion_init(q, 1.f, 0.f, 0.f, 0.f);

	// action
	quaternion_init_slerp(res, p, q, 0.5f);

	// expectations
	EXPECT_NEAR(res[0], 0.f, 1e-5f);
	EXPECT_NEAR(res[1], 0.f, 1e-5f);
	EXPECT_NEAR(res[2], 0.f, 1e-5f);
	EXPECT_NEAR(res[3], 1.f, 1e-5f);
}

TEST(TEST_cgmath_algebra_quaternion, SlerpEndpoints)
{
	// context
	quaternion p, q;
	vec3 axisZ = {0.f, 0.f, 1.f};
	quaternion_init_axis_angle(p, axisZ, 0.f);
	quaternion_init_axis_angle(q, axisZ, (float)(M_PI / 2.0));

	// action - t=0 should give p
	quaternion res0;
	quaternion_init_slerp(res0, p, q, 0.f);

	// expectations
	EXPECT_NEAR(res0[0], p[0], 1e-4f);
	EXPECT_NEAR(res0[1], p[1], 1e-4f);
	EXPECT_NEAR(res0[2], p[2], 1e-4f);
	EXPECT_NEAR(res0[3], p[3], 1e-4f);
}
