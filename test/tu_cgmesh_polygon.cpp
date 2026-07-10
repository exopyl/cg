#include <cmath>
#include <cstring>
#include <cstdio>

#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

// ---------------------------------------------------------------------------
// Helper structures and comparison functions
// ---------------------------------------------------------------------------

struct PointRef {
	float x;
	float y;
};

struct PolygonRef {
	int nContours;
	int *nPoints;       // array of size nContours
	PointRef **points;  // array of arrays
};

static void expect_polygon_eq(const Polygon2 &pol, const PolygonRef &ref, float tol = 1e-5f)
{
	ASSERT_EQ((unsigned int)pol.m_contours.size(), (unsigned int)ref.nContours);
	for (int c = 0; c < ref.nContours; c++)
	{
		ASSERT_EQ((unsigned int)pol.m_contours[c].size(), (unsigned int)ref.nPoints[c]);
		for (int i = 0; i < ref.nPoints[c]; i++)
		{
			EXPECT_NEAR(pol.m_contours[c][i].x, ref.points[c][i].x, tol)
				<< "contour=" << c << " point=" << i << " coord=x";
			EXPECT_NEAR(pol.m_contours[c][i].y, ref.points[c][i].y, tol)
				<< "contour=" << c << " point=" << i << " coord=y";
		}
	}
}

// Unit square: (0,0) (1,0) (1,1) (0,1)
static void make_unit_square(Polygon2 &pol)
{
	float pts[] = { 0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f };
	pol.input(pts, 4);
}

// Right triangle: (0,0) (1,0) (0,1)
static void make_triangle(Polygon2 &pol)
{
	float pts[] = { 0.f, 0.f, 1.f, 0.f, 0.f, 1.f };
	pol.input(pts, 3);
}

// ===========================================================================
// Coverage for the algorithmic API that had no tests: matching (Arkin),
// offsetting (thicken/dilate), extrusion, symmetry detectors, clean.
// ===========================================================================

// Arkin turning-function distance: a shape matched against a copy of itself is
// ~0; a clearly different shape (triangle) is farther. (Both the OOB crash and
// the resampling bug that ignored the sample count are now fixed.)
TEST(TEST_cgmesh_polygon, MatchingArkin_SelfNearZero_DifferentLarger)
{
	Polygon2 squareA; make_unit_square(squareA);
	Polygon2 squareB; make_unit_square(squareB);
	Polygon2 tri;     make_triangle(tri);

	const int nn = 64;
	const float dSelf = squareA.matching_arkin(&squareB, nn);
	const float dDiff = squareA.matching_arkin(&tri,     nn);
	printf("matching_arkin: self=%g  square-vs-triangle=%g\n", dSelf, dDiff);

	EXPECT_GE(dSelf, 0.f);
	EXPECT_NEAR(dSelf, 0.f, 1e-2f) << "a shape vs a copy of itself must match with ~0 distance";
	EXPECT_GT(dDiff, dSelf)        << "a triangle must be farther from a square than the square is from itself";
}

// thicken() turns an open profile of N points into a closed band of 2N points.
TEST(TEST_cgmesh_polygon, Thicken_OpenProfile_DoublesPointCount)
{
	float pts[] = { 0.f, 0.f, 1.f, 0.f, 2.f, 0.f };   // 3-point open polyline
	Polygon2 profile; profile.input(pts, 3);

	Polygon2 band;
	band.thicken(&profile, 0.1f, 0.1f, /*bOpen=*/1);

	ASSERT_EQ(band.get_n_contours(), 1);
	EXPECT_EQ(band.get_n_points(0), 2 * profile.get_n_points(0));   // 6
	// all coordinates finite
	for (int i = 0; i < band.get_n_points(0); ++i)
	{
		float x, y; band.get_point(0, i, &x, &y);
		EXPECT_TRUE(std::isfinite(x) && std::isfinite(y));
	}
}

// dilate() offsets each vertex by d; point/contour counts are preserved and the
// geometry actually moves (the offset polygon differs from the source).
TEST(TEST_cgmesh_polygon, Dilate_PreservesCountsAndOffsets)
{
	Polygon2 square; make_unit_square(square);

	Polygon2 out;
	out.dilate(&square, 0.25f);

	ASSERT_EQ(out.get_n_contours(), 1);
	ASSERT_EQ(out.get_n_points(0), square.get_n_points(0));   // 4

	bool moved = false;
	for (int i = 0; i < out.get_n_points(0); ++i)
	{
		float xs, ys, xo, yo;
		square.get_point(0, i, &xs, &ys);
		out.get_point(0, i, &xo, &yo);
		EXPECT_TRUE(std::isfinite(xo) && std::isfinite(yo));
		if (std::fabs(xo - xs) > 1e-4f || std::fabs(yo - ys) > 1e-4f) moved = true;
	}
	EXPECT_TRUE(moved) << "dilate must offset the vertices";
}

// extrude() writes an OBJ prism: 2N vertices + side faces.
TEST(TEST_cgmesh_polygon, Extrude_WritesObjWithVerticesAndFaces)
{
	Polygon2 square; make_unit_square(square);
	const char* path = "./polygon_extrude.obj";
	square.extrude(2.0f, (char*)path);

	FILE* f = fopen(path, "r");
	ASSERT_NE(f, nullptr);
	unsigned int nv = 0, nf = 0;
	char line[256];
	while (fgets(line, sizeof(line), f))
	{
		if (line[0] == 'v' && line[1] == ' ') nv++;
		else if (line[0] == 'f' && line[1] == ' ') nf++;
	}
	fclose(f);
	std::remove(path);

	EXPECT_EQ(nv, 2u * 4u);   // bottom + top rings
	EXPECT_GT(nf, 0u);        // side quads
}

// clean() is currently a no-op stub (its body is under #ifdef USE_GLUTESS, which
// is not defined): it must return 0 and not crash. Pins the current behaviour.
TEST(TEST_cgmesh_polygon, Clean_IsNoOpStub_ReturnsZero)
{
	Polygon2 square; make_unit_square(square);
	Polygon2 out;
	EXPECT_EQ(out.clean(&square), 0);
}

// Zabrodsky symmetry: returns the assumed centre (0,0) and a finite axis slope.
TEST(TEST_cgmesh_polygon, SymmetryZabrodsky_CenteredSquare_FiniteAxis)
{
	// square centred on the origin (zabrodsky assumes centre at 0,0)
	float pts[] = { -1.f,-1.f, 1.f,-1.f, 1.f,1.f, -1.f,1.f };
	Polygon2 square; square.input(pts, 4);

	float xc = 9.f, yc = 9.f, slope = std::nanf("");
	square.search_symmetry_zabrodsky(&xc, &yc, &slope);

	EXPECT_FLOAT_EQ(xc, 0.f);
	EXPECT_FLOAT_EQ(yc, 0.f);
	EXPECT_TRUE(std::isfinite(slope)) << "a symmetric square must yield a finite symmetry-axis slope";
}

