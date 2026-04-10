#include <cmath>

#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

// ---------------------------------------------------------------------------
// Helper: create a unit square (0,0)(1,0)(1,1)(0,1)
// ---------------------------------------------------------------------------
static void make_unit_square(Polygon2 &pol)
{
	float pts[] = { 0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f };
	pol.input(pts, 4);
}

// ---------------------------------------------------------------------------
// Helper: create a right triangle (0,0)(1,0)(0,1)
// ---------------------------------------------------------------------------
static void make_triangle(Polygon2 &pol)
{
	float pts[] = { 0.f, 0.f, 1.f, 0.f, 0.f, 1.f };
	pol.input(pts, 3);
}

// ---------------------------------------------------------------------------
// Helper: free tesselation output
// ---------------------------------------------------------------------------
static void free_tesselation(float *pVertices, unsigned int *pFaces)
{
	if (pVertices) free(pVertices);
	if (pFaces) free(pFaces);
}

// ---------------------------------------------------------------------------
// Helper: check tesselation result consistency
// ---------------------------------------------------------------------------
static void check_tesselation_consistency(float *pVertices, unsigned int nVertices,
	unsigned int *pFaces, unsigned int nFaces, unsigned int minInputVertices)
{
	// vertices count should be at least the input vertices
	EXPECT_GE(nVertices, minInputVertices);

	// all face indices should reference valid vertices
	for (unsigned int i = 0; i < nFaces; i++)
	{
		EXPECT_LT(pFaces[3 * i],     nVertices) << "face=" << i << " vertex=0";
		EXPECT_LT(pFaces[3 * i + 1], nVertices) << "face=" << i << " vertex=1";
		EXPECT_LT(pFaces[3 * i + 2], nVertices) << "face=" << i << " vertex=2";
	}

	// all z coordinates should be 0 (2D polygon)
	for (unsigned int i = 0; i < nVertices; i++)
		EXPECT_FLOAT_EQ(pVertices[3 * i + 2], 0.f) << "vertex=" << i << " z!=0";
}

// ---------------------------------------------------------------------------
// Helper: compute total area of tesselated triangles (unsigned)
// ---------------------------------------------------------------------------
static float tesselation_area(float *pVertices, unsigned int *pFaces, unsigned int nFaces)
{
	float totalArea = 0.f;
	for (unsigned int i = 0; i < nFaces; i++)
	{
		unsigned int i0 = pFaces[3 * i];
		unsigned int i1 = pFaces[3 * i + 1];
		unsigned int i2 = pFaces[3 * i + 2];

		float x0 = pVertices[3 * i0], y0 = pVertices[3 * i0 + 1];
		float x1 = pVertices[3 * i1], y1 = pVertices[3 * i1 + 1];
		float x2 = pVertices[3 * i2], y2 = pVertices[3 * i2 + 1];

		// signed area of triangle via cross product
		float a = 0.5f * ((x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0));
		totalArea += a;
	}
	return totalArea;
}

// ---------------------------------------------------------------------------
// TEST: tesselate a triangle (simplest polygon)
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon_tesselate, Triangle)
{
	// context
	Polygon2 pol;
	make_triangle(pol);
	float *pVertices = nullptr;
	unsigned int nVertices = 0;
	unsigned int *pFaces = nullptr;
	unsigned int nFaces = 0;

	// action
	int ret = pol.tesselate(&pVertices, &nVertices, &pFaces, &nFaces);

	// expectations
	EXPECT_EQ(ret, 0);
	EXPECT_NE(pVertices, nullptr);
	EXPECT_NE(pFaces, nullptr);
	EXPECT_GE(nVertices, 3u);
	EXPECT_GE(nFaces, 1u);

	check_tesselation_consistency(pVertices, nVertices, pFaces, nFaces, 3);

	// a triangle should produce exactly 1 face
	EXPECT_EQ(nFaces, 1u);

	// area should match: 0.5
	float area = fabsf(tesselation_area(pVertices, pFaces, nFaces));
	EXPECT_NEAR(area, 0.5f, 1e-4f);

	free_tesselation(pVertices, pFaces);
}

// ---------------------------------------------------------------------------
// TEST: tesselate a unit square
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon_tesselate, UnitSquare)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float *pVertices = nullptr;
	unsigned int nVertices = 0;
	unsigned int *pFaces = nullptr;
	unsigned int nFaces = 0;

	// action
	int ret = pol.tesselate(&pVertices, &nVertices, &pFaces, &nFaces);

	// expectations
	EXPECT_EQ(ret, 0);
	EXPECT_GE(nVertices, 4u);
	// a convex quad should produce exactly 2 triangles
	EXPECT_EQ(nFaces, 2u);

	check_tesselation_consistency(pVertices, nVertices, pFaces, nFaces, 4);

	// total area should be 1.0
	float area = fabsf(tesselation_area(pVertices, pFaces, nFaces));
	EXPECT_NEAR(area, 1.f, 1e-4f);

	free_tesselation(pVertices, pFaces);
}

// ---------------------------------------------------------------------------
// TEST: tesselate a concave L-shape polygon
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon_tesselate, ConcaveLShape)
{
	// context
	// L-shape: (0,0)(2,0)(2,1)(1,1)(1,2)(0,2) -- area = 3.0
	Polygon2 pol;
	float pts[] = {
		0.f, 0.f,
		2.f, 0.f,
		2.f, 1.f,
		1.f, 1.f,
		1.f, 2.f,
		0.f, 2.f
	};
	pol.input(pts, 6);
	float *pVertices = nullptr;
	unsigned int nVertices = 0;
	unsigned int *pFaces = nullptr;
	unsigned int nFaces = 0;

	// action
	int ret = pol.tesselate(&pVertices, &nVertices, &pFaces, &nFaces);

	// expectations
	EXPECT_EQ(ret, 0);
	EXPECT_GE(nVertices, 6u);
	// L-shape with 6 vertices should produce 4 triangles
	EXPECT_GE(nFaces, 4u);

	check_tesselation_consistency(pVertices, nVertices, pFaces, nFaces, 6);

	// total area should be 3.0
	float area = fabsf(tesselation_area(pVertices, pFaces, nFaces));
	EXPECT_NEAR(area, 3.f, 1e-3f);

	free_tesselation(pVertices, pFaces);
}

