#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "../src/cgmath/geometry.h"

namespace
{
    const double PI     = 3.14159265358979323846;
    const double PI_2   = PI / 2.0;
    const double PI_4   = PI / 4.0;
    const double PI_8   = PI / 8.0;
    const double TWO_PI = 2.0 * PI;
}

//
// Constructors
//

TEST(TEST_cgmath_arc, DefaultConstructor)
{
    Arc a;
    EXPECT_DOUBLE_EQ(a.angleStart, 0.0);
    EXPECT_DOUBLE_EQ(a.angleEnd, 0.0);
    EXPECT_TRUE(a.ccw);
    EXPECT_DOUBLE_EQ(a.circle.radius, 0.0);
}

TEST(TEST_cgmath_arc, AngleConstructor)
{
    Arc a(Circle(Vector2d(1.0, 2.0), 3.0), 0.0, PI_2, true);
    EXPECT_DOUBLE_EQ(a.angleStart, 0.0);
    EXPECT_DOUBLE_EQ(a.angleEnd, PI_2);
    EXPECT_TRUE(a.ccw);
    EXPECT_DOUBLE_EQ(a.circle.center.x, 1.0);
    EXPECT_DOUBLE_EQ(a.circle.radius, 3.0);
}

TEST(TEST_cgmath_arc, FromPointsConstructor)
{
    Circle c(Vector2d(0.0, 0.0), 1.0);
    Arc a(c, Vector2d(1.0, 0.0), Vector2d(0.0, 1.0), true);

    EXPECT_NEAR(a.angleStart, 0.0,  1e-12);
    EXPECT_NEAR(a.angleEnd,   PI_2, 1e-12);

    Vector2d p0 = a.pointAt(0.0);
    Vector2d p1 = a.pointAt(1.0);
    EXPECT_NEAR(p0.x, 1.0, 1e-12);
    EXPECT_NEAR(p0.y, 0.0, 1e-12);
    EXPECT_NEAR(p1.x, 0.0, 1e-12);
    EXPECT_NEAR(p1.y, 1.0, 1e-12);
}

//
// spanAngle / length
//

TEST(TEST_cgmath_arc, SemicircleCcwSpanAndLength)
{
    Arc a(Circle(Vector2d(0.0, 0.0), 2.0), 0.0, PI, true);
    EXPECT_NEAR(a.spanAngle(), PI,         1e-12);
    EXPECT_NEAR(a.length(),    PI * 2.0,   1e-12);
}

TEST(TEST_cgmath_arc, SemicircleCwSpanAndLength)
{
    // Same endpoints, but going clockwise -> span is negative, length is the same.
    Arc a(Circle(Vector2d(0.0, 0.0), 2.0), 0.0, PI, false);
    EXPECT_NEAR(a.spanAngle(), -PI,        1e-12);
    EXPECT_NEAR(a.length(),    PI * 2.0,   1e-12);

    // CW path goes through the lower half: at t=0.5, theta = -PI/2 -> (0, -2)
    Vector2d mid = a.pointAt(0.5);
    EXPECT_NEAR(mid.x,  0.0, 1e-12);
    EXPECT_NEAR(mid.y, -2.0, 1e-12);
}

TEST(TEST_cgmath_arc, FullCircleLength)
{
    Arc a(Circle(Vector2d(0.0, 0.0), 5.0), 0.0, TWO_PI, true);
    EXPECT_NEAR(a.spanAngle(), TWO_PI,      1e-12);
    EXPECT_NEAR(a.length(),    TWO_PI * 5,  1e-12);
}

TEST(TEST_cgmath_arc, DegenerateZeroSpan)
{
    Arc a(Circle(Vector2d(0.0, 0.0), 1.0), PI_2, PI_2, true);
    EXPECT_DOUBLE_EQ(a.spanAngle(), 0.0);
    EXPECT_DOUBLE_EQ(a.length(),    0.0);

    // pointAt(0) and pointAt(1) coincide.
    Vector2d p0 = a.pointAt(0.0);
    Vector2d p1 = a.pointAt(1.0);
    EXPECT_NEAR(p0.x, p1.x, 1e-12);
    EXPECT_NEAR(p0.y, p1.y, 1e-12);
}