// Smoke tests: the signature / Pridmore symmetry detectors (void, output via
// side channels) must at least run on a valid polygon without crashing.
TEST(TEST_cgmesh_polygon, SymmetrySignature_Smoke)
{
	Polygon2 square; make_unit_square(square);
	square.search_symmetry_signature(SIGNATURE_CURVATURE, INTERPOLATION_LINEAR, 64);
	square.search_symmetry_signature(SIGNATURE_DEVIATION, INTERPOLATION_LINEAR, 64);
	SUCCEED();
}
TEST(TEST_cgmesh_polygon, SymmetryPridmore_Smoke)
{
	Polygon2 square; make_unit_square(square);
	square.search_symmetry_pridmore();
	SUCCEED();
}
TEST(TEST_cgmesh_polygon, SymmetryPridmoreBis_Smoke)
{
	Polygon2 square; make_unit_square(square);
	square.search_symmetry_pridmore_bis();
	SUCCEED();
}

// ---------------------------------------------------------------------------
// TEST: Default constructor
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, Constructor_Default)
{
	// context

	// action
	Polygon2 pol;

	// expectations
	EXPECT_EQ(pol.m_contours.size(), 0u);
	EXPECT_TRUE(pol.m_contours.empty());
}

// ---------------------------------------------------------------------------
// TEST: Copy constructor
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, Constructor_Copy)
{
	// context
	Polygon2 src;
	make_unit_square(src);

	// action
	Polygon2 copy(src);

	// expectations
	ASSERT_EQ(copy.m_contours.size(), 1u);
	ASSERT_EQ(copy.m_contours[0].size(), 4u);
	for (int i = 0; i < 4; i++)
	{
		EXPECT_FLOAT_EQ(copy.m_contours[0][i].x, src.m_contours[0][i].x);
		EXPECT_FLOAT_EQ(copy.m_contours[0][i].y, src.m_contours[0][i].y);
	}
	// verify deep copy: modifying copy must not affect src
	copy.m_contours[0][0].x = 999.f;
	EXPECT_FLOAT_EQ(src.m_contours[0][0].x, 0.f);
}

// ---------------------------------------------------------------------------
// TEST: Assignment operator
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, AssignmentOperator)
{
	// context
	Polygon2 src;
	make_unit_square(src);
	Polygon2 dst;

	// action
	dst = src;

	// expectations
	ASSERT_EQ(dst.m_contours.size(), 1u);
	ASSERT_EQ(dst.m_contours[0].size(), 4u);
	for (int i = 0; i < 4; i++)
	{
		EXPECT_FLOAT_EQ(dst.m_contours[0][i].x, src.m_contours[0][i].x);
		EXPECT_FLOAT_EQ(dst.m_contours[0][i].y, src.m_contours[0][i].y);
	}
	// verify deep copy
	dst.m_contours[0][0].x = 999.f;
	EXPECT_FLOAT_EQ(src.m_contours[0][0].x, 0.f);
}

// ---------------------------------------------------------------------------
// TEST: input(float*, int) -- interleaved array
// ---------------------------------------------------------------------------

struct InputInterleavedParams {
	float *pts;
	int n;
};

struct InputInterleavedExpected {
	int nContours;
	int nPoints;
	float *pts; // interleaved reference
};

static void check_input_interleaved(const InputInterleavedParams &in, const InputInterleavedExpected &exp)
{
	Polygon2 pol;
	pol.input(in.pts, in.n);

	ASSERT_EQ((unsigned int)pol.m_contours.size(), (unsigned int)exp.nContours);
	ASSERT_EQ((unsigned int)pol.m_contours[0].size(), (unsigned int)exp.nPoints);
	for (int i = 0; i < exp.nPoints; i++)
	{
		EXPECT_FLOAT_EQ(pol.m_contours[0][i].x, exp.pts[2 * i]);
		EXPECT_FLOAT_EQ(pol.m_contours[0][i].y, exp.pts[2 * i + 1]);
	}
}

TEST(TEST_cgmesh_polygon, InputInterleaved_Square)
{
	// context
	float pts[] = { 0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f };
	InputInterleavedParams in = { pts, 4 };
	InputInterleavedExpected exp = { 1, 4, pts };

	// action & expectations
	check_input_interleaved(in, exp);
}

TEST(TEST_cgmesh_polygon, InputInterleaved_SinglePoint)
{
	// context
	float pts[] = { 5.f, 3.f };
	InputInterleavedParams in = { pts, 1 };
	InputInterleavedExpected exp = { 1, 1, pts };

	// action & expectations
	check_input_interleaved(in, exp);
}

// ---------------------------------------------------------------------------
// TEST: input(float*, float*, int) -- separate x/y arrays
// ---------------------------------------------------------------------------

struct InputSeparateParams {
	float *x;
	float *y;
	int n;
};

static void check_input_separate(const InputSeparateParams &in)
{
	Polygon2 pol;
	pol.input(in.x, in.y, in.n);

	ASSERT_EQ(pol.m_contours.size(), 1u);
	ASSERT_EQ((unsigned int)pol.m_contours[0].size(), (unsigned int)in.n);
	for (int i = 0; i < in.n; i++)
	{
		EXPECT_FLOAT_EQ(pol.m_contours[0][i].x, in.x[i]);
		EXPECT_FLOAT_EQ(pol.m_contours[0][i].y, in.y[i]);
	}
}

TEST(TEST_cgmesh_polygon, InputSeparate_Square)
{
	// context
	float x[] = { 0.f, 1.f, 1.f, 0.f };
	float y[] = { 0.f, 0.f, 1.f, 1.f };
	InputSeparateParams in = { x, y, 4 };

	// action & expectations
	check_input_separate(in);
}

TEST(TEST_cgmesh_polygon, InputSeparate_SinglePoint)
{
	// context
	float x[] = { 7.f };
	float y[] = { -2.f };
	InputSeparateParams in = { x, y, 1 };

	// action & expectations
	check_input_separate(in);
}

// ---------------------------------------------------------------------------
// TEST: input(char*) -- from file
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, InputFromFile)
{
	// context
	char *filename = (char *)"./test/data/polygon1.dat";

	// action
	Polygon2 pol;
	pol.input(filename);

	// expectations
	EXPECT_GE(pol.m_contours.size(), 1u);
	EXPECT_GT(pol.m_contours[0].size(), 0u);
	EXPECT_NE(pol.m_contours[0].data(), nullptr);
}

