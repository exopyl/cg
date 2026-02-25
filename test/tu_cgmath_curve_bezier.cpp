#include <gtest/gtest.h>

#include "../src/cgmath/curve_bezier.h"

TEST(TEST_cgmath_curve_bezier, AddControlPoint)
{
	// context
	CurveBezier curve;

	// action
	int res1 = curve.addControlPoint(1.f, 2.f, 3.f);
	int res2 = curve.addControlPoint(4.f, 5.f, 6.f);

	// expectations
	EXPECT_EQ(res1, 1);
	EXPECT_EQ(res2, 1);
	EXPECT_EQ(curve.getDegree(), 1);
}

TEST(TEST_cgmath_curve_bezier, AddControlPointVec3)
{
	// context
	CurveBezier curve;
	vec3 v = {1.f, 2.f, 3.f};

	// action
	int res = curve.addControlPoint(v);

	// expectations
	EXPECT_EQ(res, 1);
	EXPECT_EQ(curve.getDegree(), 0);
}

TEST(TEST_cgmath_curve_bezier, GetControlPoint)
{
	// context
	CurveBezier curve;
	curve.addControlPoint(1.f, 2.f, 3.f);
	curve.addControlPoint(4.f, 5.f, 6.f);

	// action
	vec3 v = {0.f, 0.f, 0.f};
	bool res = curve.getControlPoint(0, v);

	// expectations
	EXPECT_TRUE(res);
	EXPECT_FLOAT_EQ(v[0], 1.f);
	EXPECT_FLOAT_EQ(v[1], 2.f);
	EXPECT_FLOAT_EQ(v[2], 3.f);
}

TEST(TEST_cgmath_curve_bezier, GetDegree)
{
	// context
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(1.f, 1.f, 0.f);
	curve.addControlPoint(2.f, 0.f, 0.f);
	curve.addControlPoint(3.f, 1.f, 0.f);

	// expectations
	EXPECT_EQ(curve.getDegree(), 3);
}

TEST(TEST_cgmath_curve_bezier, EvalLinear)
{
	// context
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(1.f, 1.f, 0.f);

	// action
	vec3 pt;
	curve.eval(0.f, pt);

	// expectations
	EXPECT_FLOAT_EQ(pt[0], 0.f);
	EXPECT_FLOAT_EQ(pt[1], 0.f);
	EXPECT_FLOAT_EQ(pt[2], 0.f);
}

TEST(TEST_cgmath_curve_bezier, EvalLinearEnd)
{
	// context
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(1.f, 1.f, 0.f);

	// action
	vec3 pt;
	curve.eval(1.f, pt);

	// expectations
	EXPECT_FLOAT_EQ(pt[0], 1.f);
	EXPECT_FLOAT_EQ(pt[1], 1.f);
	EXPECT_FLOAT_EQ(pt[2], 0.f);
}

TEST(TEST_cgmath_curve_bezier, EvalLinearMid)
{
	// context
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(2.f, 4.f, 0.f);

	// action
	vec3 pt;
	curve.eval(0.5f, pt);

	// expectations
	EXPECT_FLOAT_EQ(pt[0], 1.f);
	EXPECT_FLOAT_EQ(pt[1], 2.f);
	EXPECT_FLOAT_EQ(pt[2], 0.f);
}

TEST(TEST_cgmath_curve_bezier, CopyConstructor)
{
	// context
	CurveBezier curve;
	curve.addControlPoint(1.f, 2.f, 3.f);
	curve.addControlPoint(4.f, 5.f, 6.f);

	// action
	CurveBezier copy(curve);

	// expectations
	EXPECT_EQ(copy.getDegree(), 1);
	vec3 v = {0.f, 0.f, 0.f};
	copy.getControlPoint(0, v);
	EXPECT_FLOAT_EQ(v[0], 1.f);
	EXPECT_FLOAT_EQ(v[1], 2.f);
	EXPECT_FLOAT_EQ(v[2], 3.f);
}

TEST(TEST_cgmath_curve_bezier, ComputeInterpolation)
{
	// context
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(1.f, 1.f, 0.f);
	curve.addControlPoint(2.f, 0.f, 0.f);

	// action
	vec3 *points = nullptr;
	int res = curve.computeInterpolation(3, &points);

	// expectations
	EXPECT_EQ(res, 1);
	EXPECT_NE(points, nullptr);
	EXPECT_FLOAT_EQ(points[0][0], 0.f);
	EXPECT_FLOAT_EQ(points[0][1], 0.f);
	EXPECT_FLOAT_EQ(points[2][0], 2.f);
	EXPECT_FLOAT_EQ(points[2][1], 0.f);
	free(points);
}

