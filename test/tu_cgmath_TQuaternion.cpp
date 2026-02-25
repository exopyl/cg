#include <gtest/gtest.h>
#include <math.h>

#include "../src/cgmath/TQuaternion.h"

TEST(TEST_cgmath_TQuaternion, DefaultConstructor)
{
	// context & action
	Quaternionf q;

	// expectations
	EXPECT_FLOAT_EQ(q.x, 0.f);
	EXPECT_FLOAT_EQ(q.y, 0.f);
	EXPECT_FLOAT_EQ(q.z, 0.f);
	EXPECT_FLOAT_EQ(q.w, 0.f);
}

TEST(TEST_cgmath_TQuaternion, ValueConstructor)
{
	// context & action
	Quaternionf q(1.f, 2.f, 3.f, 4.f);

	// expectations
	EXPECT_FLOAT_EQ(q.x, 1.f);
	EXPECT_FLOAT_EQ(q.y, 2.f);
	EXPECT_FLOAT_EQ(q.z, 3.f);
	EXPECT_FLOAT_EQ(q.w, 4.f);
}

TEST(TEST_cgmath_TQuaternion, CopyConstructor)
{
	// context
	Quaternionf q1(1.f, 2.f, 3.f, 4.f);

	// action
	Quaternionf q2(q1);

	// expectations
	EXPECT_FLOAT_EQ(q2.x, 1.f);
	EXPECT_FLOAT_EQ(q2.y, 2.f);
	EXPECT_FLOAT_EQ(q2.z, 3.f);
	EXPECT_FLOAT_EQ(q2.w, 4.f);
}

TEST(TEST_cgmath_TQuaternion, AxisAngleConstructor)
{
	// context - rotation of PI/2 around Z axis
	TVector3<float> axis(0.f, 0.f, 1.f);
	float angle = (float)(M_PI / 2.0);

	// action
	Quaternionf q(axis, angle);

	// expectations
	float s = sinf(angle / 2.f);
	float c = cosf(angle / 2.f);
	EXPECT_NEAR(q.x, 0.f, 1e-5f);
	EXPECT_NEAR(q.y, 0.f, 1e-5f);
	EXPECT_NEAR(q.z, s, 1e-5f);
	EXPECT_NEAR(q.w, c, 1e-5f);
}

TEST(TEST_cgmath_TQuaternion, Set)
{
	// context
	Quaternionf q;

	// action
	q.Set(5.f, 6.f, 7.f, 8.f);

	// expectations
	EXPECT_FLOAT_EQ(q.x, 5.f);
	EXPECT_FLOAT_EQ(q.y, 6.f);
	EXPECT_FLOAT_EQ(q.z, 7.f);
	EXPECT_FLOAT_EQ(q.w, 8.f);
}

TEST(TEST_cgmath_TQuaternion, Assignment)
{
	// context
	Quaternionf q1(1.f, 2.f, 3.f, 4.f);
	Quaternionf q2;

	// action
	q2 = q1;

	// expectations
	EXPECT_FLOAT_EQ(q2.x, 1.f);
	EXPECT_FLOAT_EQ(q2.y, 2.f);
	EXPECT_FLOAT_EQ(q2.z, 3.f);
	EXPECT_FLOAT_EQ(q2.w, 4.f);
}

TEST(TEST_cgmath_TQuaternion, Addition)
{
	// context
	Quaternionf q1(1.f, 2.f, 3.f, 4.f);
	Quaternionf q2(5.f, 6.f, 7.f, 8.f);

	// action
	Quaternionf r = q1 + q2;

	// expectations
	EXPECT_FLOAT_EQ(r.x, 6.f);
	EXPECT_FLOAT_EQ(r.y, 8.f);
	EXPECT_FLOAT_EQ(r.z, 10.f);
	EXPECT_FLOAT_EQ(r.w, 12.f);
}