// ---------------------------------------------------------------------------
// TEST: input(Polygon2*, interpolation_type, nn) -- reparameterization
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, InputReparameterize_Linear)
{
	// context
	Polygon2 src;
	make_unit_square(src);

	// action : resample the unit square to exactly 40 arc-length-uniform points
	Polygon2 dst;
	dst.input(&src, INTERPOLATION_LINEAR, 40);

	// expectations
	ASSERT_EQ(dst.m_contours.size(), 1u);
	EXPECT_EQ(dst.m_contours[0].size(), 40u);           // count now honours the argument
	// all samples lie on the square's boundary (x or y at 0 or 1)
	for (unsigned int i = 0; i < dst.m_contours[0].size(); ++i)
	{
		float x = dst.m_contours[0][i].x, y = dst.m_contours[0][i].y;
		bool onBoundary = (std::fabs(x) < 1e-4f || std::fabs(x-1.f) < 1e-4f ||
		                   std::fabs(y) < 1e-4f || std::fabs(y-1.f) < 1e-4f);
		EXPECT_TRUE(onBoundary) << "resampled point " << i << " = (" << x << "," << y << ") off the square";
	}
}

// ---------------------------------------------------------------------------
// TEST: alloc_contours
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, AllocContours)
{
	// context
	Polygon2 pol;

	// action
	pol.alloc_contours(3);

	// expectations
	EXPECT_EQ(pol.m_contours.size(), 3u);
	for (int i = 0; i < 3; i++)
	{
		EXPECT_EQ(pol.m_contours[i].size(), 0u);
	}
}

// ---------------------------------------------------------------------------
// TEST: add_contour -- update existing contour
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, AddContour_UpdateExisting)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float new_pts[] = { 10.f, 20.f, 30.f, 40.f, 50.f, 60.f };

	// action
	float *ret = pol.add_contour(0, 3, new_pts);

	// expectations
	EXPECT_NE(ret, nullptr);
	ASSERT_EQ(pol.m_contours[0].size(), 3u);
	EXPECT_FLOAT_EQ(pol.m_contours[0][0].x, 10.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][0].y, 20.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][2].x, 50.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][2].y, 60.f);
}

// ---------------------------------------------------------------------------
// TEST: add_contour -- add new contour beyond current count
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, AddContour_AddNew)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float new_pts[] = { 2.f, 3.f, 4.f, 5.f };

	// action
	float *ret = pol.add_contour(1, 2, new_pts);

	// expectations
	EXPECT_NE(ret, nullptr);
	ASSERT_EQ(pol.m_contours.size(), 2u);
	ASSERT_EQ(pol.m_contours[1].size(), 2u);
	EXPECT_FLOAT_EQ(pol.m_contours[1][0].x, 2.f);
	EXPECT_FLOAT_EQ(pol.m_contours[1][0].y, 3.f);
	EXPECT_FLOAT_EQ(pol.m_contours[1][1].x, 4.f);
	EXPECT_FLOAT_EQ(pol.m_contours[1][1].y, 5.f);
}

// ---------------------------------------------------------------------------
// TEST: add_contour -- with nullptr pPoints (allocates but does not copy)
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, AddContour_NullPoints)
{
	// context
	Polygon2 pol;
	pol.alloc_contours(1);

	// action
	float *ret = pol.add_contour(0, 5, nullptr);

	// expectations
	EXPECT_NE(ret, nullptr);
	EXPECT_EQ(pol.m_contours[0].size(), 5u);
}

// ---------------------------------------------------------------------------
// TEST: add_polygon2d
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, AddPolygon2d_Nominal)
{
	// context
	Polygon2 base;
	make_unit_square(base);
	Polygon2 extra;
	float pts[] = { 10.f, 10.f, 20.f, 20.f, 30.f, 30.f };
	extra.input(pts, 3);

	// action
	int ret = base.add_polygon2d(&extra);

	// expectations
	EXPECT_EQ(ret, 0);
	ASSERT_EQ(base.m_contours.size(), 2u);
	EXPECT_EQ(base.m_contours[0].size(), 4u);
	EXPECT_EQ(base.m_contours[1].size(), 3u);
	EXPECT_FLOAT_EQ(base.m_contours[1][0].x, 10.f);
}

TEST(TEST_cgmesh_polygon, AddPolygon2d_NullPointer)
{
	// context
	Polygon2 base;
	make_unit_square(base);

	// action
	int ret = base.add_polygon2d(nullptr);

	// expectations
	EXPECT_EQ(ret, 0);
	EXPECT_EQ(base.m_contours.size(), 1u);
}

// ---------------------------------------------------------------------------
// TEST: set_point / get_point
// ---------------------------------------------------------------------------

struct SetGetPointParams {
	unsigned int iContour;
	unsigned int iPoint;
	float x;
	float y;
};

struct SetGetPointExpected {
	int retSet;
	int retGet;
	float x;
	float y;
};

static void check_set_get_point(Polygon2 &pol, const SetGetPointParams &in, const SetGetPointExpected &exp)
{
	int retSet = pol.set_point(in.iContour, in.iPoint, in.x, in.y);
	EXPECT_EQ(retSet, exp.retSet);

	float gx = -1.f, gy = -1.f;
	int retGet = pol.get_point(in.iContour, in.iPoint, &gx, &gy);
	EXPECT_EQ(retGet, exp.retGet);
	if (exp.retGet == 0)
	{
		EXPECT_FLOAT_EQ(gx, exp.x);
		EXPECT_FLOAT_EQ(gy, exp.y);
	}
}

TEST(TEST_cgmesh_polygon, SetGetPoint_Nominal)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	SetGetPointParams in = { 0, 2, 42.f, 43.f };
	SetGetPointExpected exp = { 0, 0, 42.f, 43.f };

	// action & expectations
	check_set_get_point(pol, in, exp);
}

TEST(TEST_cgmesh_polygon, SetGetPoint_OutOfBoundsContour)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);

	// action
	int retSet = pol.set_point(5, 0, 1.f, 2.f);
	float gx, gy;
	int retGet = pol.get_point(5, 0, &gx, &gy);

	// expectations
	EXPECT_EQ(retSet, -1);
	EXPECT_EQ(retGet, -1);
}

TEST(TEST_cgmesh_polygon, SetGetPoint_OutOfBoundsPoint)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);

	// action
	int retSet = pol.set_point(0, 100, 1.f, 2.f);
	float gx, gy;
	int retGet = pol.get_point(0, 100, &gx, &gy);

	// expectations
	EXPECT_EQ(retSet, -1);
	EXPECT_EQ(retGet, -1);
}