//
// pointAt / tangentAt / normalAt
//

TEST(TEST_cgmath_arc, EndpointsMatchCirclePointAt)
{
    Circle c(Vector2d(2.0, -1.0), 4.0);
    Arc a(c, PI_4, 3.0 * PI_4, true);

    Vector2d expectedStart = c.pointAt(PI_4);
    Vector2d expectedEnd   = c.pointAt(3.0 * PI_4);

    Vector2d p0 = a.pointAt(0.0);
    Vector2d p1 = a.pointAt(1.0);
    EXPECT_NEAR(p0.x, expectedStart.x, 1e-12);
    EXPECT_NEAR(p0.y, expectedStart.y, 1e-12);
    EXPECT_NEAR(p1.x, expectedEnd.x,   1e-12);
    EXPECT_NEAR(p1.y, expectedEnd.y,   1e-12);
}

TEST(TEST_cgmath_arc, NormalPointsTowardCenter)
{
    Circle c(Vector2d(3.0, 4.0), 2.0);
    Arc a(c, 0.0, PI, true);

    for (int i = 0; i <= 8; ++i)
    {
        double t = (double)i / 8.0;
        Vector2d p = a.pointAt(t);
        Vector2d n = a.normalAt(t);

        // n should equal (center - p) / radius, i.e. unit-length toward center.
        double nx = (c.center.x - p.x) / c.radius;
        double ny = (c.center.y - p.y) / c.radius;
        EXPECT_NEAR(n.x, nx, 1e-12);
        EXPECT_NEAR(n.y, ny, 1e-12);

        // |n| == 1
        EXPECT_NEAR(n.getLength(), 1.0, 1e-12);
    }
}

TEST(TEST_cgmath_arc, NormalIndependentOfDirection)
{
    Circle c(Vector2d(0.0, 0.0), 1.0);
    Arc ccw(c, PI_4, 3.0 * PI_4, true);
    Arc cw (c, PI_4, 3.0 * PI_4, false);

    // Both arcs evaluated at the same theta should yield the same normal.
    // ccw: theta(t) = PI/4 + t * PI/2
    // cw : theta(t) = PI/4 + t * (-3*PI/2)
    // Pick the same t for ccw and (1 - t') for cw such that theta matches:
    // We just compare normals at corresponding theta on the same circle: at the
    // shared start angle PI/4, both arcs' normalAt(0) must agree.
    Vector2d nCcw = ccw.normalAt(0.0);
    Vector2d nCw  = cw.normalAt(0.0);
    EXPECT_NEAR(nCcw.x, nCw.x, 1e-12);
    EXPECT_NEAR(nCcw.y, nCw.y, 1e-12);
}

TEST(TEST_cgmath_arc, TangentIsUnitVector)
{
    Arc a(Circle(Vector2d(1.0, 1.0), 3.0), 0.0, PI, true);
    for (int i = 0; i <= 8; ++i)
    {
        double t = (double)i / 8.0;
        Vector2d tg = a.tangentAt(t);
        EXPECT_NEAR(tg.getLength(), 1.0, 1e-12);
    }
}

TEST(TEST_cgmath_arc, TangentPerpendicularToNormal)
{
    Arc a(Circle(Vector2d(1.0, 1.0), 3.0), 0.0, PI, true);
    for (int i = 0; i <= 10; ++i)
    {
        double t = (double)i / 10.0;
        Vector2d tg = a.tangentAt(t);
        Vector2d n  = a.normalAt(t);
        EXPECT_NEAR(tg.DotProduct(n), 0.0, 1e-12);
    }
}

TEST(TEST_cgmath_arc, TangentReversesWithDirection)
{
    Circle c(Vector2d(0.0, 0.0), 1.0);
    Arc ccw(c, 0.0, PI, true);
    Arc cw (c, 0.0, PI, false);

    // At theta=0 (i.e. t=0 for both), tangents should be opposite.
    Vector2d tgCcw = ccw.tangentAt(0.0);
    Vector2d tgCw  = cw.tangentAt(0.0);
    EXPECT_NEAR(tgCcw.x, -tgCw.x, 1e-12);
    EXPECT_NEAR(tgCcw.y, -tgCw.y, 1e-12);
}