TEST(TEST_cgmath_TQuaternion, AdditionAssign)
{
	// context
	Quaternionf q1(1.f, 2.f, 3.f, 4.f);
	Quaternionf q2(5.f, 6.f, 7.f, 8.f);

	// action
	q1 += q2;

	// expectations
	EXPECT_FLOAT_EQ(q1.x, 6.f);
	EXPECT_FLOAT_EQ(q1.y, 8.f);
	EXPECT_FLOAT_EQ(q1.z, 10.f);
	EXPECT_FLOAT_EQ(q1.w, 12.f);
}

TEST(TEST_cgmath_TQuaternion, Subtraction)
{
	// context
	Quaternionf q1(5.f, 6.f, 7.f, 8.f);
	Quaternionf q2(1.f, 2.f, 3.f, 4.f);

	// action
	Quaternionf r = q1 - q2;

	// expectations
	EXPECT_FLOAT_EQ(r.x, 4.f);
	EXPECT_FLOAT_EQ(r.y, 4.f);
	EXPECT_FLOAT_EQ(r.z, 4.f);
	EXPECT_FLOAT_EQ(r.w, 4.f);
}

TEST(TEST_cgmath_TQuaternion, SubtractionAssign)
{
	// context
	Quaternionf q1(5.f, 6.f, 7.f, 8.f);
	Quaternionf q2(1.f, 2.f, 3.f, 4.f);

	// action
	q1 -= q2;

	// expectations
	EXPECT_FLOAT_EQ(q1.x, 4.f);
	EXPECT_FLOAT_EQ(q1.y, 4.f);
	EXPECT_FLOAT_EQ(q1.z, 4.f);
	EXPECT_FLOAT_EQ(q1.w, 4.f);
}

TEST(TEST_cgmath_TQuaternion, ScalarMultiplication)
{
	// context
	Quaternionf q(1.f, 2.f, 3.f, 4.f);

	// action
	Quaternionf r = q * 2.f;

	// expectations
	EXPECT_FLOAT_EQ(r.x, 2.f);
	EXPECT_FLOAT_EQ(r.y, 4.f);
	EXPECT_FLOAT_EQ(r.z, 6.f);
	EXPECT_FLOAT_EQ(r.w, 8.f);
}

TEST(TEST_cgmath_TQuaternion, ScalarDivision)
{
	// context
	Quaternionf q(2.f, 4.f, 6.f, 8.f);

	// action
	Quaternionf r = q / 2.f;

	// expectations
	EXPECT_FLOAT_EQ(r.x, 1.f);
	EXPECT_FLOAT_EQ(r.y, 2.f);
	EXPECT_FLOAT_EQ(r.z, 3.f);
	EXPECT_FLOAT_EQ(r.w, 4.f);
}

TEST(TEST_cgmath_TQuaternion, Equality)
{
	// context
	Quaternionf q1(1.f, 2.f, 3.f, 4.f);
	Quaternionf q2(1.f, 2.f, 3.f, 4.f);
	Quaternionf q3(5.f, 6.f, 7.f, 8.f);

	// expectations
	EXPECT_TRUE(q1 == q2);
	EXPECT_TRUE(q1 != q3);
}

TEST(TEST_cgmath_TQuaternion, IndexOperator)
{
	// context
	Quaternionf q(1.f, 2.f, 3.f, 4.f);

	// expectations
	EXPECT_FLOAT_EQ(q[0], 1.f);
	EXPECT_FLOAT_EQ(q[1], 2.f);
	EXPECT_FLOAT_EQ(q[2], 3.f);
	EXPECT_FLOAT_EQ(q[3], 4.f);
}

TEST(TEST_cgmath_TQuaternion, Length)
{
	// context
	Quaternionf q(1.f, 0.f, 0.f, 0.f);

	// action
	float len = q.Length();

	// expectations
	EXPECT_FLOAT_EQ(len, 1.f);
}

TEST(TEST_cgmath_TQuaternion, LengthGeneral)
{
	// context
	Quaternionf q(1.f, 2.f, 3.f, 4.f);

	// action
	float len = q.Length();

	// expectations - sqrt(1+4+9+16) = sqrt(30)
	EXPECT_FLOAT_EQ(len, sqrtf(30.f));
}

