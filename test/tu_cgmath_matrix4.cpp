#include <gtest/gtest.h>

#include "../src/cgmath/common.h"
#include "../src/cgmath/TMatrix4.h"

//-----------------------------------------------------------------------------
// Constructeurs
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, default_constructor_is_identity)
{
	Matrix4 m;
	EXPECT_FLOAT_EQ(m.m_Mat[0][0], 1.f);
	EXPECT_FLOAT_EQ(m.m_Mat[1][1], 1.f);
	EXPECT_FLOAT_EQ(m.m_Mat[2][2], 1.f);
	EXPECT_FLOAT_EQ(m.m_Mat[3][3], 1.f);
	EXPECT_FLOAT_EQ(m.m_Mat[0][1], 0.f);
	EXPECT_FLOAT_EQ(m.m_Mat[1][0], 0.f);
	// diagonale = 1, reste = 0
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			EXPECT_FLOAT_EQ(m.m_Mat[i][j], (i == j) ? 1.f : 0.f);
}

TEST(TEST_cgmath_matrix4, constructor_16_values_row_column_order)
{
	// Ordre: row-major m_Mat[row][col]
	Matrix4 m(1, 2, 3, 4,
	          5, 6, 7, 8,
	          9, 10, 11, 12,
	          13, 14, 15, 16);
	EXPECT_FLOAT_EQ(m.m_Mat[0][0], 1.f);
	EXPECT_FLOAT_EQ(m.m_Mat[0][1], 2.f);
	EXPECT_FLOAT_EQ(m.m_Mat[0][2], 3.f);
	EXPECT_FLOAT_EQ(m.m_Mat[0][3], 4.f);
	EXPECT_FLOAT_EQ(m.m_Mat[1][0], 5.f);
	EXPECT_FLOAT_EQ(m.m_Mat[3][3], 16.f);
}

TEST(TEST_cgmath_matrix4, constructor_from_pointer)
{
	float data[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
	Matrix4 m(data);
	EXPECT_FLOAT_EQ(m.m_Mat[0][0], 1.f);
	EXPECT_FLOAT_EQ(m.m_Mat[0][1], 2.f);
	EXPECT_FLOAT_EQ(m.m_Mat[3][3], 16.f);
}

//-----------------------------------------------------------------------------
// Opérateurs == et !=
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, operator_equality)
{
	Matrix4 a(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
	Matrix4 b(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
	Matrix4 c(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2);
	EXPECT_TRUE(a == b);
	EXPECT_FALSE(a == c);
}

TEST(TEST_cgmath_matrix4, operator_inequality)
{
	Matrix4 a(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
	Matrix4 b(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2);
	EXPECT_TRUE(a != b);
	EXPECT_FALSE(a != a);
}

//-----------------------------------------------------------------------------
// operator= (copie et depuis tableau)
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, operator_assign_copy)
{
	Matrix4 a(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	Matrix4 b;
	b = a;
	EXPECT_TRUE(b == a);
}

TEST(TEST_cgmath_matrix4, operator_assign_from_array)
{
	float data[16] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 2, 3, 4, 5, 6, 7, 8};
	Matrix4 m;
	m = data;
	EXPECT_FLOAT_EQ(m.m_Mat[0][0], 9.f);
	EXPECT_FLOAT_EQ(m.m_Mat[0][3], 6.f);
	EXPECT_FLOAT_EQ(m.m_Mat[3][3], 8.f);
}

//-----------------------------------------------------------------------------
// Multiplication matrice * matrice
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, operator_multiply_matrix)
{
	Matrix4 identity;
	Matrix4 m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	Matrix4 res = identity * m;
	EXPECT_TRUE(res == m);
	res = m * identity;
	EXPECT_TRUE(res == m);
}

TEST(TEST_cgmath_matrix4, operator_multiply_matrix_non_identity)
{
	Matrix4 a(1, 0, 0, 1,
	          0, 1, 0, 0,
	          0, 0, 1, 0,
	          0, 0, 0, 1);
	Matrix4 b(2, 0, 0, 0,
	          0, 2, 0, 0,
	          0, 0, 2, 0,
	          0, 0, 0, 1);
	Matrix4 c = a * b;
	// (1,0,0,1) * (2,0,0,0) -> row0: 2,0,0,1
	EXPECT_FLOAT_EQ(c.m_Mat[0][0], 2.f);
	EXPECT_FLOAT_EQ(c.m_Mat[0][1], 0.f);
	EXPECT_FLOAT_EQ(c.m_Mat[0][3], 1.f);
	EXPECT_FLOAT_EQ(c.m_Mat[1][1], 2.f);
}

TEST(TEST_cgmath_matrix4, operator_multiply_assign)
{
	Matrix4 m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	Matrix4 m2 = m;
	Matrix4 identity;
	m2 *= identity;
	EXPECT_TRUE(m2 == m);
}

//-----------------------------------------------------------------------------
// SetIdentity
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, SetIdentity)
{
	Matrix4 m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	m.SetIdentity();
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			EXPECT_FLOAT_EQ(m.m_Mat[i][j], (i == j) ? 1.f : 0.f);
}

