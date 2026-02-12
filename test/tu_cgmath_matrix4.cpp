#include <gtest/gtest.h>
#include <cmath>

#include "../src/cgmath/TMatrix4.h"

using Matrix4f = TMatrix4<float>;
using Matrix4fRow = TMatrix4<float, StorageOrder::RowMajor>;
using Matrix4fCol = TMatrix4<float, StorageOrder::ColumnMajor>;

//-----------------------------------------------------------------------------
// Constructeurs
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, default_constructor_is_identity)
{
	Matrix4f m;
	EXPECT_FLOAT_EQ(m.at(0, 0), 1.f);
	EXPECT_FLOAT_EQ(m.at(1, 1), 1.f);
	EXPECT_FLOAT_EQ(m.at(2, 2), 1.f);
	EXPECT_FLOAT_EQ(m.at(3, 3), 1.f);
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			EXPECT_FLOAT_EQ(m.at(i, j), (i == j) ? 1.f : 0.f);
}

TEST(TEST_cgmath_matrix4, constructor_16_values_logical_row_col_order)
{
	// Valeurs en ordre logique (row, col): m00 m01 m02 m03, m10 m11 ...
	Matrix4f m(1, 2, 3, 4,
	           5, 6, 7, 8,
	           9, 10, 11, 12,
	           13, 14, 15, 16);
	EXPECT_FLOAT_EQ(m.at(0, 0), 1.f);
	EXPECT_FLOAT_EQ(m.at(0, 1), 2.f);
	EXPECT_FLOAT_EQ(m.at(0, 2), 3.f);
	EXPECT_FLOAT_EQ(m.at(0, 3), 4.f);
	EXPECT_FLOAT_EQ(m.at(1, 0), 5.f);
	EXPECT_FLOAT_EQ(m.at(3, 3), 16.f);
}

TEST(TEST_cgmath_matrix4, constructor_from_pointer_column_major)
{
	// Données brutes en ColumnMajor (défaut): colonne0, colonne1, ...
	// col0 = (1,5,9,13), col1 = (2,6,10,14), ...
	float data[16] = {1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16};
	Matrix4fCol m(data);
	EXPECT_FLOAT_EQ(m.at(0, 0), 1.f);
	EXPECT_FLOAT_EQ(m.at(1, 0), 5.f);
	EXPECT_FLOAT_EQ(m.at(0, 1), 2.f);
	EXPECT_FLOAT_EQ(m.at(3, 3), 16.f);
}

TEST(TEST_cgmath_matrix4, constructor_from_pointer_row_major)
{
	// Données brutes en RowMajor: ligne0, ligne1, ...
	float data[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
	Matrix4fRow m(data);
	EXPECT_FLOAT_EQ(m.at(0, 0), 1.f);
	EXPECT_FLOAT_EQ(m.at(0, 1), 2.f);
	EXPECT_FLOAT_EQ(m.at(1, 0), 5.f);
	EXPECT_FLOAT_EQ(m.at(3, 3), 16.f);
}

//-----------------------------------------------------------------------------
// Opérateurs == et !=
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, operator_equality)
{
	Matrix4f a(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
	Matrix4f b(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
	Matrix4f c(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2);
	EXPECT_TRUE(a == b);
	EXPECT_FALSE(a == c);
}

TEST(TEST_cgmath_matrix4, operator_inequality)
{
	Matrix4f a(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
	Matrix4f b(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2);
	EXPECT_TRUE(a != b);
	EXPECT_FALSE(a != a);
}

//-----------------------------------------------------------------------------
// operator= (copie)
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, operator_assign_copy)
{
	Matrix4f a(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	Matrix4f b;
	b = a;
	EXPECT_TRUE(b == a);
}

//-----------------------------------------------------------------------------
// Multiplication matrice * matrice
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, operator_multiply_matrix)
{
	Matrix4f identity;
	Matrix4f m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	Matrix4f res = identity * m;
	EXPECT_TRUE(res == m);
	res = m * identity;
	EXPECT_TRUE(res == m);
}

TEST(TEST_cgmath_matrix4, operator_multiply_matrix_non_identity)
{
	Matrix4f a(1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
	Matrix4f b(2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1);
	Matrix4f c = a * b;
	EXPECT_FLOAT_EQ(c.at(0, 0), 2.f);
	EXPECT_FLOAT_EQ(c.at(0, 1), 0.f);
	EXPECT_FLOAT_EQ(c.at(0, 3), 1.f);
	EXPECT_FLOAT_EQ(c.at(1, 1), 2.f);
}

TEST(TEST_cgmath_matrix4, operator_multiply_assign)
{
	Matrix4f m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	Matrix4f m2 = m;
	Matrix4f identity;
	m2 *= identity;
	EXPECT_TRUE(m2 == m);
}

//-----------------------------------------------------------------------------
// SetIdentity
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, SetIdentity)
{
	Matrix4f m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	m.SetIdentity();
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			EXPECT_FLOAT_EQ(m.at(i, j), (i == j) ? 1.f : 0.f);
}

//-----------------------------------------------------------------------------
// Transpose / GetTransposed
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, Transpose)
{
	Matrix4f m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	m.Transpose();
	Matrix4f expected(1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16);
	EXPECT_TRUE(m == expected);
}

TEST(TEST_cgmath_matrix4, GetTransposed)
{
	Matrix4f m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	Matrix4f t = m.GetTransposed();
	EXPECT_FLOAT_EQ(t.at(0, 0), 1.f);
	EXPECT_FLOAT_EQ(t.at(0, 1), 5.f);
	EXPECT_FLOAT_EQ(t.at(1, 0), 2.f);
	EXPECT_FLOAT_EQ(t.at(3, 3), 16.f);
	// Original unchanged
	EXPECT_FLOAT_EQ(m.at(0, 1), 2.f);
}

//-----------------------------------------------------------------------------
// Déterminant
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, Determinant_identity)
{
	Matrix4f identity;
	EXPECT_FLOAT_EQ(identity.Determinant(), 1.f);
}

