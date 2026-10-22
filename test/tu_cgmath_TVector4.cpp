#include <gtest/gtest.h>

#include "../src/cgmath/TVector4.h"

//
// Minimal real coverage for TVector4 (previously only exercised by a
// no-assertion "ghost" test that has been removed).
//

TEST(TEST_cgmath_TVector4, ConstructAndAccess)
{
	Vector4f v(1.f, 2.f, 3.f, 4.f);
	EXPECT_FLOAT_EQ(v.x, 1.f);
	EXPECT_FLOAT_EQ(v.y, 2.f);
	EXPECT_FLOAT_EQ(v.z, 3.f);
	EXPECT_FLOAT_EQ(v.w, 4.f);
	EXPECT_FLOAT_EQ(v[2], 3.f);
}

TEST(TEST_cgmath_TVector4, Arithmetic)
{
	Vector4f a(1.f, 2.f, 3.f, 4.f);
	Vector4f b(5.f, 6.f, 7.f, 8.f);

	Vector4f s = a + b;
	EXPECT_FLOAT_EQ(s.x, 6.f);
	EXPECT_FLOAT_EQ(s.w, 12.f);

	Vector4f d = b - a;
	EXPECT_FLOAT_EQ(d.x, 4.f);
	EXPECT_FLOAT_EQ(d.w, 4.f);

	Vector4f m = a * 2.f;
	EXPECT_FLOAT_EQ(m.x, 2.f);
	EXPECT_FLOAT_EQ(m.w, 8.f);
}

// Comparaisons sur des operandes const : elles comparent les composantes, pas les
// adresses (operator const float*() ne doit pas etre retenu).
TEST(TEST_cgmath_TVector4, EqualityOnConstOperands)
{
    const Vector4f a(1.f, 2.f, 3.f, 4.f);
    const Vector4f b(1.f, 2.f, 3.f, 4.f);
    const Vector4f c(1.f, 2.f, 3.f, 5.f);
    Vector4f m(1.f, 2.f, 3.f, 4.f);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a != c);
    EXPECT_TRUE(a == m);
    EXPECT_FALSE(a != m);
    EXPECT_TRUE(m == a);
    EXPECT_FALSE(c == m);
    EXPECT_TRUE(c != m);
    EXPECT_TRUE(a == a);
}