TEST(TEST_cgmath_TQuaternion, Normalize)
{
	// context
	Quaternionf q(2.f, 0.f, 0.f, 0.f);

	// action
	q.Normalize();

	// expectations
	EXPECT_NEAR(q.Length(), 1.f, 1e-5f);
	EXPECT_FLOAT_EQ(q.x, 1.f);
}

TEST(TEST_cgmath_TQuaternion, Conjugate)
{
	// context
	Quaternionf q(1.f, 2.f, 3.f, 4.f);

	// action
	q.Conjugate();

	// expectations
	EXPECT_FLOAT_EQ(q.x, -1.f);
	EXPECT_FLOAT_EQ(q.y, -2.f);
	EXPECT_FLOAT_EQ(q.z, -3.f);
	EXPECT_FLOAT_EQ(q.w, 4.f);
}

TEST(TEST_cgmath_TQuaternion, GetConjugate)
{
	// context
	Quaternionf q(1.f, 2.f, 3.f, 4.f);

	// action
	Quaternionf conj = q.GetConjugate();

	// expectations - original unchanged
	EXPECT_FLOAT_EQ(q.x, 1.f);
	EXPECT_FLOAT_EQ(q.y, 2.f);
	// conjugate negates imaginary part
	EXPECT_FLOAT_EQ(conj.x, -1.f);
	EXPECT_FLOAT_EQ(conj.y, -2.f);
	EXPECT_FLOAT_EQ(conj.z, -3.f);
	EXPECT_FLOAT_EQ(conj.w, 4.f);
}

TEST(TEST_cgmath_TQuaternion, GetInverse)
{
	// context - unit quaternion: inverse == conjugate
	TVector3<float> axis(0.f, 0.f, 1.f);
	Quaternionf q(axis, (float)(M_PI / 4.0));

	// action
	Quaternionf inv = q.GetInverse();

	// expectations - q * q^-1 should be identity (w=1)
	Quaternionf product = q * inv;
	EXPECT_NEAR(product.w, 1.f, 1e-4f);
	EXPECT_NEAR(product.x, 0.f, 1e-4f);
	EXPECT_NEAR(product.y, 0.f, 1e-4f);
	EXPECT_NEAR(product.z, 0.f, 1e-4f);
}

TEST(TEST_cgmath_TQuaternion, QuaternionMultiplication)
{
	// context - two 90 degree rotations around Z = 180 degrees around Z
	TVector3<float> axis(0.f, 0.f, 1.f);
	Quaternionf q(axis, (float)(M_PI / 2.0));

	// action
	Quaternionf r = q * q;

	// expectations - equivalent to 180 degree rotation around Z
	// cos(pi/2) = 0, sin(pi/2) = 1 => w ~= 0, z ~= 1
	EXPECT_NEAR(r.w, 0.f, 1e-5f);
	EXPECT_NEAR(r.x, 0.f, 1e-5f);
	EXPECT_NEAR(r.y, 0.f, 1e-5f);
	EXPECT_NEAR(fabsf(r.z), 1.f, 1e-5f);
}

TEST(TEST_cgmath_TQuaternion, Rotate)
{
	// context - rotation of PI/2 around Z axis applied to (1, 0, 0)
	TVector3<float> axis(0.f, 0.f, 1.f);
	Quaternionf q(axis, (float)(M_PI / 2.0));
	TVector3<float> v(1.f, 0.f, 0.f);

	// action
	TVector3<float> res;
	q.rotate(res, v);

	// expectations - should give (0, 1, 0)
	EXPECT_NEAR(res.x, 0.f, 1e-4f);
	EXPECT_NEAR(res.y, 1.f, 1e-4f);
	EXPECT_NEAR(res.z, 0.f, 1e-4f);
}

TEST(TEST_cgmath_TQuaternion, RotateIdentity)
{
	// context - identity quaternion (0, 0, 0, 1)
	Quaternionf q(0.f, 0.f, 0.f, 1.f);
	TVector3<float> v(3.f, 4.f, 5.f);

	// action
	TVector3<float> res;
	q.rotate(res, v);

	// expectations - should be unchanged
	EXPECT_NEAR(res.x, 3.f, 1e-4f);
	EXPECT_NEAR(res.y, 4.f, 1e-4f);
	EXPECT_NEAR(res.z, 5.f, 1e-4f);
}

