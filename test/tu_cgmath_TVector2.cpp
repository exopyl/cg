#include <gtest/gtest.h>
#include <cmath>

#include "../src/cgmath/TVector2.h"

TEST(TEST_cgmath_TVector2, DefaultConstructorIsZero)
{
    Vector2d v;
    EXPECT_DOUBLE_EQ(v.x, 0.0);
    EXPECT_DOUBLE_EQ(v.y, 0.0);
}

TEST(TEST_cgmath_TVector2, ParameterizedConstructor)
{
    Vector2d v(1.5, -2.5);
    EXPECT_DOUBLE_EQ(v.x, 1.5);
    EXPECT_DOUBLE_EQ(v.y, -2.5);
}

TEST(TEST_cgmath_TVector2, AddSubScale)
{
    Vector2d a(1.0, 2.0);
    Vector2d b(3.0, 4.0);

    Vector2d sum = a + b;
    Vector2d diff = b - a;
    Vector2d scaled = a * 2.5;

    EXPECT_DOUBLE_EQ(sum.x, 4.0);
    EXPECT_DOUBLE_EQ(sum.y, 6.0);
    EXPECT_DOUBLE_EQ(diff.x, 2.0);
    EXPECT_DOUBLE_EQ(diff.y, 2.0);
    EXPECT_DOUBLE_EQ(scaled.x, 2.5);
    EXPECT_DOUBLE_EQ(scaled.y, 5.0);
}

TEST(TEST_cgmath_TVector2, DotProduct)
{
    Vector2d a(1.0, 0.0);
    Vector2d b(0.0, 1.0);
    Vector2d c(2.0, 3.0);
    EXPECT_DOUBLE_EQ(a.DotProduct(b), 0.0);
    EXPECT_DOUBLE_EQ(a.DotProduct(c), 2.0);
    EXPECT_DOUBLE_EQ(c.DotProduct(c), 13.0);
}

TEST(TEST_cgmath_TVector2, CrossProductIsZComponent)
{
    Vector2d ex(1.0, 0.0);
    Vector2d ey(0.0, 1.0);
    EXPECT_DOUBLE_EQ(ex.CrossProduct(ey), 1.0);
    EXPECT_DOUBLE_EQ(ey.CrossProduct(ex), -1.0);
    EXPECT_DOUBLE_EQ(ex.CrossProduct(ex), 0.0);

    Vector2d a(2.0, 3.0);
    Vector2d b(5.0, 7.0);
    EXPECT_DOUBLE_EQ(a.CrossProduct(b), 2.0 * 7.0 - 3.0 * 5.0);
}

TEST(TEST_cgmath_TVector2, GetDistance)
{
    Vector2d a(0.0, 0.0);
    Vector2d b(3.0, 4.0);
    EXPECT_DOUBLE_EQ(a.getDistance(b), 5.0);
}

TEST(TEST_cgmath_TVector2, StaticDistance)
{
    Vector2d a(0.0, 0.0);
    Vector2d b(3.0, 4.0);
    EXPECT_DOUBLE_EQ(Vector2d::Distance(a, b), 5.0);
    EXPECT_DOUBLE_EQ(Vector2d::Distance(b, a), 5.0);
}

TEST(TEST_cgmath_TVector2, PerpRotates90Degrees)
{
    Vector2d ex(1.0, 0.0);
    Vector2d r1 = ex.Perp();
    EXPECT_DOUBLE_EQ(r1.x, 0.0);
    EXPECT_DOUBLE_EQ(r1.y, 1.0);

    Vector2d ey(0.0, 1.0);
    Vector2d r2 = ey.Perp();
    EXPECT_DOUBLE_EQ(r2.x, -1.0);
    EXPECT_DOUBLE_EQ(r2.y, 0.0);

    // Perp applied four times returns the original.
    Vector2d v(2.0, -3.0);
    Vector2d w = v.Perp().Perp().Perp().Perp();
    EXPECT_DOUBLE_EQ(w.x, v.x);
    EXPECT_DOUBLE_EQ(w.y, v.y);
}

TEST(TEST_cgmath_TVector2, LerpInterpolates)
{
    Vector2d a(0.0, 0.0);
    Vector2d b(1.0, 0.0);

    Vector2d m = Vector2d::Lerp(a, b, 0.5);
    EXPECT_DOUBLE_EQ(m.x, 0.5);
    EXPECT_DOUBLE_EQ(m.y, 0.0);

    Vector2d at0 = Vector2d::Lerp(a, b, 0.0);
    Vector2d at1 = Vector2d::Lerp(a, b, 1.0);
    EXPECT_DOUBLE_EQ(at0.x, a.x);
    EXPECT_DOUBLE_EQ(at0.y, a.y);
    EXPECT_DOUBLE_EQ(at1.x, b.x);
    EXPECT_DOUBLE_EQ(at1.y, b.y);
}

TEST(TEST_cgmath_TVector2, LerpDiagonal)
{
    Vector2d a(-1.0, 2.0);
    Vector2d b(3.0, -2.0);
    Vector2d m = Vector2d::Lerp(a, b, 0.25);
    EXPECT_DOUBLE_EQ(m.x, 0.0);
    EXPECT_DOUBLE_EQ(m.y, 1.0);
}

TEST(TEST_cgmath_TVector2, SignedAngleViaAtan2)
{
    const double PI_2 = std::atan2(1.0, 0.0);
    Vector2d ex(1.0, 0.0);
    Vector2d ey(0.0, 1.0);
    Vector2d eym(0.0, -1.0);

    EXPECT_NEAR(Vector2d::SignedAngle(ex, ey), PI_2, 1e-12);
    EXPECT_NEAR(Vector2d::SignedAngle(ex, eym), -PI_2, 1e-12);
    EXPECT_NEAR(Vector2d::SignedAngle(ex, ex), 0.0, 1e-12);
    EXPECT_NEAR(Vector2d::SignedAngle(ey, ex), -PI_2, 1e-12);
}

TEST(TEST_cgmath_TVector2, NormalizeUnitLength)
{
    Vector2d v(3.0, 4.0);
    v.Normalize();
    EXPECT_NEAR(v.getLength(), 1.0, 1e-12);
    EXPECT_NEAR(v.x, 0.6, 1e-12);
    EXPECT_NEAR(v.y, 0.8, 1e-12);
}

TEST(TEST_cgmath_TVector2, NormalizeOfZeroIsZero)
{
    Vector2d v(0.0, 0.0);
    v.Normalize();
    EXPECT_DOUBLE_EQ(v.x, 0.0);
    EXPECT_DOUBLE_EQ(v.y, 0.0);
}

TEST(TEST_cgmath_TVector2, GetLengthAndLength2)
{
    Vector2d v(3.0, 4.0);
    EXPECT_DOUBLE_EQ(v.getLength2(), 25.0);
    EXPECT_DOUBLE_EQ(v.getLength(), 5.0);
}