//-----------------------------------------------------------------------------
// Transpose
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, Transpose)
{
	Matrix4 m(1, 2, 3, 4,
	          5, 6, 7, 8,
	          9, 10, 11, 12,
	          13, 14, 15, 16);
	m.Transpose();
	Matrix4 mExpected(1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16);
	EXPECT_TRUE(m == mExpected);
}

//-----------------------------------------------------------------------------
// Déterminant
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, Determinant_identity)
{
	Matrix4 identity;
	EXPECT_FLOAT_EQ(identity.Determinant(), 1.f);
}

TEST(TEST_cgmath_matrix4, Determinant_diagonal)
{
	Matrix4 m(2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 0, 0, 0, 5);
	EXPECT_FLOAT_EQ(m.Determinant(), 2.f * 3.f * 4.f * 5.f);
}

//-----------------------------------------------------------------------------
// Inverse
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, GetInverse_identity)
{
	Matrix4 identity;
	Matrix4 inv;
	EXPECT_TRUE(identity.GetInverse(inv));
	EXPECT_TRUE(inv == identity);
}

TEST(TEST_cgmath_matrix4, GetInverse_then_product_is_identity)
{
	Matrix4 m(1, 2, 3, 4,
	          3, 2, 5, 3,
	          1, 4, 2, 6,
	          4, 2, 1, 4);
	Matrix4 inv;
	ASSERT_TRUE(m.GetInverse(inv));
	Matrix4 product = m * inv;
	// M * M^-1 ≈ identity
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			EXPECT_NEAR(product.m_Mat[i][j], (i == j) ? 1.f : 0.f, 1e-5f);
}

TEST(TEST_cgmath_matrix4, SetInverse)
{
	Matrix4 m(1, 2, 3, 4, 3, 2, 5, 3, 1, 4, 2, 6, 4, 2, 1, 4);
	Matrix4 mCopy = m;
	EXPECT_TRUE(m.SetInverse());
	Matrix4 product = mCopy * m;
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			EXPECT_NEAR(product.m_Mat[i][j], (i == j) ? 1.f : 0.f, 1e-5f);
}

//-----------------------------------------------------------------------------
// Matrice * vecteur (Vector3, Vector4)
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, matrix_times_vector3)
{
	Matrix4 identity;
	Vector3 v(1, 2, 3);
	Vector3 res = identity * v;
	EXPECT_FLOAT_EQ(res.x, 1.f);
	EXPECT_FLOAT_EQ(res.y, 2.f);
	EXPECT_FLOAT_EQ(res.z, 3.f);
}

TEST(TEST_cgmath_matrix4, matrix_times_vector3_translation)
{
	// translation (1, 0, 0) en dernière colonne
	Matrix4 T(1, 0, 0, 10,
	          0, 1, 0, 0,
	          0, 0, 1, 0,
	          0, 0, 0, 1);
	Vector3 v(0, 0, 0);
	Vector3 res = T * v;
	EXPECT_FLOAT_EQ(res.x, 10.f);
	EXPECT_FLOAT_EQ(res.y, 0.f);
	EXPECT_FLOAT_EQ(res.z, 0.f);
}

TEST(TEST_cgmath_matrix4, vector3_times_matrix)
{
	Matrix4 identity;
	Vector3 v(1, 2, 3);
	Vector3 res = v * identity;
	EXPECT_FLOAT_EQ(res.x, 1.f);
	EXPECT_FLOAT_EQ(res.y, 2.f);
	EXPECT_FLOAT_EQ(res.z, 3.f);
}

TEST(TEST_cgmath_matrix4, matrix_times_vector4)
{
	Matrix4 identity;
	Vector4 v(1, 2, 3, 1);
	Vector4 res = identity * v;
	EXPECT_FLOAT_EQ(res.x, 1.f);
	EXPECT_FLOAT_EQ(res.y, 2.f);
	EXPECT_FLOAT_EQ(res.z, 3.f);
	EXPECT_FLOAT_EQ(res.w, 1.f);
}