// ---------------------------------------------------------------------------
// TEST: get_n_contours / get_n_points / get_points
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, Accessors_NContours_NPoints_GetPoints)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);

	// action
	int nc = pol.get_n_contours();
	int np0 = pol.get_n_points(0);
	int npTotal = pol.get_n_points();
	float *pts = pol.get_points(0);

	// expectations
	EXPECT_EQ(nc, 1);
	EXPECT_EQ(np0, 4);
	EXPECT_EQ(npTotal, 4);
	EXPECT_NE(pts, nullptr);
	EXPECT_FLOAT_EQ(pts[0], 0.f);
	EXPECT_FLOAT_EQ(pts[1], 0.f);
}

TEST(TEST_cgmesh_polygon, Accessors_MultiContour)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float extra[] = { 5.f, 5.f, 6.f, 6.f };
	Polygon2 pol2;
	pol2.input(extra, 2);
	pol.add_polygon2d(&pol2);

	// action
	int nc = pol.get_n_contours();
	int npTotal = pol.get_n_points();

	// expectations
	EXPECT_EQ(nc, 2);
	EXPECT_EQ(npTotal, 6);  // 4 + 2
}

TEST(TEST_cgmesh_polygon, Accessors_EmptyPolygon)
{
	// context
	Polygon2 pol;

	// action
	int nc = pol.get_n_contours();
	int npTotal = pol.get_n_points();

	// expectations
	EXPECT_EQ(nc, 0);
	EXPECT_EQ(npTotal, 0);
}

// ---------------------------------------------------------------------------
// TEST: get_bbox
// ---------------------------------------------------------------------------

struct BBoxExpected {
	float xmin, xmax, ymin, ymax;
};

static void check_bbox(Polygon2 &pol, const BBoxExpected &exp, float tol = 1e-5f)
{
	float xmin, xmax, ymin, ymax;
	pol.get_bbox(&xmin, &xmax, &ymin, &ymax);
	EXPECT_NEAR(xmin, exp.xmin, tol);
	EXPECT_NEAR(xmax, exp.xmax, tol);
	EXPECT_NEAR(ymin, exp.ymin, tol);
	EXPECT_NEAR(ymax, exp.ymax, tol);
}

TEST(TEST_cgmesh_polygon, GetBBox_UnitSquare)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	BBoxExpected exp = { 0.f, 1.f, 0.f, 1.f };

	// action & expectations
	check_bbox(pol, exp);
}

TEST(TEST_cgmesh_polygon, GetBBox_NegativeCoords)
{
	// context
	Polygon2 pol;
	float pts[] = { -3.f, -4.f, 5.f, 6.f, 0.f, 0.f };
	pol.input(pts, 3);
	BBoxExpected exp = { -3.f, 5.f, -4.f, 6.f };

	// action & expectations
	check_bbox(pol, exp);
}

TEST(TEST_cgmesh_polygon, GetBBox_SinglePoint)
{
	// context
	Polygon2 pol;
	float pts[] = { 7.f, 8.f };
	pol.input(pts, 1);
	BBoxExpected exp = { 7.f, 7.f, 8.f, 8.f };

	// action & expectations
	check_bbox(pol, exp);
}

// ---------------------------------------------------------------------------
// TEST: length
// ---------------------------------------------------------------------------

struct LengthParams {
	int interpolation_type;
};

struct LengthExpected {
	float value;
};

static void check_length(Polygon2 &pol, const LengthParams &in, const LengthExpected &exp, float tol = 1e-4f)
{
	float l = pol.length(in.interpolation_type);
	EXPECT_NEAR(l, exp.value, tol);
}

TEST(TEST_cgmesh_polygon, Length_UnitSquare_Linear)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	// perimeter of unit square = 4.0
	LengthParams in = { INTERPOLATION_LINEAR };
	LengthExpected exp = { 4.f };

	// action & expectations
	check_length(pol, in, exp);
}

TEST(TEST_cgmesh_polygon, Length_Triangle_Linear)
{
	// context
	Polygon2 pol;
	make_triangle(pol);
	// sides: 1 + 1 + sqrt(2) = 2 + sqrt(2) ~ 3.41421
	LengthParams in = { INTERPOLATION_LINEAR };
	LengthExpected exp = { 2.f + sqrtf(2.f) };

	// action & expectations
	check_length(pol, in, exp);
}

TEST(TEST_cgmesh_polygon, Length_Cosine_DefaultBranch)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	// INTERPOLATION_COSINE falls into default: returns 0
	LengthParams in = { INTERPOLATION_COSINE };
	LengthExpected exp = { 0.f };

	// action & expectations
	check_length(pol, in, exp);
}

// ---------------------------------------------------------------------------
// TEST: area(void)
// ---------------------------------------------------------------------------

struct AreaExpected {
	float value;
};

static void check_area(Polygon2 &pol, const AreaExpected &exp, float tol = 1e-4f)
{
	float a = pol.area();
	EXPECT_NEAR(a, exp.value, tol);
}

TEST(TEST_cgmesh_polygon, Area_UnitSquare)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	// unit square (0,0)(1,0)(1,1)(0,1): area = 1.0 (counter-clockwise)
	AreaExpected exp = { 1.f };

	// action & expectations
	check_area(pol, exp);
}

TEST(TEST_cgmesh_polygon, Area_Triangle)
{
	// context
	Polygon2 pol;
	make_triangle(pol);
	// triangle (0,0)(1,0)(0,1): area = 0.5 (counter-clockwise)
	AreaExpected exp = { 0.5f };

	// action & expectations
	check_area(pol, exp);
}

TEST(TEST_cgmesh_polygon, Area_ClockwiseSquare)
{
	// context -- clockwise ordering -> negative area
	Polygon2 pol;
	float pts[] = { 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f, 0.f };
	pol.input(pts, 4);
	AreaExpected exp = { -1.f };

	// action & expectations
	check_area(pol, exp);
}

TEST(TEST_cgmesh_polygon, Area_TwoPoints)
{
	// context -- degenerate: 2 points -> area = 0
	Polygon2 pol;
	float pts[] = { 0.f, 0.f, 1.f, 1.f };
	pol.input(pts, 2);
	AreaExpected exp = { 0.f };

	// action & expectations
	check_area(pol, exp);
}

