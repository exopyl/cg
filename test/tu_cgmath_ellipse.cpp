#include <gtest/gtest.h>
#include <cmath>

#include "../src/cgmath/geometry.h"

TEST(TEST_cgmath_ellipse, DefaultConstructor)
{
    Ellipse e;
    EXPECT_DOUBLE_EQ(e.f1.x, 0.0);
    EXPECT_DOUBLE_EQ(e.f1.y, 0.0);
    EXPECT_DOUBLE_EQ(e.f2.x, 0.0);
    EXPECT_DOUBLE_EQ(e.f2.y, 0.0);
    EXPECT_DOUBLE_EQ(e.sumDist, 0.0);
}

TEST(TEST_cgmath_ellipse, ParameterizedConstructor)
{
    Ellipse e(Vector2d(-3.0, 0.0), Vector2d(3.0, 0.0), 10.0);
    EXPECT_DOUBLE_EQ(e.f1.x, -3.0);
    EXPECT_DOUBLE_EQ(e.f2.x,  3.0);
    EXPECT_DOUBLE_EQ(e.sumDist, 10.0);
}

TEST(TEST_cgmath_ellipse, GeometricProperties)
{
    Ellipse e(Vector2d(-3.0, 0.0), Vector2d(3.0, 0.0), 10.0);
    Vector2d c = e.center();
    EXPECT_DOUBLE_EQ(c.x, 0.0);
    EXPECT_DOUBLE_EQ(c.y, 0.0);
    EXPECT_DOUBLE_EQ(e.semiMajor(), 5.0);
    EXPECT_NEAR(e.semiMinor(), 4.0, 1e-12);   // sqrt(25 - 9) = 4
    EXPECT_NEAR(e.angle(), 0.0, 1e-12);
    EXPECT_TRUE(e.valid());
}

TEST(TEST_cgmath_ellipse, AngleForOffAxisFoci)
{
    // Foci on a 45 deg axis.
    double s = std::sqrt(2.0);
    Ellipse e(Vector2d(-s, -s), Vector2d(s, s), 10.0);
    const double PI_4 = std::atan2(1.0, 1.0);
    EXPECT_NEAR(e.angle(), PI_4, 1e-12);
}

TEST(TEST_cgmath_ellipse, InvalidWhenSumDistTooSmall)
{
    // dist(f1, f2) = 6 but sumDist = 4 < 6 -> invalid.
    Ellipse e(Vector2d(-3.0, 0.0), Vector2d(3.0, 0.0), 4.0);
    EXPECT_FALSE(e.valid());
    EXPECT_DOUBLE_EQ(e.semiMinor(), 0.0);
}

TEST(TEST_cgmath_ellipse, ConcentricFociDegenerateToCircle)
{
    // f1 == f2 -> circle of radius sumDist/2.
    Ellipse e(Vector2d(1.0, 2.0), Vector2d(1.0, 2.0), 6.0);
    EXPECT_DOUBLE_EQ(e.semiMajor(), 3.0);
    EXPECT_NEAR(e.semiMinor(), 3.0, 1e-12);
    Vector2d p = e.pointAt(0.0);
    // pointAt(0) returns center + a*(cos ang, sin ang). With f1==f2 the angle
    // is atan2(0,0) = 0, so the point is (center.x + a, center.y).
    EXPECT_NEAR(p.x, 4.0, 1e-12);
    EXPECT_NEAR(p.y, 2.0, 1e-12);
}

TEST(TEST_cgmath_ellipse, PointAtAtParameterZeroIsMajorAxisEndpoint)
{
    Ellipse e(Vector2d(-3.0, 0.0), Vector2d(3.0, 0.0), 10.0);
    Vector2d p = e.pointAt(0.0);
    EXPECT_NEAR(p.x, 5.0, 1e-12);
    EXPECT_NEAR(p.y, 0.0, 1e-12);
}