TEST(TEST_cgmath_matrix4, vector4_times_matrix)
{
	Matrix4 identity;
	Vector4 v(1, 2, 3, 1);
	Vector4 res = v * identity;
	EXPECT_FLOAT_EQ(res.x, 1.f);
	EXPECT_FLOAT_EQ(res.y, 2.f);
	EXPECT_FLOAT_EQ(res.z, 3.f);
	EXPECT_FLOAT_EQ(res.w, 1.f);
}

TEST(TEST_cgmath_matrix4, operator_assign_vector3_in_place)
{
	Matrix4 identity;
	Vector3 v(1, 2, 3);
	v *= identity;
	EXPECT_FLOAT_EQ(v.x, 1.f);
	EXPECT_FLOAT_EQ(v.y, 2.f);
	EXPECT_FLOAT_EQ(v.z, 3.f);
}

TEST(TEST_cgmath_matrix4, operator_assign_vector4_in_place)
{
	Matrix4 identity;
	Vector4 v(1, 2, 3, 1);
	v *= identity;
	EXPECT_FLOAT_EQ(v.x, 1.f);
	EXPECT_FLOAT_EQ(v.y, 2.f);
	EXPECT_FLOAT_EQ(v.z, 3.f);
	EXPECT_FLOAT_EQ(v.w, 1.f);
}

//-----------------------------------------------------------------------------
// GetMatPtr et operator TValue*
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, GetMatPtr)
{
	Matrix4 m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	float* p = m.GetMatPtr();
	EXPECT_EQ(p, &m.m_Mat[0][0]);
	EXPECT_FLOAT_EQ(p[0], 1.f);
	EXPECT_FLOAT_EQ(p[15], 16.f);
}

TEST(TEST_cgmath_matrix4, operator_pointer)
{
	Matrix4 m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	float* p = m;
	EXPECT_FLOAT_EQ(p[0], 1.f);
	EXPECT_FLOAT_EQ(p[15], 16.f);
}

//-----------------------------------------------------------------------------
// SetOrtho
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, SetOrtho)
{
	Matrix4 m;
	m.SetOrtho(-1, 1, -1, 1, 0, 1);
	// structure typique: m[0][0] = 2/(r-l), m[3][0] = -(r+l)/(r-l)
	EXPECT_FLOAT_EQ(m.m_Mat[0][0], 1.f);
	EXPECT_FLOAT_EQ(m.m_Mat[1][1], 1.f);
	EXPECT_FLOAT_EQ(m.m_Mat[2][2], -2.f);  // -2/(f-n) avec n=0, f=1
	EXPECT_FLOAT_EQ(m.m_Mat[3][3], 1.f);
	EXPECT_FLOAT_EQ(m.m_Mat[3][0], 0.f);
	EXPECT_FLOAT_EQ(m.m_Mat[3][1], 0.f);
	EXPECT_FLOAT_EQ(m.m_Mat[3][2], -1.f);  // -(f+n)/(f-n)
}

//-----------------------------------------------------------------------------
// SetLookAt (smoke test)
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, SetLookAt)
{
	Matrix4 m;
	Vector3 eye(0, 0, 5);
	Vector3 center(0, 0, 0);
	Vector3 up(0, 1, 0);
	m.SetLookAt(eye, center, up);
	// Vue -Z: dernière ligne de rotation doit donner une direction cohérente
	// On vérifie juste que la matrice n'est pas nulle et que m[3][3]=1
	EXPECT_FLOAT_EQ(m.m_Mat[3][3], 1.f);
	EXPECT_FLOAT_EQ(m.m_Mat[0][3], 0.f);
	EXPECT_FLOAT_EQ(m.m_Mat[1][3], 0.f);
	EXPECT_FLOAT_EQ(m.m_Mat[2][3], 0.f);
}

//-----------------------------------------------------------------------------
// SetPerspective / frustrum (smoke test)
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, SetPerspective)
{
	Matrix4 m;
	m.SetPerspective(45.f, 1.333f, 0.1f, 100.f);
	// Matrice de projection: m[2][3] = -1, m[3][3] = 0
	EXPECT_FLOAT_EQ(m.m_Mat[2][3], -1.f);
	EXPECT_FLOAT_EQ(m.m_Mat[3][3], 0.f);
	EXPECT_FLOAT_EQ(m.m_Mat[0][1], 0.f);
	EXPECT_FLOAT_EQ(m.m_Mat[1][0], 0.f);
}