TEST(TEST_cgmesh_polygon, Area_MultiContour_ReturnsZero)
{
	// context -- multiple contours -> area returns 0
	Polygon2 pol;
	make_unit_square(pol);
	float extra[] = { 2.f, 2.f, 3.f, 3.f, 4.f, 4.f };
	Polygon2 pol2;
	pol2.input(extra, 3);
	pol.add_polygon2d(&pol2);
	AreaExpected exp = { 0.f };

	// action & expectations
	check_area(pol, exp);
}

// ---------------------------------------------------------------------------
// TEST: area(i1, i2, i3)
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, AreaTriangleIndices_Nominal)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	// triangle formed by points 0(0,0), 1(1,0), 2(1,1)
	// cross product: (1,0,0)x(1,1,0) = (0,0,1) -> area = 0.5

	// action
	float a = pol.area(0, 1, 2);

	// expectations
	EXPECT_NEAR(a, 0.5f, 1e-5f);
}

TEST(TEST_cgmesh_polygon, AreaTriangleIndices_NegativeOrientation)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	// reverse order: triangle 0(0,0), 2(1,1), 1(1,0)
	// cross product: (1,1,0)x(1,0,0) = (0,0,-1) -> area = -0.5

	// action
	float a = pol.area(0, 2, 1);

	// expectations
	EXPECT_NEAR(a, -0.5f, 1e-5f);
}

// ---------------------------------------------------------------------------
// TEST: translate
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, Translate_Nominal)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);

	// action
	pol.translate(10.f, 20.f);

	// expectations
	PointRef ref_pts[] = { {10.f, 20.f}, {11.f, 20.f}, {11.f, 21.f}, {10.f, 21.f} };
	int nPts = 4;
	PolygonRef ref = { 1, &nPts, &ref_pts[0] ? new PointRef*[1] : nullptr };
	// Direct check instead of using PolygonRef for simplicity
	EXPECT_FLOAT_EQ(pol.m_contours[0][0].x, 10.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][0].y, 20.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][1].x, 11.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][1].y, 20.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][2].x, 11.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][2].y, 21.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][3].x, 10.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][3].y, 21.f);
}

TEST(TEST_cgmesh_polygon, Translate_Zero)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	Polygon2 before(pol);

	// action
	pol.translate(0.f, 0.f);

	// expectations
	const float *p = (const float*)pol.m_contours[0].data();
	const float *b = (const float*)before.m_contours[0].data();
	for (int i = 0; i < 8; i++)
		EXPECT_FLOAT_EQ(p[i], b[i]);
}

// ---------------------------------------------------------------------------
// TEST: rotate
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, Rotate_90Degrees)
{
	// context -- single point at (1, 0), rotate 90 degrees -> (0, 1)
	Polygon2 pol;
	float pts[] = { 1.f, 0.f };
	pol.input(pts, 1);

	// action
	pol.rotate(90.f);

	// expectations
	// Note: the code uses 3.14159 not M_PI, so there's a tiny imprecision
	EXPECT_NEAR(pol.m_contours[0][0].x, 0.f, 1e-4f);
	EXPECT_NEAR(pol.m_contours[0][0].y, 1.f, 1e-4f);
}

TEST(TEST_cgmesh_polygon, Rotate_180Degrees)
{
	// context -- point (1, 0), rotate 180 -> (-1, 0)
	Polygon2 pol;
	float pts[] = { 1.f, 0.f };
	pol.input(pts, 1);

	// action
	pol.rotate(180.f);

	// expectations
	EXPECT_NEAR(pol.m_contours[0][0].x, -1.f, 1e-4f);
	EXPECT_NEAR(pol.m_contours[0][0].y, 0.f, 1e-3f);
}

TEST(TEST_cgmesh_polygon, Rotate_360Degrees)
{
	// context -- full rotation should return to original
	Polygon2 pol;
	make_unit_square(pol);
	Polygon2 before(pol);

	// action
	pol.rotate(360.f);

	// expectations
	const float *p = (const float*)pol.m_contours[0].data();
	const float *b = (const float*)before.m_contours[0].data();
	for (unsigned int i = 0; i < 2 * pol.m_contours[0].size(); i++)
		EXPECT_NEAR(p[i], b[i], 1e-3f);
}

// ---------------------------------------------------------------------------
// TEST: center
// ---------------------------------------------------------------------------

struct CenterExpected {
	float xc;
	float yc;
};

static void check_center(Polygon2 &pol, const CenterExpected &exp, float tol = 1e-5f)
{
	float xc, yc;
	pol.center(&xc, &yc);
	EXPECT_NEAR(xc, exp.xc, tol);
	EXPECT_NEAR(yc, exp.yc, tol);
}

TEST(TEST_cgmesh_polygon, Center_UnitSquare)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	CenterExpected exp = { 0.5f, 0.5f };

	// action & expectations
	check_center(pol, exp);
}

TEST(TEST_cgmesh_polygon, Center_Triangle)
{
	// context
	Polygon2 pol;
	make_triangle(pol);
	// centroid of (0,0)(1,0)(0,1) = (1/3, 1/3)
	CenterExpected exp = { 1.f / 3.f, 1.f / 3.f };

	// action & expectations
	check_center(pol, exp);
}

// ---------------------------------------------------------------------------
// TEST: centerize
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, Centerize_UnitSquare)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);

	// action
	pol.centerize();

	// expectations -- centroid should now be at origin
	float xc, yc;
	pol.center(&xc, &yc);
	EXPECT_NEAR(xc, 0.f, 1e-5f);
	EXPECT_NEAR(yc, 0.f, 1e-5f);
	// Individual points: shifted by (-0.5, -0.5)
	EXPECT_NEAR(pol.m_contours[0][0].x, -0.5f, 1e-5f);
	EXPECT_NEAR(pol.m_contours[0][0].y, -0.5f, 1e-5f);
	EXPECT_NEAR(pol.m_contours[0][1].x,  0.5f, 1e-5f);
	EXPECT_NEAR(pol.m_contours[0][1].y, -0.5f, 1e-5f);
}

TEST(TEST_cgmesh_polygon, Centerize_AlreadyCentered)
{
	// context -- centered square: (-1,-1)(1,-1)(1,1)(-1,1)
	Polygon2 pol;
	float pts[] = { -1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, 1.f };
	pol.input(pts, 4);

	// action
	pol.centerize();

	// expectations -- should remain unchanged
	EXPECT_NEAR(pol.m_contours[0][0].x, -1.f, 1e-5f);
	EXPECT_NEAR(pol.m_contours[0][0].y, -1.f, 1e-5f);
}

// ---------------------------------------------------------------------------
// TEST: flip_x
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, FlipX_UnitSquare)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);

	// action
	pol.flip_x();

	// expectations -- x coordinates negated
	EXPECT_FLOAT_EQ(pol.m_contours[0][0].x,  0.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][0].y,  0.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][1].x, -1.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][1].y,  0.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][2].x, -1.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][2].y,  1.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][3].x,  0.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][3].y,  1.f);
}

