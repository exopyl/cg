#include <gtest/gtest.h>

#include "../src/cgmath/curve_nurbs.h"

TEST(TEST_cgmath_curve_nurbs, AddControlPoint)
{
	// context
	CurveNURBS curve;

	// action
	int res1 = curve.addControlPoint(0.f, 0.f, 0.f, 1.f);
	int res2 = curve.addControlPoint(1.f, 1.f, 0.f, 1.f);

	// expectations
	EXPECT_EQ(res1, 1);
	EXPECT_EQ(res2, 1);
	EXPECT_EQ(curve.nControlPoints(), 2);
}

TEST(TEST_cgmath_curve_nurbs, GetControlPoints)
{
	// context
	CurveNURBS curve;
	curve.addControlPoint(1.f, 2.f, 3.f, 1.f);
	curve.addControlPoint(4.f, 5.f, 6.f, 1.f);

	// action
	vec3 v;
	bool res = curve.getControlPoints(0, v);

	// expectations
	EXPECT_TRUE(res);
	EXPECT_FLOAT_EQ(v[0], 1.f);
	EXPECT_FLOAT_EQ(v[1], 2.f);
	EXPECT_FLOAT_EQ(v[2], 3.f);
}

TEST(TEST_cgmath_curve_nurbs, GetControlPointsSecond)
{
	// context
	CurveNURBS curve;
	curve.addControlPoint(1.f, 2.f, 3.f, 1.f);
	curve.addControlPoint(4.f, 5.f, 6.f, 1.f);

	// action
	vec3 v;
	bool res = curve.getControlPoints(1, v);

	// expectations
	EXPECT_TRUE(res);
	EXPECT_FLOAT_EQ(v[0], 4.f);
	EXPECT_FLOAT_EQ(v[1], 5.f);
	EXPECT_FLOAT_EQ(v[2], 6.f);
}

TEST(TEST_cgmath_curve_nurbs, NControlPoints)
{
	// context
	CurveNURBS curve;

	// action
	curve.addControlPoint(0.f, 0.f, 0.f, 1.f);
	curve.addControlPoint(1.f, 0.f, 0.f, 1.f);
	curve.addControlPoint(2.f, 0.f, 0.f, 1.f);

	// expectations
	EXPECT_EQ(curve.nControlPoints(), 3);
}

TEST(TEST_cgmath_curve_nurbs, SetKnots)
{
	// context
	CurveNURBS curve;
	curve.addControlPoint(0.f, 0.f, 0.f, 1.f);
	curve.addControlPoint(1.f, 1.f, 0.f, 1.f);
	curve.addControlPoint(2.f, 0.f, 0.f, 1.f);
	curve.addControlPoint(3.f, 1.f, 0.f, 1.f);
	float knots[] = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};

	// action
	int res = curve.setKnots(knots, 8);

	// expectations
	EXPECT_EQ(res, 1);
}

TEST(TEST_cgmath_curve_nurbs, NormalizeKnots)
{
	// context
	CurveNURBS curve;
	curve.addControlPoint(0.f, 0.f, 0.f, 1.f);
	curve.addControlPoint(1.f, 1.f, 0.f, 1.f);
	curve.addControlPoint(2.f, 0.f, 0.f, 1.f);
	curve.addControlPoint(3.f, 1.f, 0.f, 1.f);
	float knots[] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f};
	curve.setKnots(knots, 8);

	// action
	int res = curve.normalizeKnots();

	// expectations
	EXPECT_EQ(res, 1);
}

TEST(TEST_cgmath_curve_nurbs, CopyConstructor)
{
	// context
	CurveNURBS curve;
	curve.addControlPoint(1.f, 2.f, 3.f, 0.5f);
	curve.addControlPoint(4.f, 5.f, 6.f, 1.f);

	// action
	CurveNURBS copy(curve);

	// expectations
	EXPECT_EQ(copy.nControlPoints(), 2);
	vec3 v;
	copy.getControlPoints(0, v);
	EXPECT_FLOAT_EQ(v[0], 1.f);
	EXPECT_FLOAT_EQ(v[1], 2.f);
	EXPECT_FLOAT_EQ(v[2], 3.f);
}

TEST(TEST_cgmath_curve_nurbs, DynamicGrowth)
{
	// context - add more than 4 control points to trigger realloc
	CurveNURBS curve;

	// action
	for (int i = 0; i < 10; i++)
		curve.addControlPoint((float)i, (float)i, 0.f, 1.f);

	// expectations
	EXPECT_EQ(curve.nControlPoints(), 10);
}

TEST(TEST_cgmath_curve_nurbs, ComputeInterpolation)
{
	// context - cubic NURBS with uniform knots
	CurveNURBS curve;
	curve.addControlPoint(0.f, 0.f, 0.f, 1.f);
	curve.addControlPoint(1.f, 2.f, 0.f, 1.f);
	curve.addControlPoint(2.f, 2.f, 0.f, 1.f);
	curve.addControlPoint(3.f, 0.f, 0.f, 1.f);
	float knots[] = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
	curve.setKnots(knots, 8);

	// action
	vec3 *points = nullptr;
	int res = curve.computeInterpolation(10, &points);

	// expectations
	EXPECT_EQ(res, 1);
	EXPECT_NE(points, nullptr);
	free(points);
}