// ---------------------------------------------------------------------------
// TEST: tesselate a convex pentagon
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon_tesselate, ConvexPentagon)
{
	// context -- regular pentagon centered at origin, radius 1
	Polygon2 pol;
	float pts[10];
	for (int i = 0; i < 5; i++)
	{
		float angle = 2.f * 3.14159265f * i / 5.f;
		pts[2 * i]     = cosf(angle);
		pts[2 * i + 1] = sinf(angle);
	}
	pol.input(pts, 5);
	float *pVertices = nullptr;
	unsigned int nVertices = 0;
	unsigned int *pFaces = nullptr;
	unsigned int nFaces = 0;

	// action
	int ret = pol.tesselate(&pVertices, &nVertices, &pFaces, &nFaces);

	// expectations
	EXPECT_EQ(ret, 0);
	EXPECT_GE(nVertices, 5u);
	// convex polygon with n vertices -> n-2 triangles
	EXPECT_EQ(nFaces, 3u);

	check_tesselation_consistency(pVertices, nVertices, pFaces, nFaces, 5);

	// area of regular pentagon with radius 1: 5/2 * sin(2*pi/5) ~ 2.37764
	float expectedArea = 2.5f * sinf(2.f * 3.14159265f / 5.f);
	float area = fabsf(tesselation_area(pVertices, pFaces, nFaces));
	EXPECT_NEAR(area, expectedArea, 1e-2f);

	free_tesselation(pVertices, pFaces);
}

// ---------------------------------------------------------------------------
// TEST: tesselate preserves input vertex coordinates
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon_tesselate, PreservesInputVertices)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float *pVertices = nullptr;
	unsigned int nVertices = 0;
	unsigned int *pFaces = nullptr;
	unsigned int nFaces = 0;

	// action
	pol.tesselate(&pVertices, &nVertices, &pFaces, &nFaces);

	// expectations -- first 4 vertices should match the input
	float expected[][2] = { {0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f} };
	for (int i = 0; i < 4; i++)
	{
		EXPECT_FLOAT_EQ(pVertices[3 * i],     expected[i][0]) << "vertex=" << i << " x";
		EXPECT_FLOAT_EQ(pVertices[3 * i + 1], expected[i][1]) << "vertex=" << i << " y";
	}

	free_tesselation(pVertices, pFaces);
}

// ---------------------------------------------------------------------------
// TEST: tesselate returns 0 on success
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon_tesselate, ReturnValue)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float *pVertices = nullptr;
	unsigned int nVertices = 0;
	unsigned int *pFaces = nullptr;
	unsigned int nFaces = 0;

	// action
	int ret = pol.tesselate(&pVertices, &nVertices, &pFaces, &nFaces);

	// expectations
	EXPECT_EQ(ret, 0);

	free_tesselation(pVertices, pFaces);
}

// ---------------------------------------------------------------------------
// TEST: tesselate with polygon loaded from file
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon_tesselate, FromFile)
{
	// context
	Polygon2 pol;
	pol.input((char *)"./test/data/polygon1.dat");
	if (pol.get_n_contours() == 0)
		GTEST_SKIP() << "polygon1.dat not found or empty";

	float *pVertices = nullptr;
	unsigned int nVertices = 0;
	unsigned int *pFaces = nullptr;
	unsigned int nFaces = 0;

	// action
	int ret = pol.tesselate(&pVertices, &nVertices, &pFaces, &nFaces);

	// expectations
	EXPECT_EQ(ret, 0);
	EXPECT_GE(nVertices, (unsigned int)pol.get_n_points());
	EXPECT_GE(nFaces, 1u);

	check_tesselation_consistency(pVertices, nVertices, pFaces, nFaces, pol.get_n_points());

	// area from tesselation should be close to polygon area
	float polArea = fabsf(pol.area());
	if (polArea > 0.f)
	{
		float tessArea = fabsf(tesselation_area(pVertices, pFaces, nFaces));
		EXPECT_NEAR(tessArea, polArea, polArea * 0.05f);
	}

	free_tesselation(pVertices, pFaces);
}

// ---------------------------------------------------------------------------
// TEST: tesselate produces all-triangles (3 indices per face)
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon_tesselate, AllTriangles_NoDegenerate)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float *pVertices = nullptr;
	unsigned int nVertices = 0;
	unsigned int *pFaces = nullptr;
	unsigned int nFaces = 0;

	// action
	pol.tesselate(&pVertices, &nVertices, &pFaces, &nFaces);

	// expectations -- no degenerate triangles (all 3 indices distinct)
	for (unsigned int i = 0; i < nFaces; i++)
	{
		unsigned int i0 = pFaces[3 * i];
		unsigned int i1 = pFaces[3 * i + 1];
		unsigned int i2 = pFaces[3 * i + 2];
		EXPECT_NE(i0, i1) << "face=" << i << " degenerate (i0==i1)";
		EXPECT_NE(i1, i2) << "face=" << i << " degenerate (i1==i2)";
		EXPECT_NE(i0, i2) << "face=" << i << " degenerate (i0==i2)";
	}

	free_tesselation(pVertices, pFaces);
}