TEST(TEST_cgmesh_polygon, FlipX_DoubleFlipRestoresOriginal)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	Polygon2 before(pol);

	// action
	pol.flip_x();
	pol.flip_x();

	// expectations
	const float *p = (const float*)pol.m_contours[0].data();
	const float *b = (const float*)before.m_contours[0].data();
	for (unsigned int i = 0; i < 2 * pol.m_contours[0].size(); i++)
		EXPECT_FLOAT_EQ(p[i], b[i]);
}

// ---------------------------------------------------------------------------
// TEST: smooth
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, Smooth_Triangle)
{
	// context -- triangle (0,0)(1,0)(0,1)
	// smooth for closed polygon: each point becomes average of its two neighbors
	// new[0] = avg(pts[2], pts[1]) in a specific way (see implementation)
	Polygon2 pol;
	make_triangle(pol);

	// Compute expected values manually from the implementation:
	// smooth x:
	//   t[0] = (pts[2*(3-1)] + pts[1]) / 2 = (pts[4] + pts[1]) / 2 = (0 + 0) / 2 = 0
	//   Wait: pts[1] here is actually pPoints[1] which is y of point 0 = 0.
	//   Actually, looking at the code more carefully:
	//   t[0]   = (pPoints[2*(nPoints-1)] + pPoints[1]) / 2
	//          = (pPoints[4] + pPoints[1]) / 2 = (0 + 0) / 2 = 0
	//   t[2*(3-1)] = t[4] = (pPoints[2*(3-2)] + pPoints[0]) / 2
	//          = (pPoints[2] + pPoints[0]) / 2 = (1 + 0) / 2 = 0.5
	//   t[2*1] = t[2] = (pPoints[2*0] + pPoints[2*2]) / 2
	//          = (pPoints[0] + pPoints[4]) / 2 = (0 + 0) / 2 = 0
	//
	// smooth y:
	//   t[1]   = (pPoints[2*(3-1)+1] + pPoints[3]) / 2
	//          = (pPoints[5] + pPoints[3]) / 2 = (1 + 0) / 2 = 0.5
	//   t[2*(3-1)+1] = t[5] = (pPoints[2*(3-2)+1] + pPoints[1]) / 2
	//          = (pPoints[3] + pPoints[1]) / 2 = (0 + 0) / 2 = 0
	//   t[2*1+1] = t[3] = (pPoints[2*0+1] + pPoints[2*2+1]) / 2
	//          = (pPoints[1] + pPoints[5]) / 2 = (0 + 1) / 2 = 0.5
	//
	// Result: (0, 0.5), (0, 0.5), (0.5, 0)

	// action
	pol.smooth();

	// expectations
	EXPECT_NEAR(pol.m_contours[0][0].x, 0.f,  1e-5f);  // x0
	EXPECT_NEAR(pol.m_contours[0][0].y, 0.5f, 1e-5f);  // y0
	EXPECT_NEAR(pol.m_contours[0][1].x, 0.f,  1e-5f);  // x1
	EXPECT_NEAR(pol.m_contours[0][1].y, 0.5f, 1e-5f);  // y1
	EXPECT_NEAR(pol.m_contours[0][2].x, 0.5f, 1e-5f);  // x2
	EXPECT_NEAR(pol.m_contours[0][2].y, 0.f,  1e-5f);  // y2
}

// ---------------------------------------------------------------------------
// TEST: is_point_inside
// ---------------------------------------------------------------------------

struct PointInsideParams {
	float x;
	float y;
};

struct PointInsideExpected {
	int result;
};

static void check_point_inside(Polygon2 &pol, const PointInsideParams &in, const PointInsideExpected &exp)
{
	int r = pol.is_point_inside(in.x, in.y);
	EXPECT_EQ(r, exp.result);
}

TEST(TEST_cgmesh_polygon, IsPointInside_InsideSquare)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	PointInsideParams in = { 0.5f, 0.5f };
	PointInsideExpected exp = { 1 };

	// action & expectations
	check_point_inside(pol, in, exp);
}

TEST(TEST_cgmesh_polygon, IsPointInside_OutsideSquare)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	PointInsideParams in = { 5.f, 5.f };
	PointInsideExpected exp = { 0 };

	// action & expectations
	check_point_inside(pol, in, exp);
}

TEST(TEST_cgmesh_polygon, IsPointInside_OutsideNegative)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	PointInsideParams in = { -1.f, -1.f };
	PointInsideExpected exp = { 0 };

	// action & expectations
	check_point_inside(pol, in, exp);
}

TEST(TEST_cgmesh_polygon, IsPointInside_NearCorner)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	PointInsideParams in = { 0.01f, 0.01f };
	PointInsideExpected exp = { 1 };

	// action & expectations
	check_point_inside(pol, in, exp);
}

TEST(TEST_cgmesh_polygon, IsPointInside_InsideTriangle)
{
	// context
	Polygon2 pol;
	make_triangle(pol);
	PointInsideParams in = { 0.1f, 0.1f };
	PointInsideExpected exp = { 1 };

	// action & expectations
	check_point_inside(pol, in, exp);
}

TEST(TEST_cgmesh_polygon, IsPointInside_OutsideTriangle)
{
	// context
	Polygon2 pol;
	make_triangle(pol);
	PointInsideParams in = { 0.9f, 0.9f };
	PointInsideExpected exp = { 0 };

	// action & expectations
	check_point_inside(pol, in, exp);
}

// ---------------------------------------------------------------------------
// TEST: inverse_order
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, InverseOrder_Square)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	// original: (0,0)(1,0)(1,1)(0,1)

	// action
	pol.inverse_order();

	// expectations: reversed -> (0,1)(1,1)(1,0)(0,0)
	EXPECT_FLOAT_EQ(pol.m_contours[0][0].x, 0.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][0].y, 1.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][1].x, 1.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][1].y, 1.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][2].x, 1.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][2].y, 0.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][3].x, 0.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][3].y, 0.f);
}

TEST(TEST_cgmesh_polygon, InverseOrder_DoubleInverseRestoresOriginal)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	Polygon2 before(pol);

	// action
	pol.inverse_order();
	pol.inverse_order();

	// expectations
	const float *p = (const float*)pol.m_contours[0].data();
	const float *b = (const float*)before.m_contours[0].data();
	for (unsigned int i = 0; i < 2 * pol.m_contours[0].size(); i++)
		EXPECT_FLOAT_EQ(p[i], b[i]);
}