TEST(TEST_cgmath_ellipse, PointAtAtParameterPiOver2IsMinorAxisEndpoint)
{
    const double PI_2 = std::atan2(1.0, 0.0);
    Ellipse e(Vector2d(-3.0, 0.0), Vector2d(3.0, 0.0), 10.0);
    Vector2d p = e.pointAt(PI_2);
    EXPECT_NEAR(p.x, 0.0, 1e-12);
    EXPECT_NEAR(p.y, 4.0, 1e-12);
}

TEST(TEST_cgmath_ellipse, PointAtPointsLieOnEllipse)
{
    Ellipse e(Vector2d(-2.0, 1.0), Vector2d(2.0, 1.0), 6.0);
    for (int i = 0; i < 16; ++i)
    {
        double t = (double)i * 2.0 * std::atan2(1.0, 0.0) / 8.0;   // i*pi/8
        Vector2d p = e.pointAt(t);
        EXPECT_TRUE(e.contains(p, 1e-9));
    }
}

TEST(TEST_cgmath_ellipse, ContainsKnownPoint)
{
    // For axis-aligned ellipse with a=5, b=4, the point (3, 4*sqrt(1 - 9/25))
    // = (3, 16/5) = (3, 3.2) lies on it.
    Ellipse e(Vector2d(-3.0, 0.0), Vector2d(3.0, 0.0), 10.0);
    EXPECT_TRUE(e.contains(Vector2d(3.0, 16.0 / 5.0), 1e-9));
    EXPECT_FALSE(e.contains(Vector2d(0.0, 0.0)));     // center is inside, not on the curve
    EXPECT_FALSE(e.contains(Vector2d(10.0, 0.0)));    // outside
}

//
// verticalIntersection
//

TEST(TEST_cgmath_ellipse, VerticalIntersectionAxisAlignedTwoPoints)
{
    // a=5, b=4, axis-aligned. At x=0 -> y = +/-4.
    Ellipse e(Vector2d(-3.0, 0.0), Vector2d(3.0, 0.0), 10.0);
    auto r = e.verticalIntersection(0.0);
    ASSERT_EQ(r.count, 2);
    // pts[0] = upper.
    EXPECT_NEAR(r.pts[0].x,  0.0, 1e-12);
    EXPECT_NEAR(r.pts[0].y,  4.0, 1e-9);
    EXPECT_NEAR(r.pts[1].x,  0.0, 1e-12);
    EXPECT_NEAR(r.pts[1].y, -4.0, 1e-9);
}

TEST(TEST_cgmath_ellipse, VerticalIntersectionTangentAtMajorAxisVertex)
{
    Ellipse e(Vector2d(-3.0, 0.0), Vector2d(3.0, 0.0), 10.0);
    auto r = e.verticalIntersection(5.0);   // x = a -> tangent at (5, 0)
    EXPECT_EQ(r.count, 1);
    EXPECT_NEAR(r.pts[0].x, 5.0, 1e-9);
    EXPECT_NEAR(r.pts[0].y, 0.0, 1e-9);
}

TEST(TEST_cgmath_ellipse, VerticalIntersectionOutsideEllipse)
{
    Ellipse e(Vector2d(-3.0, 0.0), Vector2d(3.0, 0.0), 10.0);
    auto r = e.verticalIntersection(10.0);
    EXPECT_EQ(r.count, 0);
}

TEST(TEST_cgmath_ellipse, VerticalIntersectionInvalidEllipse)
{
    // sumDist < dist(f1, f2): no real ellipse.
    Ellipse e(Vector2d(-3.0, 0.0), Vector2d(3.0, 0.0), 4.0);
    auto r = e.verticalIntersection(0.0);
    EXPECT_EQ(r.count, 0);
}

TEST(TEST_cgmath_ellipse, VerticalIntersectionSymmetryAroundXAxis)
{
    Ellipse e(Vector2d(-3.0, 0.0), Vector2d(3.0, 0.0), 10.0);
    auto r = e.verticalIntersection(2.0);
    ASSERT_EQ(r.count, 2);
    EXPECT_NEAR(r.pts[0].x, 2.0, 1e-12);
    EXPECT_NEAR(r.pts[1].x, 2.0, 1e-12);
    EXPECT_NEAR(r.pts[0].y, -r.pts[1].y, 1e-12);
    EXPECT_GT(r.pts[0].y, r.pts[1].y);   // sorted: upper first
}

