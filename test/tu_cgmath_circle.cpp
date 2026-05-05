#include <gtest/gtest.h>
#include <cmath>

#include "../src/cgmath/geometry.h"

TEST(TEST_cgmath_circle, DefaultConstructor)
{
    Circle c;
    EXPECT_DOUBLE_EQ(c.center.x, 0.0);
    EXPECT_DOUBLE_EQ(c.center.y, 0.0);
    EXPECT_DOUBLE_EQ(c.radius, 0.0);
}

TEST(TEST_cgmath_circle, ParameterizedConstructor)
{
    Circle c(Vector2d(1.0, 2.0), 3.0);
    EXPECT_DOUBLE_EQ(c.center.x, 1.0);
    EXPECT_DOUBLE_EQ(c.center.y, 2.0);
    EXPECT_DOUBLE_EQ(c.radius, 3.0);
}

TEST(TEST_cgmath_circle, PointAtCardinalAngles)
{
    const double PI_2 = std::atan2(1.0, 0.0);
    Circle c(Vector2d(0.0, 0.0), 1.0);

    Vector2d p0    = c.pointAt(0.0);
    Vector2d ppi2  = c.pointAt(PI_2);
    Vector2d ppi   = c.pointAt(2.0 * PI_2);
    Vector2d pmpi2 = c.pointAt(-PI_2);

    EXPECT_NEAR(p0.x, 1.0, 1e-12);
    EXPECT_NEAR(p0.y, 0.0, 1e-12);
    EXPECT_NEAR(ppi2.x, 0.0, 1e-12);
    EXPECT_NEAR(ppi2.y, 1.0, 1e-12);
    EXPECT_NEAR(ppi.x, -1.0, 1e-12);
    EXPECT_NEAR(ppi.y, 0.0, 1e-12);
    EXPECT_NEAR(pmpi2.x, 0.0, 1e-12);
    EXPECT_NEAR(pmpi2.y, -1.0, 1e-12);
}

TEST(TEST_cgmath_circle, PointAtRespectsCenterAndRadius)
{
    Circle c(Vector2d(2.0, -1.0), 5.0);
    Vector2d p = c.pointAt(0.0);
    EXPECT_NEAR(p.x, 7.0, 1e-12);
    EXPECT_NEAR(p.y, -1.0, 1e-12);
}

TEST(TEST_cgmath_circle, AngleAtIsInverseOfPointAt)
{
    Circle c(Vector2d(2.0, -1.0), 5.0);
    double theta = 0.7;
    Vector2d p = c.pointAt(theta);
    EXPECT_NEAR(c.angleAt(p), theta, 1e-12);
}

TEST(TEST_cgmath_circle, ContainsPointOnCircle)
{
    Circle c(Vector2d(0.0, 0.0), 1.0);
    EXPECT_TRUE(c.contains(Vector2d(1.0, 0.0)));
    EXPECT_TRUE(c.contains(Vector2d(0.0, 1.0)));
    EXPECT_TRUE(c.contains(Vector2d(-1.0, 0.0)));
    EXPECT_FALSE(c.contains(Vector2d(0.5, 0.0)));
    EXPECT_FALSE(c.contains(Vector2d(2.0, 0.0)));
}

TEST(TEST_cgmath_circle, IntersectionDisjoint)
{
    Circle c0(Vector2d(0.0, 0.0), 1.0);
    Circle c1(Vector2d(5.0, 0.0), 1.0);
    auto r = c0.intersection(c1);
    EXPECT_EQ(r.count, 0);
}

TEST(TEST_cgmath_circle, IntersectionOneInsideOther)
{
    Circle c0(Vector2d(0.0, 0.0), 5.0);
    Circle c1(Vector2d(0.5, 0.0), 1.0);
    auto r = c0.intersection(c1);
    EXPECT_EQ(r.count, 0);
}

TEST(TEST_cgmath_circle, IntersectionConcentric)
{
    Circle c0(Vector2d(1.0, 1.0), 1.0);
    Circle c1(Vector2d(1.0, 1.0), 2.0);
    auto r = c0.intersection(c1);
    EXPECT_EQ(r.count, 0);
}