TEST(TEST_cgmesh_polygon, InverseOrder_Triangle)
{
	// context
	Polygon2 pol;
	make_triangle(pol);
	// original: (0,0)(1,0)(0,1) -- 3 points, only swap 0<->2

	// action
	pol.inverse_order();

	// expectations: reversed -> (0,1)(1,0)(0,0)
	EXPECT_FLOAT_EQ(pol.m_contours[0][0].x, 0.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][0].y, 1.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][1].x, 1.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][1].y, 0.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][2].x, 0.f);
	EXPECT_FLOAT_EQ(pol.m_contours[0][2].y, 0.f);
}

// ---------------------------------------------------------------------------
// TEST: is_trigonometric_order
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, IsTrigonometricOrder_CCW)
{
	// context -- CCW square: area > 0
	Polygon2 pol;
	make_unit_square(pol);

	// action
	int result = pol.is_trigonometric_order();

	// expectations
	EXPECT_EQ(result, 1);
}

TEST(TEST_cgmesh_polygon, IsTrigonometricOrder_CW)
{
	// context -- CW square: area < 0
	Polygon2 pol;
	float pts[] = { 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f, 0.f };
	pol.input(pts, 4);

	// action
	int result = pol.is_trigonometric_order();

	// expectations
	EXPECT_EQ(result, 0);
}

// ---------------------------------------------------------------------------
// TEST: moment_0
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, Moment0_Square)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float m[1];

	// action
	pol.moment_0(m);

	// expectations
	EXPECT_FLOAT_EQ(m[0], 4.f);
}

TEST(TEST_cgmesh_polygon, Moment0_Triangle)
{
	// context
	Polygon2 pol;
	make_triangle(pol);
	float m[1];

	// action
	pol.moment_0(m);

	// expectations
	EXPECT_FLOAT_EQ(m[0], 3.f);
}

// ---------------------------------------------------------------------------
// TEST: moment_1
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, Moment1_UnitSquare)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float m[2];

	// action
	pol.moment_1(m);

	// expectations -- centroid of (0,0)(1,0)(1,1)(0,1)
	EXPECT_NEAR(m[0], 0.5f, 1e-5f);
	EXPECT_NEAR(m[1], 0.5f, 1e-5f);
}

TEST(TEST_cgmesh_polygon, Moment1_Triangle)
{
	// context
	Polygon2 pol;
	make_triangle(pol);
	float m[2];

	// action
	pol.moment_1(m);

	// expectations -- centroid of (0,0)(1,0)(0,1) = (1/3, 1/3)
	EXPECT_NEAR(m[0], 1.f / 3.f, 1e-5f);
	EXPECT_NEAR(m[1], 1.f / 3.f, 1e-5f);
}

// ---------------------------------------------------------------------------
// TEST: moment_2
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, Moment2_UnitSquare_NonCentralized)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float m[3];

	// action
	pol.moment_2(m, false);

	// expectations
	// m20 = (0^2 + 1^2 + 1^2 + 0^2) / 4 = 2/4 = 0.5
	// m11 = (0*0 + 1*0 + 1*1 + 0*1) / 4 = 1/4 = 0.25
	// m02 = (0^2 + 0^2 + 1^2 + 1^2) / 4 = 2/4 = 0.5
	EXPECT_NEAR(m[0], 0.5f,  1e-5f);
	EXPECT_NEAR(m[1], 0.25f, 1e-5f);
	EXPECT_NEAR(m[2], 0.5f,  1e-5f);
}

TEST(TEST_cgmesh_polygon, Moment2_UnitSquare_Centralized)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float m[3];

	// action
	pol.moment_2(m, true);

	// expectations
	// non-centralized: m20=0.5, m11=0.25, m02=0.5
	// centroid: (0.5, 0.5)
	// centralized: m20 - 0.5*0.5 = 0.25, m11 - 0.5*0.5 = 0, m02 - 0.5*0.5 = 0.25
	EXPECT_NEAR(m[0], 0.25f, 1e-5f);
	EXPECT_NEAR(m[1], 0.f,   1e-5f);
	EXPECT_NEAR(m[2], 0.25f, 1e-5f);
}

// ---------------------------------------------------------------------------
// TEST: moment_3
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, Moment3_UnitSquare)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float m[4];

	// action
	pol.moment_3(m);

	// expectations (non-normalized sums)
	// points: (0,0),(1,0),(1,1),(0,1)
	// m30 = 0^3 + 1^3 + 1^3 + 0^3 = 2
	// m21 = 0^2*0 + 1^2*0 + 1^2*1 + 0^2*1 = 1
	// m12 = 0*0^2 + 1*0^2 + 1*1^2 + 0*1^2 = 1
	// m03 = 0^3 + 0^3 + 1^3 + 1^3 = 2
	EXPECT_NEAR(m[0], 2.f, 1e-5f);
	EXPECT_NEAR(m[1], 1.f, 1e-5f);
	EXPECT_NEAR(m[2], 1.f, 1e-5f);
	EXPECT_NEAR(m[3], 2.f, 1e-5f);
}

TEST(TEST_cgmesh_polygon, Moment3_Triangle)
{
	// context
	Polygon2 pol;
	make_triangle(pol);
	float m[4];

	// action
	pol.moment_3(m);

	// expectations
	// points: (0,0),(1,0),(0,1)
	// m30 = 0 + 1 + 0 = 1
	// m21 = 0 + 0 + 0 = 0
	// m12 = 0 + 0 + 0 = 0
	// m03 = 0 + 0 + 1 = 1
	EXPECT_NEAR(m[0], 1.f, 1e-5f);
	EXPECT_NEAR(m[1], 0.f, 1e-5f);
	EXPECT_NEAR(m[2], 0.f, 1e-5f);
	EXPECT_NEAR(m[3], 1.f, 1e-5f);
}

// ---------------------------------------------------------------------------
// TEST: generalized_barycentric_coordinates
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, GeneralizedBarycentricCoordinates_CenterOfSquare)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float pt[2] = { 0.5f, 0.5f };
	float coords[4];

	// action
	int ret = pol.generalized_barycentric_coordinates(pt, coords);

	// expectations
	EXPECT_EQ(ret, 0);
	// At center of square, all weights should be equal = 0.25
	float sum = 0.f;
	for (int i = 0; i < 4; i++)
	{
		EXPECT_NEAR(coords[i], 0.25f, 1e-4f);
		sum += coords[i];
	}
	EXPECT_NEAR(sum, 1.f, 1e-5f);
}

