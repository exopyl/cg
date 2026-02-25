#include <gtest/gtest.h>

#include "../src/cgmath/curve_discrete.h"

TEST(TEST_cgmath_curve_discrete, Init)
{
	// context
	CurveDiscrete curve;

	// action
	int res = curve.init(5);

	// expectations
	EXPECT_EQ(res, 0);
}

TEST(TEST_cgmath_curve_discrete, SetPosition)
{
	// context
	CurveDiscrete curve;
	curve.init(3);

	// action
	int res0 = curve.set_position(0, 1.f, 2.f, 3.f);
	int res1 = curve.set_position(1, 4.f, 5.f, 6.f);
	int res2 = curve.set_position(2, 7.f, 8.f, 9.f);

	// expectations
	EXPECT_EQ(res0, 0);
	EXPECT_EQ(res1, 0);
	EXPECT_EQ(res2, 0);
}

TEST(TEST_cgmath_curve_discrete, SetPositionOutOfRange)
{
	// context
	CurveDiscrete curve;
	curve.init(3);

	// action
	int res = curve.set_position(5, 1.f, 2.f, 3.f);

	// expectations
	EXPECT_EQ(res, -1);
}

TEST(TEST_cgmath_curve_discrete, SetPositionNegativeIndex)
{
	// context
	CurveDiscrete curve;
	curve.init(3);

	// action
	int res = curve.set_position(-1, 1.f, 2.f, 3.f);

	// expectations
	EXPECT_EQ(res, -1);
}

TEST(TEST_cgmath_curve_discrete, SetPositionNotInitialized)
{
	// context
	CurveDiscrete curve;

	// action
	int res = curve.set_position(0, 1.f, 2.f, 3.f);

	// expectations
	EXPECT_EQ(res, -1);
}

TEST(TEST_cgmath_curve_discrete, InverseOrder)
{
	// context
	CurveDiscrete curve;
	curve.init(3);
	curve.set_position(0, 1.f, 0.f, 0.f);
	curve.set_position(1, 2.f, 0.f, 0.f);
	curve.set_position(2, 3.f, 0.f, 0.f);

	// action
	int res = curve.inverse_order();

	// expectations
	EXPECT_EQ(res, 0);
}

TEST(TEST_cgmath_curve_discrete, GenerateSurfaceRevolution)
{
	// context
	CurveDiscrete curve;
	curve.init(3);
	curve.set_position(0, 1.f, 0.f, 0.f);
	curve.set_position(1, 1.f, 0.f, 1.f);
	curve.set_position(2, 1.f, 0.f, 2.f);

	// action
	unsigned int nVertices = 0, nFaces = 0;
	float *pVertices = nullptr;
	unsigned int *pFaces = nullptr;
	int res = curve.generate_surface_revolution(8, &nVertices, &pVertices, &nFaces, &pFaces);

	// expectations
	EXPECT_EQ(res, 0);
	EXPECT_EQ(nVertices, 24u);  // 8 slices * 3 points
	EXPECT_EQ(nFaces, 16u);     // 8 slices * (3-1) segments
	EXPECT_NE(pVertices, nullptr);
	EXPECT_NE(pFaces, nullptr);
	free(pVertices);
	free(pFaces);
}

TEST(TEST_cgmath_curve_discrete, GenerateFrame)
{
	// context
	CurveDiscrete curve;
	curve.init(2);
	curve.set_position(0, 0.5f, 0.f, 0.f);
	curve.set_position(1, 0.5f, 0.f, 1.f);

	// action
	unsigned int nVertices = 0, nFaces = 0;
	float *pVertices = nullptr;
	unsigned int *pFaces = nullptr;
	int res = curve.generate_frame(2.f, 2.f, &nVertices, &pVertices, &nFaces, &pFaces);

	// expectations
	EXPECT_EQ(res, 0);
	EXPECT_EQ(nVertices, 8u);  // 4 corners * 2 points
	EXPECT_EQ(nFaces, 8u);     // 4 corners * 2 points
	EXPECT_NE(pVertices, nullptr);
	EXPECT_NE(pFaces, nullptr);
	free(pVertices);
	free(pFaces);
}

TEST(TEST_cgmath_curve_discrete, ReInit)
{
	// context
	CurveDiscrete curve;
	curve.init(3);
	curve.set_position(0, 1.f, 2.f, 3.f);

	// action - reinit with different size
	int res = curve.init(5);

	// expectations
	EXPECT_EQ(res, 0);
}