TEST(TEST_cgmath_curve_bezier, ComputeInterpolation3)
{
	// context - quadratic bezier (3 control points, degree 2)
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(1.f, 2.f, 0.f);
	curve.addControlPoint(2.f, 0.f, 0.f);

	// action
	vec3 *points = nullptr;
	int res = curve.computeInterpolation3(3, &points);

	// expectations
	EXPECT_EQ(res, 1);
	EXPECT_NE(points, nullptr);
	EXPECT_FLOAT_EQ(points[0][0], 0.f);
	EXPECT_FLOAT_EQ(points[0][1], 0.f);
	EXPECT_FLOAT_EQ(points[2][0], 2.f);
	EXPECT_FLOAT_EQ(points[2][1], 0.f);
	free(points);
}

TEST(TEST_cgmath_curve_bezier, ComputeInterpolation3WrongDegree)
{
	// context - linear bezier (2 control points, degree 1)
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(1.f, 1.f, 0.f);

	// action
	vec3 *points = nullptr;
	int res = curve.computeInterpolation3(3, &points);

	// expectations
	EXPECT_EQ(res, 0);
}

TEST(TEST_cgmath_curve_bezier, ComputeInterpolation4)
{
	// context - cubic bezier (4 control points, degree 3)
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(1.f, 2.f, 0.f);
	curve.addControlPoint(2.f, 2.f, 0.f);
	curve.addControlPoint(3.f, 0.f, 0.f);

	// action
	vec3 *points = nullptr;
	int res = curve.computeInterpolation4(3, &points);

	// expectations
	EXPECT_EQ(res, 1);
	EXPECT_NE(points, nullptr);
	EXPECT_FLOAT_EQ(points[0][0], 0.f);
	EXPECT_FLOAT_EQ(points[0][1], 0.f);
	EXPECT_FLOAT_EQ(points[2][0], 3.f);
	EXPECT_FLOAT_EQ(points[2][1], 0.f);
	free(points);
}

TEST(TEST_cgmath_curve_bezier, ComputeInterpolation4WrongDegree)
{
	// context - quadratic bezier (3 control points, degree 2)
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(1.f, 1.f, 0.f);
	curve.addControlPoint(2.f, 0.f, 0.f);

	// action
	vec3 *points = nullptr;
	int res = curve.computeInterpolation4(3, &points);

	// expectations
	EXPECT_EQ(res, 0);
}

TEST(TEST_cgmath_curve_bezier, ComputeInterpolationRecursive3)
{
	// context - cubic bezier
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(1.f, 2.f, 0.f);
	curve.addControlPoint(2.f, 2.f, 0.f);
	curve.addControlPoint(3.f, 0.f, 0.f);

	// action
	int nPoints = 0;
	vec3 *points = nullptr;
	int res = curve.computeInterpolationRecursive3(2, nPoints, &points);

	// expectations
	EXPECT_EQ(res, 1);
	EXPECT_EQ(nPoints, 5); // 1 + 2^2
	EXPECT_NE(points, nullptr);
	EXPECT_FLOAT_EQ(points[0][0], 0.f);
	EXPECT_FLOAT_EQ(points[0][1], 0.f);
	free(points);
}

TEST(TEST_cgmath_curve_bezier, ComputeInterpolationRecursive3WrongDegree)
{
	// context - quadratic bezier (degree 2, not 3)
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(1.f, 1.f, 0.f);
	curve.addControlPoint(2.f, 0.f, 0.f);

	// action
	int nPoints = 0;
	vec3 *points = nullptr;
	int res = curve.computeInterpolationRecursive3(2, nPoints, &points);

	// expectations
	EXPECT_EQ(res, 0);
}

TEST(TEST_cgmath_curve_bezier, DynamicGrowth)
{
	// context - add more than 4 control points to trigger realloc
	CurveBezier curve;

	// action
	for (int i = 0; i < 10; i++)
		curve.addControlPoint((float)i, (float)i, 0.f);

	// expectations
	EXPECT_EQ(curve.getDegree(), 9);
}