TEST(TEST_cgmesh_polygon, GeneralizedBarycentricCoordinates_MultiContour_ReturnsError)
{
	// context -- multi-contour polygon -> returns -1
	Polygon2 pol;
	make_unit_square(pol);
	float extra[] = { 2.f, 2.f, 3.f, 3.f, 4.f, 4.f };
	Polygon2 pol2;
	pol2.input(extra, 3);
	pol.add_polygon2d(&pol2);
	float pt[2] = { 0.5f, 0.5f };

	// action
	int ret = pol.generalized_barycentric_coordinates(pt, nullptr);

	// expectations
	EXPECT_EQ(ret, -1);
}

TEST(TEST_cgmesh_polygon, GeneralizedBarycentricCoordinates_SumToOne)
{
	// context -- off-center point in square
	Polygon2 pol;
	make_unit_square(pol);
	float pt[2] = { 0.3f, 0.7f };
	float coords[4];

	// action
	int ret = pol.generalized_barycentric_coordinates(pt, coords);

	// expectations
	EXPECT_EQ(ret, 0);
	float sum = 0.f;
	for (int i = 0; i < 4; i++)
		sum += coords[i];
	EXPECT_NEAR(sum, 1.f, 1e-5f);
}

// ---------------------------------------------------------------------------
// TEST: dump (just verify it does not crash)
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, Dump_DoesNotCrash)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);

	// action & expectations -- no crash
	pol.dump();
}

// ---------------------------------------------------------------------------
// TEST: Combined transformations
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, TranslateAndRotate_CombinedTransform)
{
	// context -- point (1,0) translated by (1,0) -> (2,0), rotated 90 -> (0,2)
	Polygon2 pol;
	float pts[] = { 1.f, 0.f };
	pol.input(pts, 1);

	// action
	pol.translate(1.f, 0.f);
	pol.rotate(90.f);

	// expectations
	EXPECT_NEAR(pol.m_contours[0][0].x, 0.f, 1e-3f);
	EXPECT_NEAR(pol.m_contours[0][0].y, 2.f, 1e-3f);
}

TEST(TEST_cgmesh_polygon, CenterizeAndFlip_CombinedTransform)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);

	// action
	pol.centerize();
	pol.flip_x();

	// expectations -- centroid after centerize is (0,0); flip_x negates x
	// After centerize: (-0.5,-0.5)(0.5,-0.5)(0.5,0.5)(-0.5,0.5)
	// After flip_x:    (0.5,-0.5)(-0.5,-0.5)(-0.5,0.5)(0.5,0.5)
	EXPECT_NEAR(pol.m_contours[0][0].x,  0.5f, 1e-5f);
	EXPECT_NEAR(pol.m_contours[0][0].y, -0.5f, 1e-5f);
	EXPECT_NEAR(pol.m_contours[0][1].x, -0.5f, 1e-5f);
	EXPECT_NEAR(pol.m_contours[0][1].y, -0.5f, 1e-5f);
}

// ---------------------------------------------------------------------------
// TEST: inverse_order changes area sign
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, InverseOrder_ChangesAreaSign)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float areaBefore = pol.area();

	// action
	pol.inverse_order();
	float areaAfter = pol.area();

	// expectations
	EXPECT_NEAR(areaAfter, -areaBefore, 1e-5f);
}

// ---------------------------------------------------------------------------
// TEST: is_trigonometric_order consistent with inverse_order
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, IsTrigonometricOrder_AfterInverse)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	ASSERT_EQ(pol.is_trigonometric_order(), 1);

	// action
	pol.inverse_order();

	// expectations
	EXPECT_EQ(pol.is_trigonometric_order(), 0);
}

// ---------------------------------------------------------------------------
// TEST: length preserving after translate
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, Length_InvariantUnderTranslation)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float lengthBefore = pol.length(INTERPOLATION_LINEAR);

	// action
	pol.translate(100.f, 200.f);
	float lengthAfter = pol.length(INTERPOLATION_LINEAR);

	// expectations
	EXPECT_NEAR(lengthAfter, lengthBefore, 1e-4f);
}

// ---------------------------------------------------------------------------
// TEST: length preserving after rotate
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, Length_InvariantUnderRotation)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float lengthBefore = pol.length(INTERPOLATION_LINEAR);

	// action
	pol.rotate(45.f);
	float lengthAfter = pol.length(INTERPOLATION_LINEAR);

	// expectations
	EXPECT_NEAR(lengthAfter, lengthBefore, 1e-3f);
}

// ---------------------------------------------------------------------------
// TEST: area preserving after translate
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, Area_InvariantUnderTranslation)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float areaBefore = pol.area();

	// action
	pol.translate(50.f, -30.f);
	float areaAfter = pol.area();

	// expectations
	EXPECT_NEAR(areaAfter, areaBefore, 1e-3f);
}

// ---------------------------------------------------------------------------
// TEST: area preserving after rotate
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_polygon, Area_InvariantUnderRotation)
{
	// context
	Polygon2 pol;
	make_unit_square(pol);
	float areaBefore = pol.area();

	// action
	pol.rotate(37.f);
	float areaAfter = pol.area();

	// expectations
	EXPECT_NEAR(areaAfter, areaBefore, 1e-3f);
}

// Characterization of Polygon2::apply_PCA. After PCA the principal axes of the
// point cloud align with x/y, so the xy cross-covariance vanishes and the
// spread is largest along x. The eigensolver behind apply_PCA changes from the
// C mat2 to TMatrix2; this pins the observable result. Points are centered at
// the origin so the (known) centering quirk in apply_PCA does not interfere.
TEST(TEST_cgmesh_polygon, ApplyPCA_DiagonalizesCovariance)
{
	float pts[] = { 2.f, 1.f,  -2.f, -1.f,   1.f, 0.4f,
	               -1.f, -0.4f, 0.6f, 0.3f, -0.6f, -0.3f };
	const int n = 6;
	Polygon2 pol;
	pol.add_contour(0, n, pts);

	pol.apply_PCA();

	float* out = pol.get_points(0);
	float mx = 0.f, my = 0.f;
	for (int i = 0; i < n; ++i) { mx += out[2*i]; my += out[2*i+1]; }
	mx /= n; my /= n;

	float cxx = 0.f, cyy = 0.f, cxy = 0.f;
	for (int i = 0; i < n; ++i)
	{
		float dx = out[2*i] - mx, dy = out[2*i+1] - my;
		cxx += dx*dx; cyy += dy*dy; cxy += dx*dy;
	}

	EXPECT_NEAR(cxy, 0.f, 1e-2f);  // principal axes aligned to x/y
	EXPECT_GT(cxx, cyy);           // largest spread along the principal (x) axis
}
