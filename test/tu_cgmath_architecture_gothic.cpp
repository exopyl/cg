#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "../src/cgmath/architecture_gothic.h"

namespace
{
    const double WIDTH = 200.0;

    ArchBasis makeBasis(double excess)
    {
        ArchBasis b;
        b.pL = Vector2d(-WIDTH / 2.0, 0.0);
        b.pR = Vector2d( WIDTH / 2.0, 0.0);
        b.excess = excess;
        return b;
    }
}

//
// Spec tests (adapted : excess=0.5 reinterpreted as a forbidden degenerate case)
//

TEST(TEST_cgmath_architecture_gothic, EquilateralApexHeight)
{
    ArchGeom g = buildArch(makeBasis(1.0));
    EXPECT_NEAR(g.apex.x, 0.0,                          1e-9);
    EXPECT_NEAR(g.apex.y, (WIDTH / 2.0) * std::sqrt(3), 1e-9);
    EXPECT_NEAR(g.height, (WIDTH / 2.0) * std::sqrt(3), 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, IncreasingExcessIncreasesHeight)
{
    ArchGeom g10 = buildArch(makeBasis(1.0));
    ArchGeom g15 = buildArch(makeBasis(1.5));
    EXPECT_GT(g15.height, g10.height);

    // Closed-form check : height = WIDTH * sqrt(excess^2 - 0.25)
    EXPECT_NEAR(g15.height, WIDTH * std::sqrt(1.5 * 1.5 - 0.25), 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, ApexContinuity)
{
    ArchGeom g = buildArch(makeBasis(1.2));
    Vector2d endL   = g.arcLeft.pointAt(1.0);
    Vector2d startR = g.arcRight.pointAt(0.0);
    EXPECT_NEAR(endL.x, g.apex.x, 1e-9);
    EXPECT_NEAR(endL.y, g.apex.y, 1e-9);
    EXPECT_NEAR(startR.x, g.apex.x, 1e-9);
    EXPECT_NEAR(startR.y, g.apex.y, 1e-9);
}

//
// Geometry sanity
//

TEST(TEST_cgmath_architecture_gothic, WidthEqualsBaseDistance)
{
    ArchGeom g = buildArch(makeBasis(1.0));
    EXPECT_NEAR(g.width, WIDTH, 1e-12);
}

TEST(TEST_cgmath_architecture_gothic, CircleRadiusEqualsExcessTimesWidth)
{
    ArchGeom g = buildArch(makeBasis(1.2));
    EXPECT_NEAR(g.circleL.radius, 1.2 * WIDTH, 1e-12);
    EXPECT_NEAR(g.circleR.radius, 1.2 * WIDTH, 1e-12);
}

TEST(TEST_cgmath_architecture_gothic, CircleCentersAreFootPoints)
{
    ArchGeom g = buildArch(makeBasis(1.0));
    EXPECT_DOUBLE_EQ(g.circleL.center.x, -WIDTH / 2.0);
    EXPECT_DOUBLE_EQ(g.circleR.center.x,  WIDTH / 2.0);
    EXPECT_DOUBLE_EQ(g.circleL.center.y, 0.0);
    EXPECT_DOUBLE_EQ(g.circleR.center.y, 0.0);
}

TEST(TEST_cgmath_architecture_gothic, ApexIsOnBothCircles)
{
    ArchGeom g = buildArch(makeBasis(1.3));
    EXPECT_TRUE(g.circleL.contains(g.apex, 1e-9));
    EXPECT_TRUE(g.circleR.contains(g.apex, 1e-9));
}

TEST(TEST_cgmath_architecture_gothic, ApexIsOnSymmetryAxis)
{
    ArchGeom g = buildArch(makeBasis(1.4));
    double midX = (g.circleL.center.x + g.circleR.center.x) / 2.0;
    EXPECT_NEAR(g.apex.x, midX, 1e-9);
}

//
// Ground footprint behavior (Havemann subtlety)
//

TEST(TEST_cgmath_architecture_gothic, ArcAnchorsToFootForExcess1)
{
    // Equilateral case : the arc starting at the pR-direction on circleL is
    // exactly pR (since |pR - pL| = r = width).
    ArchGeom g = buildArch(makeBasis(1.0));
    Vector2d startL = g.arcLeft.pointAt(0.0);
    Vector2d endR   = g.arcRight.pointAt(1.0);

    EXPECT_NEAR(startL.x,  WIDTH / 2.0, 1e-9);
    EXPECT_NEAR(startL.y,  0.0,         1e-9);
    EXPECT_NEAR(endR.x,   -WIDTH / 2.0, 1e-9);
    EXPECT_NEAR(endR.y,    0.0,         1e-9);
}

TEST(TEST_cgmath_architecture_gothic, ArcExtendsBeyondFootForExcess15)
{
    // For excess > 1, the radius exceeds the inter-foot distance; the arc's
    // ground-anchor lies beyond pR (resp. pL).
    ArchGeom g = buildArch(makeBasis(1.5));
    Vector2d startL = g.arcLeft.pointAt(0.0);
    Vector2d endR   = g.arcRight.pointAt(1.0);

    EXPECT_GT(startL.x,  WIDTH / 2.0);
    EXPECT_LT(endR.x,   -WIDTH / 2.0);
}

//
// Arc orientation and span
//

TEST(TEST_cgmath_architecture_gothic, BothArcsAreCcw)
{
    ArchGeom g = buildArch(makeBasis(1.2));
    EXPECT_TRUE(g.arcLeft.ccw);
    EXPECT_TRUE(g.arcRight.ccw);
}

TEST(TEST_cgmath_architecture_gothic, BothArcsHaveSameSpan)
{
    ArchGeom g = buildArch(makeBasis(1.2));
    EXPECT_NEAR(g.arcLeft.spanAngle(), g.arcRight.spanAngle(), 1e-12);
}

TEST(TEST_cgmath_architecture_gothic, EquilateralArcSpanIsPiOver3)
{
    // For excess == 1, the apex sees pL/pR under an angle of 60 deg from
    // each foot circle's center -> each arc spans pi/3 rad.
    ArchGeom g = buildArch(makeBasis(1.0));
    const double PI = std::atan2(0.0, -1.0);
    EXPECT_NEAR(g.arcLeft.spanAngle(),  PI / 3.0, 1e-9);
    EXPECT_NEAR(g.arcRight.spanAngle(), PI / 3.0, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, TangentDiscontinuityAtApex)
{
    // A pointed arch has a corner at the apex : tangents on either side differ.
    ArchGeom g = buildArch(makeBasis(1.0));
    Vector2d tEndL   = g.arcLeft.tangentAt(1.0);
    Vector2d tStartR = g.arcRight.tangentAt(0.0);

    // Both unit vectors. They are non-parallel iff |dot| < 1.
    double dot = tEndL.DotProduct(tStartR);
    EXPECT_LT(std::fabs(dot), 1.0 - 1e-6);

    // For equilateral : dot = 1/2 (60 deg between tangents on either side).
    EXPECT_NEAR(dot, 0.5, 1e-9);
}

//
// Translation invariance
//

TEST(TEST_cgmath_architecture_gothic, TranslatedBasisProducesTranslatedGeometry)
{
    ArchGeom base = buildArch(makeBasis(1.0));

    Vector2d offset(50.0, 30.0);
    ArchBasis trBasis = makeBasis(1.0);
    trBasis.pL = trBasis.pL + offset;
    trBasis.pR = trBasis.pR + offset;
    ArchGeom tr = buildArch(trBasis);

    EXPECT_NEAR(tr.apex.x,    base.apex.x    + offset.x, 1e-9);
    EXPECT_NEAR(tr.apex.y,    base.apex.y    + offset.y, 1e-9);
    EXPECT_NEAR(tr.height,    base.height,               1e-9);
    EXPECT_NEAR(tr.width,     base.width,                1e-9);
}

//
// Input validation (blinded)
//

TEST(TEST_cgmath_architecture_gothic, ExcessBelowHalfThrows)
{
    EXPECT_THROW(buildArch(makeBasis(0.4)), std::invalid_argument);
    EXPECT_THROW(buildArch(makeBasis(0.0)), std::invalid_argument);
    EXPECT_THROW(buildArch(makeBasis(-1.0)), std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, ExcessExactlyHalfThrows)
{
    // Strict bound : 0.5 produces tangent circles (height 0). Excluded.
    EXPECT_THROW(buildArch(makeBasis(0.5)), std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, ExcessNonFiniteThrows)
{
    EXPECT_THROW(buildArch(makeBasis(std::numeric_limits<double>::infinity())),
                 std::invalid_argument);
    EXPECT_THROW(buildArch(makeBasis(-std::numeric_limits<double>::infinity())),
                 std::invalid_argument);
    EXPECT_THROW(buildArch(makeBasis(std::numeric_limits<double>::quiet_NaN())),
                 std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, ZeroWidthBaseThrows)
{
    ArchBasis b;
    b.pL = Vector2d(50.0, 50.0);
    b.pR = Vector2d(50.0, 50.0);
    b.excess = 1.0;
    EXPECT_THROW(buildArch(b), std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, NonFiniteCoordinatesThrow)
{
    ArchBasis b = makeBasis(1.0);

    b.pL = Vector2d(std::numeric_limits<double>::infinity(), 0.0);
    EXPECT_THROW(buildArch(b), std::invalid_argument);

    b.pL = Vector2d(std::numeric_limits<double>::quiet_NaN(), 0.0);
    EXPECT_THROW(buildArch(b), std::invalid_argument);

    // Reset pL, corrupt pR.
    b = makeBasis(1.0);
    b.pR = Vector2d(0.0, std::numeric_limits<double>::infinity());
    EXPECT_THROW(buildArch(b), std::invalid_argument);

    b.pR = Vector2d(0.0, std::numeric_limits<double>::quiet_NaN());
    EXPECT_THROW(buildArch(b), std::invalid_argument);
}

//
// Offset (A2) — spec tests
//

TEST(TEST_cgmath_architecture_gothic, OffsetOuterRadiusEqualsBasePlusDelta)
{
    ArchGeom geom = buildArch(makeBasis(1.0));
    ArchOffsetParams p; p.outer = 15.0; p.inner = 10.0;
    ArchOffsetGeom off = buildArchOffset(geom, p);
    EXPECT_NEAR(off.outer.circleL.radius, geom.circleL.radius + p.outer, 1e-12);
    EXPECT_NEAR(off.outer.circleR.radius, geom.circleR.radius + p.outer, 1e-12);
}

TEST(TEST_cgmath_architecture_gothic, OffsetInnerRadiusEqualsBaseMinusDelta)
{
    ArchGeom geom = buildArch(makeBasis(1.0));
    ArchOffsetParams p; p.outer = 15.0; p.inner = 10.0;
    ArchOffsetGeom off = buildArchOffset(geom, p);
    EXPECT_NEAR(off.inner.circleL.radius, geom.circleL.radius - p.inner, 1e-12);
    EXPECT_NEAR(off.inner.circleR.radius, geom.circleR.radius - p.inner, 1e-12);
}

TEST(TEST_cgmath_architecture_gothic, OffsetCentersAreUnchanged)
{
    ArchGeom geom = buildArch(makeBasis(1.0));
    ArchOffsetParams p; p.outer = 15.0; p.inner = 10.0;
    ArchOffsetGeom off = buildArchOffset(geom, p);

    EXPECT_DOUBLE_EQ(off.inner.circleL.center.x, geom.circleL.center.x);
    EXPECT_DOUBLE_EQ(off.inner.circleL.center.y, geom.circleL.center.y);
    EXPECT_DOUBLE_EQ(off.outer.circleL.center.x, geom.circleL.center.x);
    EXPECT_DOUBLE_EQ(off.outer.circleL.center.y, geom.circleL.center.y);

    EXPECT_DOUBLE_EQ(off.inner.circleR.center.x, geom.circleR.center.x);
    EXPECT_DOUBLE_EQ(off.inner.circleR.center.y, geom.circleR.center.y);
    EXPECT_DOUBLE_EQ(off.outer.circleR.center.x, geom.circleR.center.x);
    EXPECT_DOUBLE_EQ(off.outer.circleR.center.y, geom.circleR.center.y);
}

//
// Offset — sanity geometric checks
//

TEST(TEST_cgmath_architecture_gothic, OffsetWidthsAreUnchanged)
{
    ArchGeom geom = buildArch(makeBasis(1.0));
    ArchOffsetParams p; p.outer = 15.0; p.inner = 10.0;
    ArchOffsetGeom off = buildArchOffset(geom, p);
    EXPECT_NEAR(off.inner.width, geom.width, 1e-12);
    EXPECT_NEAR(off.outer.width, geom.width, 1e-12);
}

TEST(TEST_cgmath_architecture_gothic, OffsetApexStacking)
{
    ArchGeom geom = buildArch(makeBasis(1.0));
    ArchOffsetParams p; p.outer = 15.0; p.inner = 10.0;
    ArchOffsetGeom off = buildArchOffset(geom, p);
    EXPECT_GT(off.outer.apex.y, geom.apex.y);
    EXPECT_LT(off.inner.apex.y, geom.apex.y);
}

TEST(TEST_cgmath_architecture_gothic, OffsetApexOnSymmetryAxis)
{
    ArchGeom geom = buildArch(makeBasis(1.2));
    ArchOffsetParams p; p.outer = 12.0; p.inner = 8.0;
    ArchOffsetGeom off = buildArchOffset(geom, p);
    EXPECT_NEAR(off.inner.apex.x, 0.0, 1e-9);
    EXPECT_NEAR(off.outer.apex.x, 0.0, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, OffsetArcsCcw)
{
    ArchGeom geom = buildArch(makeBasis(1.0));
    ArchOffsetParams p; p.outer = 10.0; p.inner = 10.0;
    ArchOffsetGeom off = buildArchOffset(geom, p);
    EXPECT_TRUE(off.inner.arcLeft.ccw);
    EXPECT_TRUE(off.inner.arcRight.ccw);
    EXPECT_TRUE(off.outer.arcLeft.ccw);
    EXPECT_TRUE(off.outer.arcRight.ccw);
}

TEST(TEST_cgmath_architecture_gothic, OffsetStoneWidthActualVerticalBandAtApex)
{
    // For excess=1.0, d=200, outer=inner=10:
    //   inner.apex.y = sqrt(190^2 - 100^2) = sqrt(26100)
    //   outer.apex.y = sqrt(210^2 - 100^2) = sqrt(34100)
    //   stoneWidthActual = outer.apex.y - inner.apex.y
    ArchGeom geom = buildArch(makeBasis(1.0));
    ArchOffsetParams p; p.outer = 10.0; p.inner = 10.0;
    ArchOffsetGeom off = buildArchOffset(geom, p);

    double expectedInnerY = std::sqrt(190.0 * 190.0 - 100.0 * 100.0);
    double expectedOuterY = std::sqrt(210.0 * 210.0 - 100.0 * 100.0);
    EXPECT_NEAR(off.stoneWidthActual, expectedOuterY - expectedInnerY, 1e-9);

    // Sanity : differs from the radial sum (outer+inner = 20) by a few units.
    EXPECT_GT(off.stoneWidthActual, p.outer + p.inner);
}

TEST(TEST_cgmath_architecture_gothic, OffsetStoneWidthActualVariesWithExcess)
{
    ArchOffsetParams p; p.outer = 10.0; p.inner = 10.0;
    ArchOffsetGeom off10 = buildArchOffset(buildArch(makeBasis(1.0)), p);
    ArchOffsetGeom off15 = buildArchOffset(buildArch(makeBasis(1.5)), p);
    // Approaches outer+inner as excess grows; should differ noticeably.
    EXPECT_GT(std::fabs(off10.stoneWidthActual - off15.stoneWidthActual), 1.0);
}

//
// Offset — C1 continuity at foot
//

TEST(TEST_cgmath_architecture_gothic, OffsetTangentsAreParallelAtFoot)
{
    // At the right foot direction (start of arcLeft on circleL), the tangent
    // is purely vertical (perpendicular to the radial direction along x-axis)
    // and identical for inner / geom / outer since they share the same center.
    ArchGeom geom = buildArch(makeBasis(1.2));
    ArchOffsetParams p; p.outer = 10.0; p.inner = 10.0;
    ArchOffsetGeom off = buildArchOffset(geom, p);

    Vector2d tInner = off.inner.arcLeft.tangentAt(0.0);
    Vector2d tBase  = geom.arcLeft.tangentAt(0.0);
    Vector2d tOuter = off.outer.arcLeft.tangentAt(0.0);

    // Cross product of unit vectors == 0 iff parallel.
    EXPECT_NEAR(tInner.CrossProduct(tBase),  0.0, 1e-12);
    EXPECT_NEAR(tOuter.CrossProduct(tBase),  0.0, 1e-12);
}

//
// Offset — boundary cases
//

TEST(TEST_cgmath_architecture_gothic, OffsetZeroIsIdentity)
{
    ArchGeom geom = buildArch(makeBasis(1.0));
    ArchOffsetParams p;   // both 0 by default
    ArchOffsetGeom off = buildArchOffset(geom, p);

    EXPECT_NEAR(off.inner.apex.x, geom.apex.x, 1e-9);
    EXPECT_NEAR(off.inner.apex.y, geom.apex.y, 1e-9);
    EXPECT_NEAR(off.outer.apex.x, geom.apex.x, 1e-9);
    EXPECT_NEAR(off.outer.apex.y, geom.apex.y, 1e-9);
    EXPECT_DOUBLE_EQ(off.inner.circleL.radius, geom.circleL.radius);
    EXPECT_DOUBLE_EQ(off.outer.circleL.radius, geom.circleL.radius);
    EXPECT_NEAR(off.stoneWidthActual, 0.0, 1e-9);
}

//
// Offset — input validation
//

TEST(TEST_cgmath_architecture_gothic, OffsetInnerEqualsRadiusThrows)
{
    ArchGeom geom = buildArch(makeBasis(1.0));    // radius = 200
    ArchOffsetParams p; p.outer = 10.0; p.inner = geom.circleL.radius;
    EXPECT_THROW(buildArchOffset(geom, p), std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, OffsetInnerCollapsesArchThrows)
{
    // For excess=1.0, r=200, w=200: inner must be < r - w/2 = 100.
    ArchGeom geom = buildArch(makeBasis(1.0));
    ArchOffsetParams p; p.outer = 0.0; p.inner = 100.0;   // exactly the boundary
    EXPECT_THROW(buildArchOffset(geom, p), std::invalid_argument);

    p.inner = 110.0;                                       // beyond the boundary
    EXPECT_THROW(buildArchOffset(geom, p), std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, OffsetNegativeOuterThrows)
{
    ArchGeom geom = buildArch(makeBasis(1.0));
    ArchOffsetParams p; p.outer = -1.0; p.inner = 10.0;
    EXPECT_THROW(buildArchOffset(geom, p), std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, OffsetNegativeInnerThrows)
{
    ArchGeom geom = buildArch(makeBasis(1.0));
    ArchOffsetParams p; p.outer = 10.0; p.inner = -1.0;
    EXPECT_THROW(buildArchOffset(geom, p), std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, OffsetNonFiniteThrows)
{
    ArchGeom geom = buildArch(makeBasis(1.0));
    ArchOffsetParams p; p.outer = 10.0; p.inner = 10.0;

    p.outer = std::numeric_limits<double>::infinity();
    EXPECT_THROW(buildArchOffset(geom, p), std::invalid_argument);
    p.outer = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(buildArchOffset(geom, p), std::invalid_argument);

    // Reset outer, corrupt inner.
    p.outer = 10.0;
    p.inner = std::numeric_limits<double>::infinity();
    EXPECT_THROW(buildArchOffset(geom, p), std::invalid_argument);
    p.inner = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(buildArchOffset(geom, p), std::invalid_argument);
}

//
// Subwindows (A3)
//

namespace
{
    SubwindowParams makeSubParamsFraction(int count, double drop, double excess,
                                           double gapFraction)
    {
        SubwindowParams p;
        p.count  = count;
        p.drop   = drop;
        p.excess = excess;
        p.gap.mode        = SubwindowParams::Gap::Mode::Fraction;
        p.gap.gapFraction = gapFraction;
        return p;
    }

    ArchOffsetParams smallOffset()
    {
        ArchOffsetParams p;
        p.outer = 5.0;
        p.inner = 5.0;
        return p;
    }
}

//
// Subwindows — spec tests
//

TEST(TEST_cgmath_architecture_gothic, SubwindowsSymmetryTwoLancets)
{
    ArchGeom main = buildArch(makeBasis(1.0));    // width 200, centered on x=0
    SubwindowsGeom sw = buildSubwindows(
        main, smallOffset(),
        makeSubParamsFraction(2, 0.0, 1.0, 0.05));

    ASSERT_EQ(sw.lancets.size(), 2u);
    double midX = (sw.lancets[0].center.x + sw.lancets[1].center.x) / 2.0;
    EXPECT_NEAR(midX, 0.0, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, SubwindowsTotalWidthSums)
{
    ArchGeom main = buildArch(makeBasis(1.0));
    SubwindowsGeom sw = buildSubwindows(
        main, smallOffset(),
        makeSubParamsFraction(3, 0.0, 1.0, 0.05));

    int n = (int)sw.lancets.size();
    double sum = 0.0;
    for (const auto &l : sw.lancets) sum += l.subWidth;
    sum += (n + 1) * sw.gapWidth;
    EXPECT_NEAR(sum, main.width, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, SubwindowsSingleLancetSpansFullWidthMinusTwoGaps)
{
    ArchGeom main = buildArch(makeBasis(1.0));
    SubwindowsGeom sw = buildSubwindows(
        main, smallOffset(),
        makeSubParamsFraction(1, 0.0, 1.0, 0.1));

    ASSERT_EQ(sw.lancets.size(), 1u);
    EXPECT_NEAR(sw.lancets[0].subWidth, main.width - 2.0 * sw.gapWidth, 1e-9);
    // gap width = 0.1 * 200 = 20, so subWidth = 200 - 40 = 160.
    EXPECT_NEAR(sw.gapWidth, 20.0, 1e-9);
    EXPECT_NEAR(sw.lancets[0].subWidth, 160.0, 1e-9);
}

//
// Subwindows — geometric coherence
//

TEST(TEST_cgmath_architecture_gothic, SubwindowsLancetCount)
{
    ArchGeom main = buildArch(makeBasis(1.0));
    for (int n = 1; n <= 6; ++n)
    {
        SubwindowsGeom sw = buildSubwindows(
            main, smallOffset(),
            makeSubParamsFraction(n, 0.0, 1.0, 0.04));
        EXPECT_EQ((int)sw.lancets.size(), n);
    }
}

TEST(TEST_cgmath_architecture_gothic, SubwindowsGapWidthFractionMode)
{
    ArchGeom main = buildArch(makeBasis(1.0));
    SubwindowsGeom sw = buildSubwindows(
        main, smallOffset(),
        makeSubParamsFraction(2, 0.0, 1.0, 0.07));
    EXPECT_NEAR(sw.gapWidth, 0.07 * main.width, 1e-12);
}

TEST(TEST_cgmath_architecture_gothic, SubwindowsGapWidthAbsoluteMode)
{
    ArchGeom main = buildArch(makeBasis(1.0));
    SubwindowParams p;
    p.count = 2; p.drop = 0.0; p.excess = 1.0;
    p.gap.mode = SubwindowParams::Gap::Mode::Absolute;
    p.gap.absoluteWidth = 25.0;
    SubwindowsGeom sw = buildSubwindows(main, smallOffset(), p);
    EXPECT_NEAR(sw.gapWidth, 25.0, 1e-12);
}

TEST(TEST_cgmath_architecture_gothic, SubwindowsAllLancetsSameSubWidth)
{
    ArchGeom main = buildArch(makeBasis(1.0));
    SubwindowsGeom sw = buildSubwindows(
        main, smallOffset(),
        makeSubParamsFraction(4, 0.0, 1.0, 0.04));
    for (int i = 1; i < (int)sw.lancets.size(); ++i)
        EXPECT_NEAR(sw.lancets[i].subWidth, sw.lancets[0].subWidth, 1e-12);
}

TEST(TEST_cgmath_architecture_gothic, SubwindowsLancetBaseDropped)
{
    ArchGeom main = buildArch(makeBasis(1.0));
    double drop = 12.5;
    SubwindowsGeom sw = buildSubwindows(
        main, smallOffset(),
        makeSubParamsFraction(2, drop, 1.0, 0.05));
    for (const auto &l : sw.lancets)
    {
        EXPECT_NEAR(l.basis.pL.y, main.circleL.center.y - drop, 1e-12);
        EXPECT_NEAR(l.basis.pR.y, main.circleL.center.y - drop, 1e-12);
        EXPECT_NEAR(l.center.y,   main.circleL.center.y - drop, 1e-12);
    }
}

TEST(TEST_cgmath_architecture_gothic, SubwindowsLancetExcessApplied)
{
    ArchGeom main = buildArch(makeBasis(1.0));
    double excess = 1.3;
    SubwindowsGeom sw = buildSubwindows(
        main, smallOffset(),
        makeSubParamsFraction(2, 0.0, excess, 0.05));
    for (const auto &l : sw.lancets)
    {
        EXPECT_NEAR(l.basis.excess, excess, 1e-12);
        EXPECT_NEAR(l.arch.circleL.radius, excess * l.subWidth, 1e-9);
    }
}

TEST(TEST_cgmath_architecture_gothic, SubwindowsLancetOffsetApplied)
{
    ArchGeom main = buildArch(makeBasis(1.0));
    ArchOffsetParams op; op.outer = 4.0; op.inner = 3.0;
    SubwindowsGeom sw = buildSubwindows(
        main, op,
        makeSubParamsFraction(2, 0.0, 1.2, 0.05));
    for (const auto &l : sw.lancets)
    {
        EXPECT_NEAR(l.offset.outer.circleL.radius, l.arch.circleL.radius + op.outer, 1e-9);
        EXPECT_NEAR(l.offset.inner.circleL.radius, l.arch.circleL.radius - op.inner, 1e-9);
    }
}

TEST(TEST_cgmath_architecture_gothic, SubwindowsLancetCenterIsBaseMidpoint)
{
    ArchGeom main = buildArch(makeBasis(1.0));
    SubwindowsGeom sw = buildSubwindows(
        main, smallOffset(),
        makeSubParamsFraction(3, 0.0, 1.0, 0.04));
    for (const auto &l : sw.lancets)
    {
        EXPECT_NEAR(l.center.x, (l.basis.pL.x + l.basis.pR.x) / 2.0, 1e-12);
        EXPECT_NEAR(l.center.y, l.basis.pL.y, 1e-12);
    }
}

//
// Subwindows — symmetry & ordering
//

TEST(TEST_cgmath_architecture_gothic, SubwindowsThreeLancetMiddleCentered)
{
    ArchGeom main = buildArch(makeBasis(1.0));    // centered on x=0
    SubwindowsGeom sw = buildSubwindows(
        main, smallOffset(),
        makeSubParamsFraction(3, 0.0, 1.0, 0.04));
    ASSERT_EQ(sw.lancets.size(), 3u);
    EXPECT_NEAR(sw.lancets[1].center.x, 0.0, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, SubwindowsLancetsOrderedLeftToRight)
{
    ArchGeom main = buildArch(makeBasis(1.0));
    SubwindowsGeom sw = buildSubwindows(
        main, smallOffset(),
        makeSubParamsFraction(4, 0.0, 1.0, 0.04));
    for (int i = 1; i < (int)sw.lancets.size(); ++i)
        EXPECT_LT(sw.lancets[i - 1].basis.pL.x, sw.lancets[i].basis.pL.x);
}

//
// Subwindows — input validation
//

TEST(TEST_cgmath_architecture_gothic, SubwindowsCountOutOfRangeThrows)
{
    ArchGeom main = buildArch(makeBasis(1.0));
    EXPECT_THROW(buildSubwindows(main, smallOffset(),
                                 makeSubParamsFraction(0, 0.0, 1.0, 0.05)),
                 std::invalid_argument);
    EXPECT_THROW(buildSubwindows(main, smallOffset(),
                                 makeSubParamsFraction(7, 0.0, 1.0, 0.05)),
                 std::invalid_argument);
    EXPECT_THROW(buildSubwindows(main, smallOffset(),
                                 makeSubParamsFraction(-1, 0.0, 1.0, 0.05)),
                 std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, SubwindowsExcessTooSmallThrows)
{
    ArchGeom main = buildArch(makeBasis(1.0));
    EXPECT_THROW(buildSubwindows(main, smallOffset(),
                                 makeSubParamsFraction(2, 0.0, 0.4, 0.05)),
                 std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, SubwindowsDropInvalidThrows)
{
    ArchGeom main = buildArch(makeBasis(1.0));

    EXPECT_THROW(buildSubwindows(main, smallOffset(),
                                 makeSubParamsFraction(2, -1.0, 1.0, 0.05)),
                 std::invalid_argument);
    EXPECT_THROW(buildSubwindows(main, smallOffset(),
                                 makeSubParamsFraction(2,
                                     std::numeric_limits<double>::infinity(),
                                     1.0, 0.05)),
                 std::invalid_argument);
    EXPECT_THROW(buildSubwindows(main, smallOffset(),
                                 makeSubParamsFraction(2,
                                     std::numeric_limits<double>::quiet_NaN(),
                                     1.0, 0.05)),
                 std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, SubwindowsGapFractionInvalidThrows)
{
    ArchGeom main = buildArch(makeBasis(1.0));

    // <= 0
    EXPECT_THROW(buildSubwindows(main, smallOffset(),
                                 makeSubParamsFraction(2, 0.0, 1.0, 0.0)),
                 std::invalid_argument);
    EXPECT_THROW(buildSubwindows(main, smallOffset(),
                                 makeSubParamsFraction(2, 0.0, 1.0, -0.01)),
                 std::invalid_argument);

    // Gaps consume full width: 3 * 0.34 > 1 (count=2 -> 3 gaps).
    EXPECT_THROW(buildSubwindows(main, smallOffset(),
                                 makeSubParamsFraction(2, 0.0, 1.0, 0.4)),
                 std::invalid_argument);

    // Non-finite
    EXPECT_THROW(buildSubwindows(main, smallOffset(),
                                 makeSubParamsFraction(2, 0.0, 1.0,
                                     std::numeric_limits<double>::quiet_NaN())),
                 std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, SubwindowsGapAbsoluteInvalidThrows)
{
    ArchGeom main = buildArch(makeBasis(1.0));
    SubwindowParams base;
    base.count = 2; base.drop = 0.0; base.excess = 1.0;
    base.gap.mode = SubwindowParams::Gap::Mode::Absolute;

    base.gap.absoluteWidth = 0.0;
    EXPECT_THROW(buildSubwindows(main, smallOffset(), base), std::invalid_argument);
    base.gap.absoluteWidth = -5.0;
    EXPECT_THROW(buildSubwindows(main, smallOffset(), base), std::invalid_argument);

    // Too large : 3 * 80 = 240 > main.width = 200.
    base.gap.absoluteWidth = 80.0;
    EXPECT_THROW(buildSubwindows(main, smallOffset(), base), std::invalid_argument);

    base.gap.absoluteWidth = std::numeric_limits<double>::infinity();
    EXPECT_THROW(buildSubwindows(main, smallOffset(), base), std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, SubwindowsOffsetParamsInvalidThrows)
{
    ArchGeom main = buildArch(makeBasis(1.0));
    SubwindowParams sp = makeSubParamsFraction(2, 0.0, 1.0, 0.05);

    ArchOffsetParams op;
    op.outer = -1.0; op.inner = 5.0;
    EXPECT_THROW(buildSubwindows(main, op, sp), std::invalid_argument);
    op.outer = 5.0; op.inner = -1.0;
    EXPECT_THROW(buildSubwindows(main, op, sp), std::invalid_argument);
    op.outer = std::numeric_limits<double>::infinity(); op.inner = 5.0;
    EXPECT_THROW(buildSubwindows(main, op, sp), std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, SubwindowsOffsetTooLargeForSubarchThrows)
{
    // Main arch width = 200, n=4 -> subWidth ~= 36 (with gapFraction=0.04).
    // With excess=1.0, sub-arch radius ~= 36. inner = 30 collapses the sub-arch
    // (30 > 36 - 18 = 18). Error propagates from buildArchOffset.
    ArchGeom main = buildArch(makeBasis(1.0));
    ArchOffsetParams op; op.outer = 5.0; op.inner = 30.0;
    EXPECT_THROW(buildSubwindows(main, op,
                                 makeSubParamsFraction(4, 0.0, 1.0, 0.04)),
                 std::invalid_argument);
}

//
// Rosette (A4)
//

namespace
{
    // Builds a "typical" main + subwindows configuration suitable for rosette
    // construction. Two lancets, modest offset, drop=0 so the geometry is easy
    // to reason about.
    SubwindowsGeom typicalSubwindows(const ArchGeom &main, int count = 2,
                                      double drop = 0.0, double excess = 1.0)
    {
        return buildSubwindows(main, smallOffset(),
                               makeSubParamsFraction(count, drop, excess, 0.05));
    }
}

//
// Rosette — spec tests : tangency relations
//

TEST(TEST_cgmath_architecture_gothic, RosetteTangentInternalToMainArc)
{
    ArchGeom main = buildArch(makeBasis(1.2));
    SubwindowsGeom sw = typicalSubwindows(main);
    RosetteGeom r = buildRosette(main, sw);

    Vector2d F1 = main.circleR.center;
    double rMain = main.circleR.radius;
    // Internal tangency : dist(mC, F1) + r_ros == r_main
    EXPECT_NEAR(Vector2d::Distance(r.center, F1) + r.radius, rMain, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, RosetteTangentExternalToSubArc)
{
    ArchGeom main = buildArch(makeBasis(1.2));
    SubwindowsGeom sw = typicalSubwindows(main);
    RosetteGeom r = buildRosette(main, sw);

    const LancetGeom &right = sw.lancets.back();
    Vector2d F2 = right.arch.circleL.center;
    double rSub = right.arch.circleL.radius;
    // External tangency : dist(mC, F2) - r_sub == r_ros
    EXPECT_NEAR(Vector2d::Distance(r.center, F2) - rSub, r.radius, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, RosetteCenterOnSymmetryAxis)
{
    ArchGeom main = buildArch(makeBasis(1.2));
    SubwindowsGeom sw = typicalSubwindows(main);
    RosetteGeom r = buildRosette(main, sw);

    double midX = (main.circleL.center.x + main.circleR.center.x) / 2.0;
    EXPECT_NEAR(r.center.x, midX, 1e-9);
}

//
// Rosette — geometric coherence
//

TEST(TEST_cgmath_architecture_gothic, RosetteRadiusPositive)
{
    ArchGeom main = buildArch(makeBasis(1.2));
    SubwindowsGeom sw = typicalSubwindows(main);
    RosetteGeom r = buildRosette(main, sw);
    EXPECT_GT(r.radius, 0.0);
}

TEST(TEST_cgmath_architecture_gothic, RosetteCenterAboveSubarchApex)
{
    ArchGeom main = buildArch(makeBasis(1.2));
    SubwindowsGeom sw = typicalSubwindows(main);
    RosetteGeom r = buildRosette(main, sw);
    EXPECT_GT(r.center.y, sw.lancets.back().arch.apex.y);
}

TEST(TEST_cgmath_architecture_gothic, RosetteCenterBelowMainApex)
{
    ArchGeom main = buildArch(makeBasis(1.2));
    SubwindowsGeom sw = typicalSubwindows(main);
    RosetteGeom r = buildRosette(main, sw);
    EXPECT_LT(r.center.y, main.apex.y);
}

TEST(TEST_cgmath_architecture_gothic, RosetteCircleConsistentWithCenterRadius)
{
    ArchGeom main = buildArch(makeBasis(1.2));
    SubwindowsGeom sw = typicalSubwindows(main);
    RosetteGeom r = buildRosette(main, sw);
    EXPECT_DOUBLE_EQ(r.circle.center.x, r.center.x);
    EXPECT_DOUBLE_EQ(r.circle.center.y, r.center.y);
    EXPECT_DOUBLE_EQ(r.circle.radius, r.radius);
}

TEST(TEST_cgmath_architecture_gothic, RosetteFocalSumIsMainPlusSubRadii)
{
    ArchGeom main = buildArch(makeBasis(1.2));
    SubwindowsGeom sw = typicalSubwindows(main);
    RosetteGeom r = buildRosette(main, sw);

    Vector2d F1 = main.circleR.center;
    Vector2d F2 = sw.lancets.back().arch.circleL.center;
    double expectedSum = main.circleR.radius + sw.lancets.back().arch.circleL.radius;

    double d1 = Vector2d::Distance(r.center, F1);
    double d2 = Vector2d::Distance(r.center, F2);
    EXPECT_NEAR(d1 + d2, expectedSum, 1e-9);
}

//
// Rosette — typical configurations
//

TEST(TEST_cgmath_architecture_gothic, RosetteWithSingleLancet)
{
    ArchGeom main = buildArch(makeBasis(1.2));
    SubwindowsGeom sw = typicalSubwindows(main, 1);
    RosetteGeom r = buildRosette(main, sw);
    EXPECT_GT(r.radius, 0.0);
    EXPECT_NEAR(r.center.x, 0.0, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, RosetteWithTwoLancets)
{
    ArchGeom main = buildArch(makeBasis(1.2));
    SubwindowsGeom sw = typicalSubwindows(main, 2);
    RosetteGeom r = buildRosette(main, sw);
    EXPECT_GT(r.radius, 0.0);
}

TEST(TEST_cgmath_architecture_gothic, RosetteWithThreeLancets)
{
    // Middle lancet exists but is unused for the rosette construction.
    ArchGeom main = buildArch(makeBasis(1.2));
    SubwindowsGeom sw = typicalSubwindows(main, 3);
    RosetteGeom r = buildRosette(main, sw);
    EXPECT_GT(r.radius, 0.0);
    EXPECT_NEAR(r.center.x, 0.0, 1e-9);
}

//
// Rosette — translation invariance
//

TEST(TEST_cgmath_architecture_gothic, RosetteTranslatedBasisProducesTranslatedRosette)
{
    ArchGeom mainBase = buildArch(makeBasis(1.2));
    SubwindowsGeom swBase = typicalSubwindows(mainBase);
    RosetteGeom rBase = buildRosette(mainBase, swBase);

    Vector2d offset(50.0, 30.0);
    ArchBasis trBasis = makeBasis(1.2);
    trBasis.pL = trBasis.pL + offset;
    trBasis.pR = trBasis.pR + offset;
    ArchGeom mainTr = buildArch(trBasis);
    SubwindowsGeom swTr = typicalSubwindows(mainTr);
    RosetteGeom rTr = buildRosette(mainTr, swTr);

    EXPECT_NEAR(rTr.center.x, rBase.center.x + offset.x, 1e-9);
    EXPECT_NEAR(rTr.center.y, rBase.center.y + offset.y, 1e-9);
    EXPECT_NEAR(rTr.radius,   rBase.radius,              1e-9);
}

//
// Rosette — input validation
//

TEST(TEST_cgmath_architecture_gothic, RosetteEmptySubwindowsThrows)
{
    ArchGeom main = buildArch(makeBasis(1.2));
    SubwindowsGeom emptySw;   // no lancets
    EXPECT_THROW(buildRosette(main, emptySw), std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, RosetteApexAboveMainApexThrows)
{
    // Hand-craft a SubwindowsGeom whose lone "lancet" has an apex strictly above
    // the main arch apex. The candidate selection window [sub_apex.y, main_apex.y]
    // is then inverted, so no ellipse intersection can fit -> runtime_error.
    ArchGeom main = buildArch(makeBasis(1.0));   // main apex y ~= 173

    SubwindowsGeom sw;
    LancetGeom l;
    l.arch = buildArch(makeBasis(2.0));          // apex y ~= 387 > main apex
    sw.lancets.push_back(l);

    EXPECT_THROW(buildRosette(main, sw), std::runtime_error);
}

//
// Foils (A5)
//

namespace
{
    FoilsParams makeRoundFoilParams(int count, double phi0 = 0.0,
                                     bool orientLying = false)
    {
        FoilsParams p;
        p.count = count;
        p.type  = FoilType::Round;
        p.phi0  = phi0;
        p.orientLying = orientLying;
        return p;
    }

    FoilsParams makePointedFoilParams(int count, double pointedness,
                                       double phi0 = 0.0)
    {
        FoilsParams p;
        p.count       = count;
        p.type        = FoilType::Pointed;
        p.pointedness = pointedness;
        p.phi0        = phi0;
        return p;
    }
}

//
// Round foils
//

TEST(TEST_cgmath_architecture_gothic, FoilsRoundCount)
{
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    FoilRing r = buildFoilRing(outer, makeRoundFoilParams(6));
    EXPECT_EQ(r.foils.size(), 6u);
    for (const auto &f : r.foils)
        EXPECT_TRUE(std::holds_alternative<RoundFoil>(f));
}

TEST(TEST_cgmath_architecture_gothic, FoilsRoundRadiusFormulaN6)
{
    // n=6 -> sin(pi/6) = 0.5 -> fr = 0.5 / 1.5 * R = R/3
    Circle outer(Vector2d(0.0, 0.0), 9.0);
    FoilRing r = buildFoilRing(outer, makeRoundFoilParams(6));
    EXPECT_NEAR(r.foilRadius, 9.0 / 3.0, 1e-12);
}

TEST(TEST_cgmath_architecture_gothic, FoilsRoundRadiusFormulaN4)
{
    // n=4 -> sin(pi/4) = sqrt(2)/2 -> fr = (sqrt(2)/2) / (1 + sqrt(2)/2) * R
    //                                    = sqrt(2) / (2 + sqrt(2)) * R
    //                                    = (sqrt(2) - 1) * R     after rationalization
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    FoilRing r = buildFoilRing(outer, makeRoundFoilParams(4));
    EXPECT_NEAR(r.foilRadius, std::sqrt(2.0) - 1.0, 1e-12);
}

TEST(TEST_cgmath_architecture_gothic, FoilsRoundCentersAtDistanceRm)
{
    Circle outer(Vector2d(2.0, -1.0), 6.0);
    FoilRing r = buildFoilRing(outer, makeRoundFoilParams(6));
    double Rm = outer.radius - r.foilRadius;
    for (const auto &f : r.foils)
    {
        const Circle &c = std::get<RoundFoil>(f).circle;
        EXPECT_NEAR(Vector2d::Distance(c.center, outer.center), Rm, 1e-9);
    }
}

TEST(TEST_cgmath_architecture_gothic, FoilsRoundTangentToOuterCircle)
{
    Circle outer(Vector2d(0.0, 0.0), 5.0);
    FoilRing r = buildFoilRing(outer, makeRoundFoilParams(8));
    for (const auto &f : r.foils)
    {
        const Circle &c = std::get<RoundFoil>(f).circle;
        EXPECT_NEAR(Vector2d::Distance(c.center, outer.center) + c.radius,
                    outer.radius, 1e-9);
    }
}

TEST(TEST_cgmath_architecture_gothic, FoilsRoundAdjacentFoilsTangent)
{
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    FoilRing r = buildFoilRing(outer, makeRoundFoilParams(6));
    int n = (int)r.foils.size();
    for (int i = 0; i < n; ++i)
    {
        const Circle &c1 = std::get<RoundFoil>(r.foils[i]).circle;
        const Circle &c2 = std::get<RoundFoil>(r.foils[(i + 1) % n]).circle;
        EXPECT_NEAR(Vector2d::Distance(c1.center, c2.center),
                    2.0 * r.foilRadius, 1e-9);
    }
}

//
// Pointed foils
//

TEST(TEST_cgmath_architecture_gothic, FoilsPointedAllVariantsArePointed)
{
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    FoilRing r = buildFoilRing(outer, makePointedFoilParams(6, 0.5));
    EXPECT_EQ(r.foils.size(), 6u);
    for (const auto &f : r.foils)
        EXPECT_TRUE(std::holds_alternative<PointedFoil>(f));
}

TEST(TEST_cgmath_architecture_gothic, FoilsPointedBasesOnOuterCircle)
{
    Circle outer(Vector2d(2.0, -1.0), 5.0);
    FoilRing r = buildFoilRing(outer, makePointedFoilParams(6, 0.5));
    for (const auto &f : r.foils)
    {
        const PointedFoil &pf = std::get<PointedFoil>(f);
        EXPECT_TRUE(outer.contains(pf.baseLeft, 1e-9));
        EXPECT_TRUE(outer.contains(pf.baseRight, 1e-9));
    }
}

TEST(TEST_cgmath_architecture_gothic, FoilsPointedApexCloserToCenterThanM)
{
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    FoilRing r = buildFoilRing(outer, makePointedFoilParams(6, 0.5));
    double Rm = outer.radius - r.foilRadius;
    for (const auto &f : r.foils)
    {
        const PointedFoil &pf = std::get<PointedFoil>(f);
        double dApex = Vector2d::Distance(pf.apex, outer.center);
        EXPECT_LT(dApex, Rm);
    }
}

TEST(TEST_cgmath_architecture_gothic, FoilsPointedApexOnSymmetryAxis)
{
    // For each foil i, apex must lie on the radial line from outer.center
    // through m_i. Equivalently, cross((apex - cc), m_i - cc) ~= 0 where m_i
    // points along the foil's direction.
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    int n = 6;
    FoilRing r = buildFoilRing(outer, makePointedFoilParams(n, 0.5));
    double Rm = outer.radius - r.foilRadius;

    for (int i = 0; i < n; ++i)
    {
        const PointedFoil &pf = std::get<PointedFoil>(r.foils[i]);
        double thetaI = i * (2.0 * std::atan2(0.0, -1.0) / n);   // i*alpha, phi0=0
        Vector2d mI(outer.center.x + Rm * std::cos(thetaI),
                    outer.center.y + Rm * std::sin(thetaI));
        Vector2d a = Vector2d(pf.apex.x - outer.center.x, pf.apex.y - outer.center.y);
        Vector2d b = Vector2d(mI.x - outer.center.x, mI.y - outer.center.y);
        double cross = a.x * b.y - a.y * b.x;
        EXPECT_NEAR(cross, 0.0, 1e-9);
    }
}

TEST(TEST_cgmath_architecture_gothic, FoilsPointedArcEndpoints)
{
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    FoilRing r = buildFoilRing(outer, makePointedFoilParams(6, 0.5));
    for (const auto &f : r.foils)
    {
        const PointedFoil &pf = std::get<PointedFoil>(f);
        Vector2d L0 = pf.arcLeft.pointAt(0.0);
        Vector2d L1 = pf.arcLeft.pointAt(1.0);
        Vector2d R0 = pf.arcRight.pointAt(0.0);
        Vector2d R1 = pf.arcRight.pointAt(1.0);
        EXPECT_NEAR(L0.x, pf.baseLeft.x, 1e-9);
        EXPECT_NEAR(L0.y, pf.baseLeft.y, 1e-9);
        EXPECT_NEAR(L1.x, pf.apex.x, 1e-9);
        EXPECT_NEAR(L1.y, pf.apex.y, 1e-9);
        EXPECT_NEAR(R0.x, pf.apex.x, 1e-9);
        EXPECT_NEAR(R0.y, pf.apex.y, 1e-9);
        EXPECT_NEAR(R1.x, pf.baseRight.x, 1e-9);
        EXPECT_NEAR(R1.y, pf.baseRight.y, 1e-9);
    }
}

TEST(TEST_cgmath_architecture_gothic, FoilsPointedArcsLieOnTheirCircles)
{
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    FoilRing r = buildFoilRing(outer, makePointedFoilParams(6, 0.5));
    for (const auto &f : r.foils)
    {
        const PointedFoil &pf = std::get<PointedFoil>(f);
        EXPECT_TRUE(pf.circleLeft.contains(pf.baseLeft, 1e-9));
        EXPECT_TRUE(pf.circleLeft.contains(pf.apex,     1e-9));
        EXPECT_TRUE(pf.circleRight.contains(pf.baseRight, 1e-9));
        EXPECT_TRUE(pf.circleRight.contains(pf.apex,      1e-9));
    }
}

//
// phi0 / orientLying
//

TEST(TEST_cgmath_architecture_gothic, FoilsPhi0RotatesRing)
{
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    FoilRing r0 = buildFoilRing(outer, makeRoundFoilParams(6, 0.0));
    double shift = std::atan2(0.0, -1.0) / 6.0;          // pi/6 (half a step)
    FoilRing r1 = buildFoilRing(outer, makeRoundFoilParams(6, shift));

    // First foil of r1 is rotated by `shift` from first foil of r0.
    Vector2d c0 = std::get<RoundFoil>(r0.foils[0]).circle.center;
    Vector2d c1 = std::get<RoundFoil>(r1.foils[0]).circle.center;
    double a0 = std::atan2(c0.y, c0.x);
    double a1 = std::atan2(c1.y, c1.x);
    EXPECT_NEAR(a1 - a0, shift, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, FoilsLyingShiftsByPiOverN)
{
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    FoilRing rStanding = buildFoilRing(outer, makeRoundFoilParams(6, 0.0, false));
    FoilRing rLying    = buildFoilRing(outer, makeRoundFoilParams(6, 0.0, true));

    Vector2d cs = std::get<RoundFoil>(rStanding.foils[0]).circle.center;
    Vector2d cl = std::get<RoundFoil>(rLying.foils[0]).circle.center;
    double aS = std::atan2(cs.y, cs.x);
    double aL = std::atan2(cl.y, cl.x);
    double piOverN = std::atan2(0.0, -1.0) / 6.0;
    EXPECT_NEAR(aL - aS, piOverN, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, FoilsRoundIgnoresPointednessSilently)
{
    // Round type with non-zero pointedness should still build successfully :
    // pointedness is silently ignored.
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    FoilsParams p;
    p.count = 6; p.type = FoilType::Round;
    p.pointedness = 1.5;   // ignored for Round
    EXPECT_NO_THROW(buildFoilRing(outer, p));
    FoilRing r = buildFoilRing(outer, p);
    EXPECT_EQ(r.foils.size(), 6u);
    EXPECT_TRUE(std::holds_alternative<RoundFoil>(r.foils[0]));
}

//
// Validation
//

TEST(TEST_cgmath_architecture_gothic, FoilsCountTooSmallThrows)
{
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    EXPECT_THROW(buildFoilRing(outer, makeRoundFoilParams(2)),
                 std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, FoilsCountTooLargeThrows)
{
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    EXPECT_THROW(buildFoilRing(outer, makeRoundFoilParams(25)),
                 std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, FoilsPointedZeroPointednessThrows)
{
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    EXPECT_THROW(buildFoilRing(outer, makePointedFoilParams(6, 0.0)),
                 std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, FoilsPointedNegativePointednessThrows)
{
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    EXPECT_THROW(buildFoilRing(outer, makePointedFoilParams(6, -0.5)),
                 std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, FoilsPointedPointednessAboveSchemaMaxThrows)
{
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    EXPECT_THROW(buildFoilRing(outer, makePointedFoilParams(6, 2.5)),
                 std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, FoilsPointedPointednessExceedsGeometricLimitThrows)
{
    // For n=6, |a_i - m| / fr ~= 1.61. Pointedness 1.7 should hit the
    // geometric upper bound (disp >= |a_i - m|) -> invalid_argument.
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    EXPECT_THROW(buildFoilRing(outer, makePointedFoilParams(6, 1.7)),
                 std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, FoilsOuterRadiusNonPositiveThrows)
{
    EXPECT_THROW(buildFoilRing(Circle(Vector2d(0,0),  0.0), makeRoundFoilParams(6)),
                 std::invalid_argument);
    EXPECT_THROW(buildFoilRing(Circle(Vector2d(0,0), -1.0), makeRoundFoilParams(6)),
                 std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, FoilsPhi0NonFiniteThrows)
{
    Circle outer(Vector2d(0.0, 0.0), 1.0);
    FoilsParams p = makeRoundFoilParams(6);
    p.phi0 = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(buildFoilRing(outer, p), std::invalid_argument);
    p.phi0 = std::numeric_limits<double>::infinity();
    EXPECT_THROW(buildFoilRing(outer, p), std::invalid_argument);
}

//
// inscribedCircleOfPointedArch
//

TEST(TEST_cgmath_architecture_gothic, InscribedCircleEquilateralFormula)
{
    // Equilateral arch (excess=1, w=200, r=200) -> rk = 3w/8 = 75.
    ArchGeom a = buildArch(makeBasis(1.0));
    Circle ic = inscribedCircleOfPointedArch(a);
    EXPECT_NEAR(ic.radius, 75.0, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, InscribedCircleCenterOnSymmetryAxis)
{
    ArchGeom a = buildArch(makeBasis(1.2));
    Circle ic = inscribedCircleOfPointedArch(a);
    double midX = (a.circleL.center.x + a.circleR.center.x) / 2.0;
    EXPECT_NEAR(ic.center.x, midX, 1e-12);
}

TEST(TEST_cgmath_architecture_gothic, InscribedCircleTangentToBase)
{
    // Lowest point of inscribed circle == foot-line y0.
    ArchGeom a = buildArch(makeBasis(1.0));
    Circle ic = inscribedCircleOfPointedArch(a);
    EXPECT_NEAR(ic.center.y - ic.radius, a.circleL.center.y, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, InscribedCircleTangentToFootCircles)
{
    // Internal tangency to both foot circles: dist(center, footCenter) + rk == r.
    ArchGeom a = buildArch(makeBasis(1.2));
    Circle ic = inscribedCircleOfPointedArch(a);
    double d_to_L = Vector2d::Distance(ic.center, a.circleL.center);
    double d_to_R = Vector2d::Distance(ic.center, a.circleR.center);
    EXPECT_NEAR(d_to_L + ic.radius, a.circleL.radius, 1e-9);
    EXPECT_NEAR(d_to_R + ic.radius, a.circleR.radius, 1e-9);
}

//
// Trefoil arch (A6)
//

namespace
{
    TrefoilArchGeom makeTrefoil(double excess, double splitT, double factor)
    {
        ArchGeom a = buildArch(makeBasis(excess));
        TrefoilParams p;
        p.splitParameter   = splitT;
        p.foilRadiusFactor = factor;
        return buildTrefoilArch(a, p);
    }
}

//
// Spec tests
//

TEST(TEST_cgmath_architecture_gothic, TrefoilUpperLeftStartsAtSplitL)
{
    TrefoilArchGeom t = makeTrefoil(1.0, 0.45, 0.30);
    Vector2d p0 = t.upperLeft.pointAt(0.0);
    EXPECT_NEAR(p0.x, t.foilLeft.splitPoint.x, 1e-9);
    EXPECT_NEAR(p0.y, t.foilLeft.splitPoint.y, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, TrefoilUpperLeftEndsAtApex)
{
    TrefoilArchGeom t = makeTrefoil(1.0, 0.45, 0.30);
    Vector2d p1 = t.upperLeft.pointAt(1.0);
    EXPECT_NEAR(p1.x, t.base.apex.x, 1e-9);
    EXPECT_NEAR(p1.y, t.base.apex.y, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, TrefoilUpperRightStartsAtApex)
{
    TrefoilArchGeom t = makeTrefoil(1.0, 0.45, 0.30);
    Vector2d p0 = t.upperRight.pointAt(0.0);
    EXPECT_NEAR(p0.x, t.base.apex.x, 1e-9);
    EXPECT_NEAR(p0.y, t.base.apex.y, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, TrefoilUpperRightEndsAtSplitR)
{
    TrefoilArchGeom t = makeTrefoil(1.0, 0.45, 0.30);
    Vector2d p1 = t.upperRight.pointAt(1.0);
    EXPECT_NEAR(p1.x, t.foilRight.splitPoint.x, 1e-9);
    EXPECT_NEAR(p1.y, t.foilRight.splitPoint.y, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, TrefoilFoilRadiusEqualsFactorTimesRadius)
{
    TrefoilArchGeom t = makeTrefoil(1.0, 0.45, 0.30);
    double rExpected = 0.30 * t.base.circleL.radius;
    EXPECT_NEAR(t.foilLeft.circle.radius,  rExpected, 1e-9);
    EXPECT_NEAR(t.foilRight.circle.radius, rExpected, 1e-9);
}

//
// C1 continuity at split points
//

TEST(TEST_cgmath_architecture_gothic, TrefoilC1AtSplitL)
{
    // Tangent of upperLeft at t=0 (splitL) and tangent of foilLeft.arc at t=0
    // (splitL) should be collinear (cross product 0). They may point in
    // opposite directions due to parameterization conventions ; the spec test
    // is about C1 (collinear tangent line), not same-direction traversal.
    TrefoilArchGeom t = makeTrefoil(1.0, 0.45, 0.30);
    Vector2d t1 = t.upperLeft.tangentAt(0.0);
    Vector2d t2 = t.foilLeft.arc.tangentAt(0.0);
    EXPECT_NEAR(t1.CrossProduct(t2), 0.0, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, TrefoilC1AtSplitR)
{
    TrefoilArchGeom t = makeTrefoil(1.0, 0.45, 0.30);
    Vector2d t1 = t.upperRight.tangentAt(1.0);
    Vector2d t2 = t.foilRight.arc.tangentAt(0.0);
    EXPECT_NEAR(t1.CrossProduct(t2), 0.0, 1e-9);
}

//
// Geometric sanity
//

TEST(TEST_cgmath_architecture_gothic, TrefoilSplitPointOnMainCircle)
{
    TrefoilArchGeom t = makeTrefoil(1.2, 0.45, 0.30);
    EXPECT_TRUE(t.base.circleL.contains(t.foilLeft.splitPoint,  1e-9));
    EXPECT_TRUE(t.base.circleR.contains(t.foilRight.splitPoint, 1e-9));
}

TEST(TEST_cgmath_architecture_gothic, TrefoilSplitPointMatchesArcLeftAtParameter)
{
    // splitL coincides with arcLeft.pointAt(splitParameter).
    double s = 0.45;
    TrefoilArchGeom t = makeTrefoil(1.0, s, 0.30);
    Vector2d expected = t.base.arcLeft.pointAt(s);
    EXPECT_NEAR(t.foilLeft.splitPoint.x, expected.x, 1e-9);
    EXPECT_NEAR(t.foilLeft.splitPoint.y, expected.y, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, TrefoilFoilCenterOnRadialLine)
{
    // mf, splitPoint, and main circle's center are colinear.
    TrefoilArchGeom t = makeTrefoil(1.2, 0.45, 0.30);
    Vector2d a = Vector2d(t.foilLeft.circle.center.x - t.base.circleL.center.x,
                          t.foilLeft.circle.center.y - t.base.circleL.center.y);
    Vector2d b = Vector2d(t.foilLeft.splitPoint.x - t.base.circleL.center.x,
                          t.foilLeft.splitPoint.y - t.base.circleL.center.y);
    EXPECT_NEAR(a.CrossProduct(b), 0.0, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, TrefoilFootPointIsAntipodalOfSplitPoint)
{
    // footPoint = splitPoint + 2 * (mf - splitPoint), so 2*mf - splitPoint.
    TrefoilArchGeom t = makeTrefoil(1.0, 0.45, 0.30);
    Vector2d expected(2.0 * t.foilLeft.circle.center.x - t.foilLeft.splitPoint.x,
                      2.0 * t.foilLeft.circle.center.y - t.foilLeft.splitPoint.y);
    EXPECT_NEAR(t.foilLeft.footPoint.x, expected.x, 1e-9);
    EXPECT_NEAR(t.foilLeft.footPoint.y, expected.y, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, TrefoilFoilArcEndpoints)
{
    TrefoilArchGeom t = makeTrefoil(1.0, 0.45, 0.30);
    Vector2d aL0 = t.foilLeft.arc.pointAt(0.0);
    Vector2d aL1 = t.foilLeft.arc.pointAt(1.0);
    EXPECT_NEAR(aL0.x, t.foilLeft.splitPoint.x, 1e-9);
    EXPECT_NEAR(aL0.y, t.foilLeft.splitPoint.y, 1e-9);
    EXPECT_NEAR(aL1.x, t.foilLeft.footPoint.x, 1e-9);
    EXPECT_NEAR(aL1.y, t.foilLeft.footPoint.y, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, TrefoilFoilArcSpansSemicircle)
{
    TrefoilArchGeom t = makeTrefoil(1.0, 0.45, 0.30);
    const double PI = std::atan2(0.0, -1.0);
    EXPECT_NEAR(std::fabs(t.foilLeft.arc.spanAngle()),  PI, 1e-9);
    EXPECT_NEAR(std::fabs(t.foilRight.arc.spanAngle()), PI, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, TrefoilFoilArcBumpsDownward)
{
    // The midpoint of each foil arc has y less than the foil center's y
    // (under the horizontal-base assumption) because of the chosen orientation.
    TrefoilArchGeom t = makeTrefoil(1.0, 0.45, 0.30);
    Vector2d midL = t.foilLeft.arc.pointAt(0.5);
    Vector2d midR = t.foilRight.arc.pointAt(0.5);
    EXPECT_LT(midL.y, t.foilLeft.circle.center.y);
    EXPECT_LT(midR.y, t.foilRight.circle.center.y);
}

//
// Symmetry
//

TEST(TEST_cgmath_architecture_gothic, TrefoilSymmetryAroundCenterAxis)
{
    TrefoilArchGeom t = makeTrefoil(1.0, 0.45, 0.30);
    double midX = (t.base.circleL.center.x + t.base.circleR.center.x) / 2.0;
    EXPECT_NEAR(t.foilLeft.splitPoint.x  + t.foilRight.splitPoint.x,
                2.0 * midX, 1e-9);
    EXPECT_NEAR(t.foilLeft.splitPoint.y, t.foilRight.splitPoint.y, 1e-9);
    EXPECT_NEAR(t.foilLeft.footPoint.x  + t.foilRight.footPoint.x,
                2.0 * midX, 1e-9);
}

//
// Validation
//

TEST(TEST_cgmath_architecture_gothic, TrefoilSplitParameterOutOfRangeThrows)
{
    ArchGeom a = buildArch(makeBasis(1.0));
    TrefoilParams p; p.foilRadiusFactor = 0.30;
    p.splitParameter = 0.0;
    EXPECT_THROW(buildTrefoilArch(a, p), std::invalid_argument);
    p.splitParameter = 1.0;
    EXPECT_THROW(buildTrefoilArch(a, p), std::invalid_argument);
    p.splitParameter = -0.1;
    EXPECT_THROW(buildTrefoilArch(a, p), std::invalid_argument);
    p.splitParameter = 1.5;
    EXPECT_THROW(buildTrefoilArch(a, p), std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, TrefoilFoilRadiusFactorOutOfRangeThrows)
{
    ArchGeom a = buildArch(makeBasis(1.0));
    TrefoilParams p; p.splitParameter = 0.45;
    p.foilRadiusFactor = 0.0;
    EXPECT_THROW(buildTrefoilArch(a, p), std::invalid_argument);
    p.foilRadiusFactor = -0.1;
    EXPECT_THROW(buildTrefoilArch(a, p), std::invalid_argument);
    p.foilRadiusFactor = 0.6;
    EXPECT_THROW(buildTrefoilArch(a, p), std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, TrefoilNonFiniteThrows)
{
    ArchGeom a = buildArch(makeBasis(1.0));
    TrefoilParams p;
    p.splitParameter = std::numeric_limits<double>::quiet_NaN();
    p.foilRadiusFactor = 0.30;
    EXPECT_THROW(buildTrefoilArch(a, p), std::invalid_argument);

    p.splitParameter = 0.45;
    p.foilRadiusFactor = std::numeric_limits<double>::infinity();
    EXPECT_THROW(buildTrefoilArch(a, p), std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, TrefoilFoilBulgesTowardBaseOnTiltedArch)
{
    // Hand-craft a tilted ArchGeom : same equilateral arch geometry but rotated 90 deg.
    // Original : pL=(-100,0), pR=(100,0), apex=(0, 100*sqrt(3)).
    // Rotated  : pL=(0,-100), pR=(0, 100), apex=(-100*sqrt(3), 0). Base is vertical.
    const double H = 100.0 * std::sqrt(3.0);
    ArchGeom a;
    a.circleL = Circle(Vector2d(0.0, -100.0), 200.0);
    a.circleR = Circle(Vector2d(0.0,  100.0), 200.0);
    a.apex    = Vector2d(-H, 0.0);
    a.width   = 200.0;
    a.height  = H;        // not strictly correct for tilted arch but unused here
    // (arcLeft / arcRight unused by the trefoil builder for the foil orientation)

    TrefoilParams p;
    p.splitParameter   = 0.45;
    p.foilRadiusFactor = 0.30;
    TrefoilArchGeom t = buildTrefoilArch(a, p);

    // For this rotated arch, "downward" = from apex (-H, 0) toward base midpoint (0, 0),
    // which is the +x direction. So the foil should bulge in +x.
    // Verify : midpoint of foilLeft.arc has x > splitPoint.x.
    Vector2d midL = t.foilLeft.arc.pointAt(0.5);
    EXPECT_GT(midL.x, t.foilLeft.splitPoint.x);
    Vector2d midR = t.foilRight.arc.pointAt(0.5);
    EXPECT_GT(midR.x, t.foilRight.splitPoint.x);
}

//
// Fillets (A7) -- minimal scope : 2 lateral fillets only
//

namespace
{
    // Build a typical (mainOffset, subwindows, rosette) trio for fillet tests.
    void buildTypicalConfig(ArchOffsetGeom &mainOffsetOut,
                             SubwindowsGeom  &swOut,
                             RosetteGeom     &rosetteOut,
                             int n = 2)
    {
        ArchGeom main = buildArch(makeBasis(1.0));
        ArchOffsetParams op; op.outer = 16.0; op.inner = 10.0;
        mainOffsetOut = buildArchOffset(main, op);

        SubwindowParams sp = makeSubParamsFraction(n, 0.0, 1.0, 0.05);
        ArchOffsetParams smallOp; smallOp.outer = 5.0; smallOp.inner = 5.0;
        swOut = buildSubwindows(main, smallOp, sp);
        rosetteOut = buildRosette(main, swOut);
    }
}

TEST(TEST_cgmath_architecture_gothic, FilletsCountIsTwo)
{
    ArchOffsetGeom mainOff; SubwindowsGeom sw; RosetteGeom ros;
    buildTypicalConfig(mainOff, sw, ros, 2);
    FilletsGeom f = buildFillets(mainOff, sw, ros, 10.0);
    EXPECT_EQ(f.fillets.size(), 2u);
}

TEST(TEST_cgmath_architecture_gothic, FilletsCountIsTwoForN1)
{
    ArchOffsetGeom mainOff; SubwindowsGeom sw; RosetteGeom ros;
    buildTypicalConfig(mainOff, sw, ros, 1);
    FilletsGeom f = buildFillets(mainOff, sw, ros, 10.0);
    EXPECT_EQ(f.fillets.size(), 2u);
}

TEST(TEST_cgmath_architecture_gothic, FilletsCountIsTwoForN3)
{
    ArchOffsetGeom mainOff; SubwindowsGeom sw; RosetteGeom ros;
    buildTypicalConfig(mainOff, sw, ros, 3);
    FilletsGeom f = buildFillets(mainOff, sw, ros, 10.0);
    EXPECT_EQ(f.fillets.size(), 2u);
}

TEST(TEST_cgmath_architecture_gothic, FilletsCornersOnIntersectionCircles)
{
    ArchOffsetGeom mainOff; SubwindowsGeom sw; RosetteGeom ros;
    buildTypicalConfig(mainOff, sw, ros);
    FilletsGeom f = buildFillets(mainOff, sw, ros, 10.0);

    // For each fillet, each corner must lie on the two circles that define it.
    for (const FilletGeom &fil : f.fillets)
    {
        EXPECT_TRUE(fil.arcOuter.circle.contains(fil.cornerA, 1e-6));
        EXPECT_TRUE(fil.arcSub.circle.contains  (fil.cornerA, 1e-6));

        EXPECT_TRUE(fil.arcSub.circle.contains    (fil.cornerB, 1e-6));
        EXPECT_TRUE(fil.arcRosette.circle.contains(fil.cornerB, 1e-6));

        EXPECT_TRUE(fil.arcRosette.circle.contains(fil.cornerC, 1e-6));
        EXPECT_TRUE(fil.arcOuter.circle.contains  (fil.cornerC, 1e-6));
    }
}

TEST(TEST_cgmath_architecture_gothic, FilletsArcsEndAtCorners)
{
    ArchOffsetGeom mainOff; SubwindowsGeom sw; RosetteGeom ros;
    buildTypicalConfig(mainOff, sw, ros);
    FilletsGeom f = buildFillets(mainOff, sw, ros, 10.0);

    for (const FilletGeom &fil : f.fillets)
    {
        // arcOuter goes from cornerC to cornerA.
        Vector2d outerStart = fil.arcOuter.pointAt(0.0);
        Vector2d outerEnd   = fil.arcOuter.pointAt(1.0);
        EXPECT_NEAR(outerStart.x, fil.cornerC.x, 1e-6);
        EXPECT_NEAR(outerStart.y, fil.cornerC.y, 1e-6);
        EXPECT_NEAR(outerEnd.x,   fil.cornerA.x, 1e-6);
        EXPECT_NEAR(outerEnd.y,   fil.cornerA.y, 1e-6);

        // arcSub goes from cornerA to cornerB.
        Vector2d subStart = fil.arcSub.pointAt(0.0);
        Vector2d subEnd   = fil.arcSub.pointAt(1.0);
        EXPECT_NEAR(subStart.x, fil.cornerA.x, 1e-6);
        EXPECT_NEAR(subStart.y, fil.cornerA.y, 1e-6);
        EXPECT_NEAR(subEnd.x,   fil.cornerB.x, 1e-6);
        EXPECT_NEAR(subEnd.y,   fil.cornerB.y, 1e-6);

        // arcRosette goes from cornerB to cornerC.
        Vector2d rosStart = fil.arcRosette.pointAt(0.0);
        Vector2d rosEnd   = fil.arcRosette.pointAt(1.0);
        EXPECT_NEAR(rosStart.x, fil.cornerB.x, 1e-6);
        EXPECT_NEAR(rosStart.y, fil.cornerB.y, 1e-6);
        EXPECT_NEAR(rosEnd.x,   fil.cornerC.x, 1e-6);
        EXPECT_NEAR(rosEnd.y,   fil.cornerC.y, 1e-6);
    }
}

TEST(TEST_cgmath_architecture_gothic, FilletsCornersAreInUpperPart)
{
    ArchOffsetGeom mainOff; SubwindowsGeom sw; RosetteGeom ros;
    buildTypicalConfig(mainOff, sw, ros);
    FilletsGeom f = buildFillets(mainOff, sw, ros, 10.0);

    // All corners should be in the upper portion of the window (y > foot line).
    double y0 = mainOff.inner.circleL.center.y;
    for (const FilletGeom &fil : f.fillets)
    {
        EXPECT_GT(fil.cornerA.y, y0);
        EXPECT_GT(fil.cornerB.y, y0);
        EXPECT_GT(fil.cornerC.y, y0);
    }
}

TEST(TEST_cgmath_architecture_gothic, FilletsLeftAndRightAreSymmetric)
{
    ArchOffsetGeom mainOff; SubwindowsGeom sw; RosetteGeom ros;
    buildTypicalConfig(mainOff, sw, ros);
    FilletsGeom f = buildFillets(mainOff, sw, ros, 10.0);

    ASSERT_EQ(f.fillets.size(), 2u);
    const FilletGeom &left  = f.fillets[0];
    const FilletGeom &right = f.fillets[1];

    // For symmetric inputs centered on x=0, the right fillet should mirror the left.
    // Mirror of cornerA (left) across x=0 should match cornerA (right) up to tolerance.
    EXPECT_NEAR(left.cornerA.x, -right.cornerA.x, 1e-9);
    EXPECT_NEAR(left.cornerA.y,  right.cornerA.y, 1e-9);
    EXPECT_NEAR(left.cornerB.x, -right.cornerB.x, 1e-9);
    EXPECT_NEAR(left.cornerB.y,  right.cornerB.y, 1e-9);
    EXPECT_NEAR(left.cornerC.x, -right.cornerC.x, 1e-9);
    EXPECT_NEAR(left.cornerC.y,  right.cornerC.y, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic, FilletsCornersAreDistinct)
{
    ArchOffsetGeom mainOff; SubwindowsGeom sw; RosetteGeom ros;
    buildTypicalConfig(mainOff, sw, ros);
    FilletsGeom f = buildFillets(mainOff, sw, ros, 10.0);

    // Within each fillet, the 3 corners must be distinct (no degenerate triangle).
    for (const FilletGeom &fil : f.fillets)
    {
        EXPECT_GT(Vector2d::Distance(fil.cornerA, fil.cornerB), 1.0);
        EXPECT_GT(Vector2d::Distance(fil.cornerB, fil.cornerC), 1.0);
        EXPECT_GT(Vector2d::Distance(fil.cornerC, fil.cornerA), 1.0);
    }

    // Between left and right fillets, all corners must also be distinct.
    const FilletGeom &left  = f.fillets[0];
    const FilletGeom &right = f.fillets[1];
    Vector2d leftCorners[]  = { left.cornerA,  left.cornerB,  left.cornerC };
    Vector2d rightCorners[] = { right.cornerA, right.cornerB, right.cornerC };
    for (const Vector2d &lc : leftCorners)
        for (const Vector2d &rc : rightCorners)
            EXPECT_GT(Vector2d::Distance(lc, rc), 1.0);
}

TEST(TEST_cgmath_architecture_gothic, FilletsTranslatedBasis)
{
    // Build a translated configuration and check that fillets translate accordingly.
    ArchOffsetGeom mainOff0; SubwindowsGeom sw0; RosetteGeom ros0;
    buildTypicalConfig(mainOff0, sw0, ros0);
    FilletsGeom f0 = buildFillets(mainOff0, sw0, ros0, 10.0);

    Vector2d offset(50.0, 30.0);
    ArchBasis trBasis = makeBasis(1.0);
    trBasis.pL = trBasis.pL + offset;
    trBasis.pR = trBasis.pR + offset;
    ArchGeom mainTr = buildArch(trBasis);
    ArchOffsetParams op; op.outer = 16.0; op.inner = 10.0;
    ArchOffsetGeom mainOffTr = buildArchOffset(mainTr, op);
    ArchOffsetParams smallOp; smallOp.outer = 5.0; smallOp.inner = 5.0;
    SubwindowsGeom swTr = buildSubwindows(mainTr, smallOp,
                                           makeSubParamsFraction(2, 0.0, 1.0, 0.05));
    RosetteGeom rosTr = buildRosette(mainTr, swTr);
    FilletsGeom fTr = buildFillets(mainOffTr, swTr, rosTr, 10.0);

    ASSERT_EQ(fTr.fillets.size(), f0.fillets.size());
    for (size_t i = 0; i < f0.fillets.size(); ++i)
    {
        EXPECT_NEAR(fTr.fillets[i].cornerA.x, f0.fillets[i].cornerA.x + offset.x, 1e-9);
        EXPECT_NEAR(fTr.fillets[i].cornerA.y, f0.fillets[i].cornerA.y + offset.y, 1e-9);
    }
}

TEST(TEST_cgmath_architecture_gothic, FilletsEmptySubwindowsThrows)
{
    ArchOffsetGeom mainOff; SubwindowsGeom sw; RosetteGeom ros;
    buildTypicalConfig(mainOff, sw, ros);
    SubwindowsGeom emptySw;
    EXPECT_THROW(buildFillets(mainOff, emptySw, ros, 10.0), std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, FilletsStoneBandWidthInvalidThrows)
{
    ArchOffsetGeom mainOff; SubwindowsGeom sw; RosetteGeom ros;
    buildTypicalConfig(mainOff, sw, ros);

    EXPECT_THROW(buildFillets(mainOff, sw, ros, 0.0), std::invalid_argument);
    EXPECT_THROW(buildFillets(mainOff, sw, ros, -1.0), std::invalid_argument);
    EXPECT_THROW(buildFillets(mainOff, sw, ros,
                              std::numeric_limits<double>::infinity()),
                 std::invalid_argument);
    EXPECT_THROW(buildFillets(mainOff, sw, ros,
                              std::numeric_limits<double>::quiet_NaN()),
                 std::invalid_argument);
}

//
// Mouchettes (A8)
//

namespace
{
    SubwindowsGeom buildSwForMouchettes(int n)
    {
        ArchGeom main = buildArch(makeBasis(1.0));
        ArchOffsetParams op; op.outer = 5.0; op.inner = 5.0;
        return buildSubwindows(main, op, makeSubParamsFraction(n, 0.0, 1.0, 0.05));
    }

    MouchetteParams makeMouchetteParams(MouchetteType type,
                                         double radiusFactor = 0.18,
                                         double rotation = 0.0)
    {
        MouchetteParams p;
        p.type = type;
        p.radiusFactor = radiusFactor;
        p.rotation = rotation;
        return p;
    }
}

//
// Count / placement
//

TEST(TEST_cgmath_architecture_gothic, MouchettesCountIsLancetCountMinusOne)
{
    SubwindowsGeom sw2 = buildSwForMouchettes(2);
    SubwindowsGeom sw3 = buildSwForMouchettes(3);
    SubwindowsGeom sw4 = buildSwForMouchettes(4);
    MouchetteParams p = makeMouchetteParams(MouchetteType::Vesica);
    EXPECT_EQ(buildMouchettes(sw2, p).size(), 1u);
    EXPECT_EQ(buildMouchettes(sw3, p).size(), 2u);
    EXPECT_EQ(buildMouchettes(sw4, p).size(), 3u);
}

TEST(TEST_cgmath_architecture_gothic, MouchettesEmptyForSingleLancet)
{
    SubwindowsGeom sw = buildSwForMouchettes(1);
    MouchetteParams p = makeMouchetteParams(MouchetteType::Vesica);
    EXPECT_TRUE(buildMouchettes(sw, p).empty());
}

TEST(TEST_cgmath_architecture_gothic, MouchettesPositionedInGapBetweenLancets)
{
    SubwindowsGeom sw = buildSwForMouchettes(2);
    MouchetteParams p = makeMouchetteParams(MouchetteType::Vesica);
    auto m = buildMouchettes(sw, p);
    ASSERT_EQ(m.size(), 1u);

    double midX = (sw.lancets[0].basis.pR.x + sw.lancets[1].basis.pL.x) / 2.0;
    EXPECT_NEAR(m[0].center.x, midX, 1e-9);

    double maxApex = std::max(sw.lancets[0].arch.apex.y,
                              sw.lancets[1].arch.apex.y);
    EXPECT_GT(m[0].center.y, maxApex);    // above the apexes
}

//
// Vesica
//

TEST(TEST_cgmath_architecture_gothic, MouchettesVesicaHasTwoArcs)
{
    SubwindowsGeom sw = buildSwForMouchettes(2);
    auto m = buildMouchettes(sw, makeMouchetteParams(MouchetteType::Vesica));
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(m[0].contour.size(), 2u);
}

TEST(TEST_cgmath_architecture_gothic, MouchettesVesicaArcsHaveSameRadius)
{
    SubwindowsGeom sw = buildSwForMouchettes(2);
    auto m = buildMouchettes(sw, makeMouchetteParams(MouchetteType::Vesica));
    ASSERT_EQ(m.size(), 1u);
    const auto &c = m[0].contour;
    EXPECT_NEAR(c[0].circle.radius, m[0].radius, 1e-12);
    EXPECT_NEAR(c[1].circle.radius, m[0].radius, 1e-12);
}

TEST(TEST_cgmath_architecture_gothic, MouchettesVesicaCentersSymmetricAroundMouchetteCenter)
{
    SubwindowsGeom sw = buildSwForMouchettes(2);
    auto m = buildMouchettes(sw, makeMouchetteParams(MouchetteType::Vesica, 0.18, 0.0));
    ASSERT_EQ(m.size(), 1u);

    Vector2d sumCenters(m[0].contour[0].circle.center.x + m[0].contour[1].circle.center.x,
                        m[0].contour[0].circle.center.y + m[0].contour[1].circle.center.y);
    EXPECT_NEAR(sumCenters.x, 2.0 * m[0].center.x, 1e-9);
    EXPECT_NEAR(sumCenters.y, 2.0 * m[0].center.y, 1e-9);
}

//
// Teardrop
//

TEST(TEST_cgmath_architecture_gothic, MouchettesTeardropHasThreeArcs)
{
    SubwindowsGeom sw = buildSwForMouchettes(2);
    auto m = buildMouchettes(sw, makeMouchetteParams(MouchetteType::Teardrop));
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(m[0].contour.size(), 3u);
}

TEST(TEST_cgmath_architecture_gothic, MouchettesTeardropApexAlongRotation)
{
    // For rotation = 0 (apex pointing +x), apex is at (center.x + 2r, center.y).
    SubwindowsGeom sw = buildSwForMouchettes(2);
    auto m = buildMouchettes(sw, makeMouchetteParams(MouchetteType::Teardrop, 0.18, 0.0));
    ASSERT_EQ(m.size(), 1u);

    // Apex is the endpoint of side arcs (sideArc1 t=1, sideArc2 t=0).
    Vector2d apex = m[0].contour[1].pointAt(1.0);
    EXPECT_NEAR(apex.x, m[0].center.x + 2.0 * m[0].radius, 1e-6);
    EXPECT_NEAR(apex.y, m[0].center.y,                     1e-6);
}

TEST(TEST_cgmath_architecture_gothic, MouchettesTeardropBaseCircleRadiusEqualsMouchetteRadius)
{
    SubwindowsGeom sw = buildSwForMouchettes(2);
    auto m = buildMouchettes(sw, makeMouchetteParams(MouchetteType::Teardrop));
    ASSERT_EQ(m.size(), 1u);
    EXPECT_NEAR(m[0].contour[0].circle.radius, m[0].radius, 1e-12);
}

TEST(TEST_cgmath_architecture_gothic, MouchettesTeardropSideArcsTangentToBase)
{
    // The side circle's center lies on the radial line through the tangent
    // point on the base circle. Equivalently, base.center, T_i, and side.center
    // are colinear.
    SubwindowsGeom sw = buildSwForMouchettes(2);
    auto m = buildMouchettes(sw, makeMouchetteParams(MouchetteType::Teardrop));
    ASSERT_EQ(m.size(), 1u);
    const auto &mou = m[0];

    Vector2d T1 = mou.contour[1].pointAt(0.0);   // start of sideArc1
    Vector2d T2 = mou.contour[2].pointAt(1.0);   // end   of sideArc2

    // Cross product of (T1 - center) and (sideCircle1.center - center) ~= 0.
    Vector2d a1(T1.x - mou.center.x, T1.y - mou.center.y);
    Vector2d b1(mou.contour[1].circle.center.x - mou.center.x,
                mou.contour[1].circle.center.y - mou.center.y);
    EXPECT_NEAR(a1.CrossProduct(b1), 0.0, 1e-9);

    Vector2d a2(T2.x - mou.center.x, T2.y - mou.center.y);
    Vector2d b2(mou.contour[2].circle.center.x - mou.center.x,
                mou.contour[2].circle.center.y - mou.center.y);
    EXPECT_NEAR(a2.CrossProduct(b2), 0.0, 1e-9);
}

//
// Soufflet aliasing
//

TEST(TEST_cgmath_architecture_gothic, MouchettesSouffletIsAliasOfTeardrop)
{
    SubwindowsGeom sw = buildSwForMouchettes(2);
    auto mS = buildMouchettes(sw, makeMouchetteParams(MouchetteType::Soufflet));
    auto mT = buildMouchettes(sw, makeMouchetteParams(MouchetteType::Teardrop));
    ASSERT_EQ(mS.size(), mT.size());
    EXPECT_EQ(mS[0].contour.size(), mT[0].contour.size());
    // Identical geometry.
    EXPECT_NEAR(mS[0].center.x, mT[0].center.x, 1e-12);
    EXPECT_NEAR(mS[0].center.y, mT[0].center.y, 1e-12);
    EXPECT_NEAR(mS[0].radius,   mT[0].radius,   1e-12);
}

//
// Validation
//

TEST(TEST_cgmath_architecture_gothic, MouchettesEmptySubwindowsThrows)
{
    SubwindowsGeom emptySw;
    EXPECT_THROW(buildMouchettes(emptySw, makeMouchetteParams(MouchetteType::Vesica)),
                 std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, MouchettesRadiusFactorOutOfRangeThrows)
{
    SubwindowsGeom sw = buildSwForMouchettes(2);
    EXPECT_THROW(buildMouchettes(sw, makeMouchetteParams(MouchetteType::Vesica, 0.0)),
                 std::invalid_argument);
    EXPECT_THROW(buildMouchettes(sw, makeMouchetteParams(MouchetteType::Vesica, -0.1)),
                 std::invalid_argument);
    EXPECT_THROW(buildMouchettes(sw, makeMouchetteParams(MouchetteType::Vesica, 0.6)),
                 std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic, MouchettesNonFiniteThrows)
{
    SubwindowsGeom sw = buildSwForMouchettes(2);
    MouchetteParams p = makeMouchetteParams(MouchetteType::Vesica);

    p.radiusFactor = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(buildMouchettes(sw, p), std::invalid_argument);
    p.radiusFactor = 0.18;
    p.rotation = std::numeric_limits<double>::infinity();
    EXPECT_THROW(buildMouchettes(sw, p), std::invalid_argument);
}