//
// tessellate
//

TEST(TEST_cgmath_arc, TessellateQuarterCircle)
{
    // Quarter circle CCW from theta=0 to PI/2, radius=1, n=4
    // -> 5 points at theta = 0, PI/8, PI/4, 3*PI/8, PI/2
    Arc a(Circle(Vector2d(0.0, 0.0), 1.0), 0.0, PI_2, true);
    auto pts = a.tessellate(4);
    ASSERT_EQ(pts.size(), 5u);

    for (int i = 0; i < 5; ++i)
    {
        double theta = i * PI_8;
        EXPECT_NEAR(pts[i].x, std::cos(theta), 1e-12);
        EXPECT_NEAR(pts[i].y, std::sin(theta), 1e-12);
    }
}

TEST(TEST_cgmath_arc, TessellateEndpointsMatchPointAt)
{
    Arc a(Circle(Vector2d(2.0, -1.0), 5.0), PI_4, 5.0 * PI_4, true);
    auto pts = a.tessellate(7);
    ASSERT_EQ(pts.size(), 8u);

    Vector2d p0 = a.pointAt(0.0);
    Vector2d p1 = a.pointAt(1.0);
    EXPECT_NEAR(pts.front().x, p0.x, 1e-12);
    EXPECT_NEAR(pts.front().y, p0.y, 1e-12);
    EXPECT_NEAR(pts.back().x,  p1.x, 1e-12);
    EXPECT_NEAR(pts.back().y,  p1.y, 1e-12);
}

TEST(TEST_cgmath_arc, TessellateClampsBelowOne)
{
    Arc a(Circle(Vector2d(0.0, 0.0), 1.0), 0.0, PI_2, true);
    auto pts = a.tessellate(0);
    EXPECT_EQ(pts.size(), 2u);   // clamped to n=1 -> 2 points
}

//
// tessellateAdaptive
//

TEST(TEST_cgmath_arc, TessellateAdaptiveSemicircleQuarterAngle)
{
    // Semicircle with maxAngle = PI/4 -> n = ceil(PI / (PI/4)) = 4 -> 5 points
    Arc a(Circle(Vector2d(0.0, 0.0), 1.0), 0.0, PI, true);
    auto pts = a.tessellateAdaptive(PI_4);
    EXPECT_EQ(pts.size(), 5u);
}

TEST(TEST_cgmath_arc, TessellateAdaptiveCeilsUp)
{
    // Span PI, maxAngle slightly less than PI/3 -> n = ceil(PI / x) where x = PI/3 - eps
    // Should be 4 (since PI / (PI/3) = 3 exactly, but slightly less than PI/3
    // gives n > 3 -> ceil = 4).
    Arc a(Circle(Vector2d(0.0, 0.0), 1.0), 0.0, PI, true);
    auto pts = a.tessellateAdaptive(PI / 3.0 - 1e-6);
    EXPECT_EQ(pts.size(), 5u);
}

TEST(TEST_cgmath_arc, TessellateAdaptiveZeroOrNegativeFallsBack)
{
    Arc a(Circle(Vector2d(0.0, 0.0), 1.0), 0.0, PI, true);
    auto pts0 = a.tessellateAdaptive(0.0);
    auto ptsN = a.tessellateAdaptive(-1.0);
    EXPECT_EQ(pts0.size(), 2u);
    EXPECT_EQ(ptsN.size(), 2u);
}

TEST(TEST_cgmath_arc, TessellateAdaptiveCwUsesAbsoluteSpan)
{
    // Same span magnitude regardless of direction.
    Arc ccw(Circle(Vector2d(0.0, 0.0), 1.0), 0.0, PI, true);
    Arc cw (Circle(Vector2d(0.0, 0.0), 1.0), 0.0, PI, false);
    auto pCcw = ccw.tessellateAdaptive(PI_4);
    auto pCw  = cw.tessellateAdaptive(PI_4);
    EXPECT_EQ(pCcw.size(), pCw.size());
    EXPECT_EQ(pCw.size(), 5u);
}