TEST(TEST_cgmath_matrix4, Determinant_diagonal)
{
	Matrix4f m(2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 0, 0, 0, 5);
	EXPECT_FLOAT_EQ(m.Determinant(), 2.f * 3.f * 4.f * 5.f);
}

//-----------------------------------------------------------------------------
// Inverse
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, GetInverse_identity)
{
	Matrix4f identity;
	Matrix4f inv;
	EXPECT_TRUE(identity.GetInverse(inv));
	EXPECT_TRUE(inv == identity);
}

TEST(TEST_cgmath_matrix4, GetInverse_then_product_is_identity)
{
	Matrix4f m(1, 2, 3, 4, 3, 2, 5, 3, 1, 4, 2, 6, 4, 2, 1, 4);
	Matrix4f inv;
	ASSERT_TRUE(m.GetInverse(inv));
	Matrix4f product = m * inv;
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			EXPECT_NEAR(product.at(i, j), (i == j) ? 1.f : 0.f, 1e-5f);
}

TEST(TEST_cgmath_matrix4, SetInverse)
{
	Matrix4f m(1, 2, 3, 4, 3, 2, 5, 3, 1, 4, 2, 6, 4, 2, 1, 4);
	Matrix4f mCopy = m;
	EXPECT_TRUE(m.SetInverse());
	Matrix4f product = mCopy * m;
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			EXPECT_NEAR(product.at(i, j), (i == j) ? 1.f : 0.f, 1e-5f);
}

//-----------------------------------------------------------------------------
// data() / GetMatPtr / operator TValue*
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, GetMatPtr)
{
	Matrix4f m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	float* p = m.GetMatPtr();
	EXPECT_EQ(p, m.data());
	EXPECT_FLOAT_EQ(p[0], m.at(0, 0));
}

TEST(TEST_cgmath_matrix4, data_and_operator_pointer)
{
	Matrix4f m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	const float* cp = m.data();
	float* p = m;
	EXPECT_EQ(cp, p);
	// Contenu cohérent avec at() (layout dépend de StorageOrder)
	EXPECT_FLOAT_EQ(m.at(0, 0), 1.f);
	EXPECT_FLOAT_EQ(m.at(3, 3), 16.f);
}

//-----------------------------------------------------------------------------
// RowMajor / ColumnMajor et toOrder()
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, storage_order_column_major_default)
{
	Matrix4fCol m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	// data() en column-major: col0 puis col1 puis col2 puis col3
	// at(0,0)=1 -> data[0], at(1,0)=5 -> data[1], at(0,1)=2 -> data[4]
	const float* d = m.data();
	EXPECT_FLOAT_EQ(d[0], 1.f);
	EXPECT_FLOAT_EQ(d[1], 5.f);
	EXPECT_FLOAT_EQ(d[4], 2.f);
}