TEST(TEST_cgmath_TQuaternion, GetMatrixRotationIdentity)
{
	// context - identity quaternion
	Quaternionf q(0.f, 0.f, 0.f, 1.f);

	// action
	float m[16];
	q.get_matrix_rotation(m);

	// expectations - identity matrix
	EXPECT_NEAR(m[0], 1.f, 1e-5f);
	EXPECT_NEAR(m[5], 1.f, 1e-5f);
	EXPECT_NEAR(m[10], 1.f, 1e-5f);
	EXPECT_NEAR(m[15], 1.f, 1e-5f);
	EXPECT_NEAR(m[1], 0.f, 1e-5f);
	EXPECT_NEAR(m[2], 0.f, 1e-5f);
	EXPECT_NEAR(m[3], 0.f, 1e-5f);
	EXPECT_NEAR(m[4], 0.f, 1e-5f);
	EXPECT_NEAR(m[6], 0.f, 1e-5f);
	EXPECT_NEAR(m[7], 0.f, 1e-5f);
	EXPECT_NEAR(m[8], 0.f, 1e-5f);
	EXPECT_NEAR(m[9], 0.f, 1e-5f);
	EXPECT_NEAR(m[11], 0.f, 1e-5f);
	EXPECT_NEAR(m[12], 0.f, 1e-5f);
	EXPECT_NEAR(m[13], 0.f, 1e-5f);
	EXPECT_NEAR(m[14], 0.f, 1e-5f);
}

TEST(TEST_cgmath_TQuaternion, GetParameters)
{
	// context
	Quaternionf q(1.f, 2.f, 3.f, 4.f);

	// action
	float x, y, z, w;
	q.get_parameters(&x, &y, &z, &w);

	// expectations
	EXPECT_FLOAT_EQ(x, 1.f);
	EXPECT_FLOAT_EQ(y, 2.f);
	EXPECT_FLOAT_EQ(z, 3.f);
	EXPECT_FLOAT_EQ(w, 4.f);
}

TEST(TEST_cgmath_TQuaternion, Slerp)
{
	// context - slerp between two rotations around Z
	TVector3<float> axis(0.f, 0.f, 1.f);
	Quaternionf p(axis, 0.f);
	Quaternionf q(axis, (float)(M_PI / 2.0));

	// action - t=0 should give p
	Quaternionf res;
	res.slerp(&p, &q, 0.f);

	// expectations
	EXPECT_NEAR(res.x, p.x, 1e-4f);
	EXPECT_NEAR(res.y, p.y, 1e-4f);
	EXPECT_NEAR(res.z, p.z, 1e-4f);
	EXPECT_NEAR(res.w, p.w, 1e-4f);
}

TEST(TEST_cgmath_TQuaternion, SlerpIdentical)
{
	// context
	Quaternionf p(0.f, 0.f, 0.f, 1.f);
	Quaternionf q(0.f, 0.f, 0.f, 1.f);

	// action
	Quaternionf res;
	res.slerp(&p, &q, 0.5f);

	// expectations
	EXPECT_NEAR(res.w, 1.f, 1e-4f);
	EXPECT_NEAR(res.x, 0.f, 1e-4f);
	EXPECT_NEAR(res.y, 0.f, 1e-4f);
	EXPECT_NEAR(res.z, 0.f, 1e-4f);
}

TEST(TEST_cgmath_TQuaternion, DoubleType)
{
	// context & action - verify double instantiation works
	Quaterniond q(1.0, 2.0, 3.0, 4.0);

	// expectations
	EXPECT_DOUBLE_EQ(q.x, 1.0);
	EXPECT_DOUBLE_EQ(q.y, 2.0);
	EXPECT_DOUBLE_EQ(q.z, 3.0);
	EXPECT_DOUBLE_EQ(q.w, 4.0);
}