TEST(TEST_cgmath_circle, IntersectionTangentExternal)
{
    Circle c0(Vector2d(0.0, 0.0), 1.0);
    Circle c1(Vector2d(2.0, 0.0), 1.0);
    auto r = c0.intersection(c1);
    EXPECT_EQ(r.count, 1);
    EXPECT_NEAR(r.pts[0].x, 1.0, 1e-12);
    EXPECT_NEAR(r.pts[0].y, 0.0, 1e-12);
}

TEST(TEST_cgmath_circle, IntersectionTangentInternal)
{
    Circle c0(Vector2d(0.0, 0.0), 2.0);
    Circle c1(Vector2d(1.0, 0.0), 1.0);
    auto r = c0.intersection(c1);
    EXPECT_EQ(r.count, 1);
    EXPECT_NEAR(r.pts[0].x, 2.0, 1e-12);
    EXPECT_NEAR(r.pts[0].y, 0.0, 1e-12);
}

// Equilateral configuration (Havemann section 2) :
// pL = (-d/2, 0), pR = (d/2, 0), both radii = d.
// Apex (upper intersection) at (0, d * sqrt(3) / 2).
TEST(TEST_cgmath_circle, IntersectionEquilateral)
{
    const double d = 200.0;
    Circle cL(Vector2d(-d / 2.0, 0.0), d);
    Circle cR(Vector2d( d / 2.0, 0.0), d);

    auto r = cL.intersection(cR);
    ASSERT_EQ(r.count, 2);

    const double expectedY = d * std::sqrt(3.0) / 2.0;
    EXPECT_NEAR(r.pts[0].x,  0.0,        1e-9);
    EXPECT_NEAR(r.pts[0].y,  expectedY,  1e-9);
    EXPECT_NEAR(r.pts[1].x,  0.0,        1e-9);
    EXPECT_NEAR(r.pts[1].y, -expectedY,  1e-9);
}

// Two unit circles overlapping symmetrically along x.
// Intersection points are vertically aligned at x = (d/2).
TEST(TEST_cgmath_circle, IntersectionSymmetricOverlap)
{
    Circle c0(Vector2d(0.0, 0.0), 5.0);
    Circle c1(Vector2d(6.0, 0.0), 5.0);

    auto r = c0.intersection(c1);
    ASSERT_EQ(r.count, 2);

    // Sorted by descending y.
    EXPECT_GE(r.pts[0].y, r.pts[1].y);

    EXPECT_NEAR(r.pts[0].x, 3.0, 1e-12);
    EXPECT_NEAR(r.pts[1].x, 3.0, 1e-12);

    // y = sqrt(r^2 - (d/2)^2) = sqrt(25 - 9) = 4
    EXPECT_NEAR(r.pts[0].y,  4.0, 1e-12);
    EXPECT_NEAR(r.pts[1].y, -4.0, 1e-12);
}

TEST(TEST_cgmath_circle, IntersectionResultPointsAreOnBothCircles)
{
    Circle c0(Vector2d(-1.0, 0.5), 3.0);
    Circle c1(Vector2d( 2.0, 1.5), 2.5);

    auto r = c0.intersection(c1);
    ASSERT_EQ(r.count, 2);

    for (int i = 0; i < r.count; ++i)
    {
        EXPECT_TRUE(c0.contains(r.pts[i], 1e-9));
        EXPECT_TRUE(c1.contains(r.pts[i], 1e-9));
    }
}

//
// segmentIntersection
//

TEST(TEST_cgmath_circle, SegmentIntersectionMissesCircle)
{
    Circle c(Vector2d(0.0, 0.0), 1.0);
    auto r = c.segmentIntersection(Vector2d(2.0, -1.0), Vector2d(2.0, 1.0));
    EXPECT_EQ(r.count, 0);
}

