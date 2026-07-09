#include <gtest/gtest.h>
#include <vector>

#include "../src/cgmath/curve_nurbs.h"

TEST(TEST_cgmath_curve_nurbs, AddControlPoint)
{
	CurveNURBS curve;

	int res1 = curve.addControlPoint(0.f, 0.f, 0.f, 1.f);
	int res2 = curve.addControlPoint(1.f, 1.f, 0.f, 1.f);

	EXPECT_EQ(res1, 1);
	EXPECT_EQ(res2, 1);
	EXPECT_EQ(curve.nControlPoints(), 2);
}

TEST(TEST_cgmath_curve_nurbs, GetControlPoints)
{
	CurveNURBS curve;
	curve.addControlPoint(1.f, 2.f, 3.f, 1.f);
	curve.addControlPoint(4.f, 5.f, 6.f, 1.f);

	Vector3f v;
	bool res = curve.getControlPoints(0, v);

	EXPECT_TRUE(res);
	EXPECT_FLOAT_EQ(v.x, 1.f);
	EXPECT_FLOAT_EQ(v.y, 2.f);
	EXPECT_FLOAT_EQ(v.z, 3.f);
}

TEST(TEST_cgmath_curve_nurbs, GetControlPointsSecond)
{
	CurveNURBS curve;
	curve.addControlPoint(1.f, 2.f, 3.f, 1.f);
	curve.addControlPoint(4.f, 5.f, 6.f, 1.f);

	Vector3f v;
	bool res = curve.getControlPoints(1, v);

	EXPECT_TRUE(res);
	EXPECT_FLOAT_EQ(v.x, 4.f);
	EXPECT_FLOAT_EQ(v.y, 5.f);
	EXPECT_FLOAT_EQ(v.z, 6.f);
}

TEST(TEST_cgmath_curve_nurbs, GetControlPointsOutOfRange)
{
	CurveNURBS curve;
	curve.addControlPoint(1.f, 2.f, 3.f, 1.f);

	Vector3f v;
	EXPECT_FALSE(curve.getControlPoints(5, v));
	EXPECT_FALSE(curve.getControlPoints(-1, v));
}

TEST(TEST_cgmath_curve_nurbs, NControlPoints)
{
	CurveNURBS curve;

	curve.addControlPoint(0.f, 0.f, 0.f, 1.f);
	curve.addControlPoint(1.f, 0.f, 0.f, 1.f);
	curve.addControlPoint(2.f, 0.f, 0.f, 1.f);

	EXPECT_EQ(curve.nControlPoints(), 3);
}

TEST(TEST_cgmath_curve_nurbs, SetKnots)
{
	CurveNURBS curve;
	curve.addControlPoint(0.f, 0.f, 0.f, 1.f);
	curve.addControlPoint(1.f, 1.f, 0.f, 1.f);
	curve.addControlPoint(2.f, 0.f, 0.f, 1.f);
	curve.addControlPoint(3.f, 1.f, 0.f, 1.f);
	float knots[] = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};

	int res = curve.setKnots(knots, 8);

	EXPECT_EQ(res, 1);
}

TEST(TEST_cgmath_curve_nurbs, NormalizeKnots)
{
	CurveNURBS curve;
	curve.addControlPoint(0.f, 0.f, 0.f, 1.f);
	curve.addControlPoint(1.f, 1.f, 0.f, 1.f);
	curve.addControlPoint(2.f, 0.f, 0.f, 1.f);
	curve.addControlPoint(3.f, 1.f, 0.f, 1.f);
	float knots[] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f};
	curve.setKnots(knots, 8);

	int res = curve.normalizeKnots();

	EXPECT_EQ(res, 1);
}

TEST(TEST_cgmath_curve_nurbs, CopyConstructor)
{
	CurveNURBS curve;
	curve.addControlPoint(1.f, 2.f, 3.f, 0.5f);
	curve.addControlPoint(4.f, 5.f, 6.f, 1.f);

	CurveNURBS copy(curve);

	EXPECT_EQ(copy.nControlPoints(), 2);
	Vector3f v;
	copy.getControlPoints(0, v);
	EXPECT_FLOAT_EQ(v.x, 1.f);
	EXPECT_FLOAT_EQ(v.y, 2.f);
	EXPECT_FLOAT_EQ(v.z, 3.f);
}

TEST(TEST_cgmath_curve_nurbs, DynamicGrowth)
{
	CurveNURBS curve;

	for (int i = 0; i < 10; i++)
		curve.addControlPoint((float)i, (float)i, 0.f, 1.f);

	EXPECT_EQ(curve.nControlPoints(), 10);
}

TEST(TEST_cgmath_curve_nurbs, Tessellate)
{
	CurveNURBS curve;
	curve.addControlPoint(0.f, 0.f, 0.f, 1.f);
	curve.addControlPoint(1.f, 2.f, 0.f, 1.f);
	curve.addControlPoint(2.f, 2.f, 0.f, 1.f);
	curve.addControlPoint(3.f, 0.f, 0.f, 1.f);
	float knots[] = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
	curve.setKnots(knots, 8);

	std::vector<Vector3f> pts;
	int n = curve.tessellate(10, pts);

	EXPECT_EQ(n, 10);
	EXPECT_EQ(pts.size(), 10u);
}

TEST(TEST_cgmath_curve_nurbs, TessellateWithoutKnots)
{
	CurveNURBS curve;
	curve.addControlPoint(0.f, 0.f, 0.f, 1.f);

	std::vector<Vector3f> pts;
	EXPECT_EQ(curve.tessellate(10, pts), 0);
}
