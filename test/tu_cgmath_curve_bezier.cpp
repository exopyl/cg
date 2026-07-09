#include <gtest/gtest.h>
#include <vector>

#include "../src/cgmath/curve_bezier.h"

TEST(TEST_cgmath_curve_bezier, AddControlPoint)
{
	CurveBezier curve;

	int res1 = curve.addControlPoint(1.f, 2.f, 3.f);
	int res2 = curve.addControlPoint(4.f, 5.f, 6.f);

	EXPECT_EQ(res1, 1);
	EXPECT_EQ(res2, 1);
	EXPECT_EQ(curve.getDegree(), 1);
}

TEST(TEST_cgmath_curve_bezier, AddControlPointVector3f)
{
	CurveBezier curve;

	int res = curve.addControlPoint(Vector3f(1.f, 2.f, 3.f));

	EXPECT_EQ(res, 1);
	EXPECT_EQ(curve.getDegree(), 0);
}

TEST(TEST_cgmath_curve_bezier, GetControlPoint)
{
	CurveBezier curve;
	curve.addControlPoint(1.f, 2.f, 3.f);
	curve.addControlPoint(4.f, 5.f, 6.f);

	Vector3f v;
	bool res = curve.getControlPoint(0, v);

	EXPECT_TRUE(res);
	EXPECT_FLOAT_EQ(v.x, 1.f);
	EXPECT_FLOAT_EQ(v.y, 2.f);
	EXPECT_FLOAT_EQ(v.z, 3.f);
}

TEST(TEST_cgmath_curve_bezier, GetControlPointOutOfRange)
{
	CurveBezier curve;
	curve.addControlPoint(1.f, 2.f, 3.f);

	Vector3f v;
	EXPECT_FALSE(curve.getControlPoint(5, v));
	EXPECT_FALSE(curve.getControlPoint(-1, v));
}

TEST(TEST_cgmath_curve_bezier, GetDegree)
{
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(1.f, 1.f, 0.f);
	curve.addControlPoint(2.f, 0.f, 0.f);
	curve.addControlPoint(3.f, 1.f, 0.f);

	EXPECT_EQ(curve.getDegree(), 3);
}

TEST(TEST_cgmath_curve_bezier, EvalLinear)
{
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(1.f, 1.f, 0.f);

	Vector3f pt;
	EXPECT_TRUE(curve.eval(0.f, pt));

	EXPECT_FLOAT_EQ(pt.x, 0.f);
	EXPECT_FLOAT_EQ(pt.y, 0.f);
	EXPECT_FLOAT_EQ(pt.z, 0.f);
}

TEST(TEST_cgmath_curve_bezier, EvalLinearEnd)
{
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(1.f, 1.f, 0.f);

	Vector3f pt;
	curve.eval(1.f, pt);

	EXPECT_FLOAT_EQ(pt.x, 1.f);
	EXPECT_FLOAT_EQ(pt.y, 1.f);
	EXPECT_FLOAT_EQ(pt.z, 0.f);
}

TEST(TEST_cgmath_curve_bezier, EvalLinearMid)
{
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(2.f, 4.f, 0.f);

	Vector3f pt;
	curve.eval(0.5f, pt);

	EXPECT_FLOAT_EQ(pt.x, 1.f);
	EXPECT_FLOAT_EQ(pt.y, 2.f);
	EXPECT_FLOAT_EQ(pt.z, 0.f);
}

TEST(TEST_cgmath_curve_bezier, EvalEmpty)
{
	CurveBezier curve;
	Vector3f pt;
	EXPECT_FALSE(curve.eval(0.5f, pt));
}

TEST(TEST_cgmath_curve_bezier, CopyConstructor)
{
	CurveBezier curve;
	curve.addControlPoint(1.f, 2.f, 3.f);
	curve.addControlPoint(4.f, 5.f, 6.f);

	CurveBezier copy(curve);

	EXPECT_EQ(copy.getDegree(), 1);
	Vector3f v;
	copy.getControlPoint(0, v);
	EXPECT_FLOAT_EQ(v.x, 1.f);
	EXPECT_FLOAT_EQ(v.y, 2.f);
	EXPECT_FLOAT_EQ(v.z, 3.f);
}

TEST(TEST_cgmath_curve_bezier, Tessellate)
{
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(1.f, 1.f, 0.f);
	curve.addControlPoint(2.f, 0.f, 0.f);

	std::vector<Vector3f> pts;
	int n = curve.tessellate(3, pts);

	EXPECT_EQ(n, 3);
	ASSERT_EQ(pts.size(), 3u);
	EXPECT_FLOAT_EQ(pts[0].x, 0.f);
	EXPECT_FLOAT_EQ(pts[0].y, 0.f);
	EXPECT_FLOAT_EQ(pts[2].x, 2.f);
	EXPECT_FLOAT_EQ(pts[2].y, 0.f);
}

TEST(TEST_cgmath_curve_bezier, TessellateTooFewPoints)
{
	CurveBezier curve;
	curve.addControlPoint(0.f, 0.f, 0.f);
	curve.addControlPoint(1.f, 1.f, 0.f);

	std::vector<Vector3f> pts;
	EXPECT_EQ(curve.tessellate(1, pts), 0);
}

TEST(TEST_cgmath_curve_bezier, DynamicGrowth)
{
	CurveBezier curve;

	for (int i = 0; i < 10; i++)
		curve.addControlPoint((float)i, (float)i, 0.f);

	EXPECT_EQ(curve.getDegree(), 9);
}