TEST(TEST_cgmath_circle, SegmentIntersectionTwoPoints)
{
    Circle c(Vector2d(0.0, 0.0), 1.0);
    // Horizontal segment from (-2,0) to (2,0) crosses the unit circle at (-1,0) and (1,0).
    auto r = c.segmentIntersection(Vector2d(-2.0, 0.0), Vector2d(2.0, 0.0));
    ASSERT_EQ(r.count, 2);
    // pts sorted by ascending t : pts[0] at t=0.25 -> (-1,0), pts[1] at t=0.75 -> (1,0).
    EXPECT_NEAR(r.pts[0].x, -1.0, 1e-12);
    EXPECT_NEAR(r.pts[0].y,  0.0, 1e-12);
    EXPECT_NEAR(r.pts[1].x,  1.0, 1e-12);
    EXPECT_NEAR(r.pts[1].y,  0.0, 1e-12);
}

TEST(TEST_cgmath_circle, SegmentIntersectionTangent)
{
    Circle c(Vector2d(0.0, 0.0), 1.0);
    // Tangent line y = 1, from x=-2 to x=2 -> single contact at (0,1).
    auto r = c.segmentIntersection(Vector2d(-2.0, 1.0), Vector2d(2.0, 1.0));
    EXPECT_EQ(r.count, 1);
    EXPECT_NEAR(r.pts[0].x, 0.0, 1e-9);
    EXPECT_NEAR(r.pts[0].y, 1.0, 1e-9);
}

TEST(TEST_cgmath_circle, SegmentIntersectionEndpointInside)
{
    Circle c(Vector2d(0.0, 0.0), 1.0);
    // From inside the circle (0,0) to outside (2,0) -> single exit point at (1,0).
    auto r = c.segmentIntersection(Vector2d(0.0, 0.0), Vector2d(2.0, 0.0));
    EXPECT_EQ(r.count, 1);
    EXPECT_NEAR(r.pts[0].x, 1.0, 1e-12);
    EXPECT_NEAR(r.pts[0].y, 0.0, 1e-12);
}

TEST(TEST_cgmath_circle, SegmentIntersectionFullyInside)
{
    Circle c(Vector2d(0.0, 0.0), 5.0);
    // Both endpoints strictly inside the circle: the segment never crosses it.
    auto r = c.segmentIntersection(Vector2d(-1.0, 0.0), Vector2d(1.0, 0.0));
    EXPECT_EQ(r.count, 0);
}

TEST(TEST_cgmath_circle, SegmentIntersectionLineMissesButLineWouldHit)
{
    Circle c(Vector2d(0.0, 0.0), 1.0);
    // Segment far from circle in segment range, but the underlying line would hit.
    // From (5,0) to (10,0): both far past the circle, no intersection on the segment.
    auto r = c.segmentIntersection(Vector2d(5.0, 0.0), Vector2d(10.0, 0.0));
    EXPECT_EQ(r.count, 0);
}

TEST(TEST_cgmath_circle, SegmentIntersectionResultLiesOnCircle)
{
    Circle c(Vector2d(2.0, -1.0), 3.0);
    Vector2d a(-5.0, -1.0);
    Vector2d b( 8.0, -1.0);
    auto r = c.segmentIntersection(a, b);
    ASSERT_EQ(r.count, 2);
    for (int i = 0; i < r.count; ++i)
        EXPECT_TRUE(c.contains(r.pts[i], 1e-9));
}

TEST(TEST_cgmath_circle, SegmentIntersectionSortedByT)
{
    Circle c(Vector2d(0.0, 0.0), 5.0);
    // Diagonal segment crossing the circle.
    Vector2d a(-10.0, -10.0);
    Vector2d b( 10.0,  10.0);
    auto r = c.segmentIntersection(a, b);
    ASSERT_EQ(r.count, 2);
    // pts[0] should be closer to a (lower-left), pts[1] closer to b (upper-right).
    EXPECT_LT(r.pts[0].x, r.pts[1].x);
    EXPECT_LT(r.pts[0].y, r.pts[1].y);
}