TEST(TEST_cgmath_ellipse, VerticalIntersectionPointsSatisfyFocalSum)
{
    // Spec requirement: dist(pt, f1) + dist(pt, f2) ~= sumDist.
    Ellipse e(Vector2d(-2.0, 1.0), Vector2d(2.0, 1.0), 6.0);
    auto r = e.verticalIntersection(0.5);
    ASSERT_EQ(r.count, 2);
    for (int i = 0; i < r.count; ++i)
    {
        double d1 = std::hypot(r.pts[i].x - e.f1.x, r.pts[i].y - e.f1.y);
        double d2 = std::hypot(r.pts[i].x - e.f2.x, r.pts[i].y - e.f2.y);
        EXPECT_NEAR(d1 + d2, e.sumDist, 1e-9);
        EXPECT_TRUE(e.contains(r.pts[i], 1e-9));
    }
}

TEST(TEST_cgmath_ellipse, VerticalIntersectionRotatedEllipse)
{
    // Foci on a 45 deg axis : f1 = (-1,-1), f2 = (1,1).
    // dist = 2*sqrt(2), c = sqrt(2). With sumDist = 6, a = 3, b = sqrt(7).
    Ellipse e(Vector2d(-1.0, -1.0), Vector2d(1.0, 1.0), 6.0);
    auto r = e.verticalIntersection(0.0);
    ASSERT_EQ(r.count, 2);

    // Each result must lie on the ellipse (focal-sum check).
    for (int i = 0; i < r.count; ++i)
    {
        double d1 = std::hypot(r.pts[i].x - e.f1.x, r.pts[i].y - e.f1.y);
        double d2 = std::hypot(r.pts[i].x - e.f2.x, r.pts[i].y - e.f2.y);
        EXPECT_NEAR(d1 + d2, e.sumDist, 1e-9);
        EXPECT_NEAR(r.pts[i].x, 0.0, 1e-12);
    }
    EXPECT_GT(r.pts[0].y, r.pts[1].y);
}

// Havemann section 2.3 use case : ellipse foci at the centers of two adjacent
// arcs, sumDist = sum of their radii. The intersection with the symmetry axis
// gives a candidate rosette center mC such that the rosette is internally
// tangent to the F1 circle and externally tangent to the F2 circle.
TEST(TEST_cgmath_ellipse, VerticalIntersectionRosetteUseCase)
{
    Vector2d F1(100.0, 0.0);    // center of main outer arc (radius 200)
    Vector2d F2( 50.0, 0.0);    // center of right sub-arc   (radius  60)
    double rMain = 200.0;
    double rSub  =  60.0;
    double sumDist = rMain + rSub;
    Ellipse e(F1, F2, sumDist);
    ASSERT_TRUE(e.valid());

    // Symmetry axis at x = 0 (main arch centered on origin).
    auto r = e.verticalIntersection(0.0);
    ASSERT_EQ(r.count, 2);

    // Pick the upper candidate (Havemann algorithm uses the apex-side point).
    Vector2d mC = r.pts[0];

    // Tangency relations imposed by the focal-sum construction :
    //   dist(mC, F1) + dist(mC, F2) = rMain + rSub   (focal definition)
    //   r_rosette = rMain - dist(mC, F1)             (internal tangency to F1)
    //   r_rosette = dist(mC, F2) - rSub              (external tangency to F2)
    double d1 = std::hypot(mC.x - F1.x, mC.y - F1.y);
    double d2 = std::hypot(mC.x - F2.x, mC.y - F2.y);
    EXPECT_NEAR(d1 + d2, sumDist, 1e-9);

    double rRosetteFromF1 = rMain - d1;
    double rRosetteFromF2 = d2 - rSub;
    EXPECT_NEAR(rRosetteFromF1, rRosetteFromF2, 1e-9);
    EXPECT_GT(rRosetteFromF1, 0.0);
}