TEST(TEST_cgmath_matrix4, storage_order_row_major)
{
	Matrix4fRow m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	// data() en row-major: row0 puis row1 ...
	const float* d = m.data();
	EXPECT_FLOAT_EQ(d[0], 1.f);
	EXPECT_FLOAT_EQ(d[1], 2.f);
	EXPECT_FLOAT_EQ(d[4], 5.f);
}

TEST(TEST_cgmath_matrix4, toOrder_preserves_logical_matrix)
{
	Matrix4fCol mCol(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	Matrix4fRow mRow = mCol.toOrder<StorageOrder::RowMajor>();
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			EXPECT_FLOAT_EQ(mCol.at(i, j), mRow.at(i, j));
}

//-----------------------------------------------------------------------------
// Rotations (radians)
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, SetRotateZ_90)
{
	Matrix4f m;
	const float halfPi = 3.14159265358979323846f / 2.f;
	m.SetRotateZ(halfPi);
	// (cos, -sin, 0, 0); (sin, cos, 0, 0); (0,0,1,0); (0,0,0,1)
	EXPECT_NEAR(m.at(0, 0), 0.f, 1e-5f);
	EXPECT_NEAR(m.at(0, 1), -1.f, 1e-5f);
	EXPECT_NEAR(m.at(1, 0), 1.f, 1e-5f);
	EXPECT_NEAR(m.at(1, 1), 0.f, 1e-5f);
	EXPECT_FLOAT_EQ(m.at(2, 2), 1.f);
	EXPECT_FLOAT_EQ(m.at(3, 3), 1.f);
}

TEST(TEST_cgmath_matrix4, SetRotateX_identity_angle_zero)
{
	Matrix4f m;
	m.SetRotateX(0.f);
	Matrix4f identity;
	EXPECT_TRUE(m == identity);
}

//-----------------------------------------------------------------------------
// SetOrtho
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, SetOrtho)
{
	Matrix4f m;
	m.SetOrtho(-1, 1, -1, 1, 0, 1);
	EXPECT_FLOAT_EQ(m.at(0, 0), 1.f);
	EXPECT_FLOAT_EQ(m.at(1, 1), 1.f);
	EXPECT_FLOAT_EQ(m.at(2, 2), -2.f);
	EXPECT_FLOAT_EQ(m.at(3, 3), 1.f);
	EXPECT_FLOAT_EQ(m.at(0, 3), 0.f);
	EXPECT_FLOAT_EQ(m.at(1, 3), 0.f);
	EXPECT_FLOAT_EQ(m.at(2, 3), -1.f);
}

//-----------------------------------------------------------------------------
// SetLookAt (tableaux float[3])
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, SetLookAt)
{
	Matrix4f m;
	float eye[3] = {0.f, 0.f, 5.f};
	float center[3] = {0.f, 0.f, 0.f};
	float up[3] = {0.f, 1.f, 0.f};
	m.SetLookAt(eye, center, up);

	Matrix4f expected({ 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, -5.f, 0.f, 0.f, 0.f, 1.f});
	EXPECT_TRUE(m == expected);
}

//-----------------------------------------------------------------------------
// SetPerspective (Vulkan: radians, Y-flip, depth [0,1])
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, SetPerspective_radians)
{
	Matrix4f m;
	float fovyRad = 45.f * 3.14159265358979323846f / 180.f;
	m.SetPerspective(fovyRad, 1.333f, 0.1f, 100.f);
	EXPECT_FLOAT_EQ(m.at(3, 2), -1.f);
	EXPECT_FLOAT_EQ(m.at(3, 3), 0.f);
	EXPECT_FLOAT_EQ(m.at(0, 1), 0.f);
	EXPECT_FLOAT_EQ(m.at(1, 0), 0.f);
	// Y-flip: at(1,1) < 0
	EXPECT_LT(m.at(1, 1), 0.f);
}

//-----------------------------------------------------------------------------
// SetPerspectiveGL (degrés, OpenGL depth [-1,1])
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_matrix4, SetPerspectiveGL_degrees)
{
	Matrix4f m;
	m.SetPerspectiveGL(45.f, 1.333f, 0.1f, 100.f);
	EXPECT_FLOAT_EQ(m.at(3, 2), -1.f);
	EXPECT_FLOAT_EQ(m.at(3, 3), 0.f);
}
