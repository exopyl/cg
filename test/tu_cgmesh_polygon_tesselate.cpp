#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

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
	// La donnee est livree avec le depot et copiee dans le repertoire de travail
	// des tests : son absence signale une invocation hors de ce repertoire, pas
	// une condition d'environnement acceptable.
	ASSERT_GT(pol.get_n_contours(), 0)
		<< "./test/data/polygon1.dat introuvable ou illisible";

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

// ---------------------------------------------------------------------------
// Sommet pose sur le cote d'un autre contour : le balayage de glutess peut
// emettre un triangle d'aire nulle dont les trois sommets, issus de contours
// differents, sont alignes. La tessellation le remplace par deux triangles
// d'aire positive, sans toucher au tableau de sommets.
// ---------------------------------------------------------------------------
namespace
{
	struct Tessellation
	{
		std::vector<float>        vertices;
		std::vector<unsigned int> faces;
	};

	Tessellation tessellateContours(const std::vector<std::vector<float>> &contours)
	{
		Polygon2 pol;
		pol.alloc_contours((int)contours.size());
		std::vector<std::vector<float>> packed = contours;
		for (size_t c = 0; c < packed.size(); ++c)
			pol.add_contour((unsigned int)c, (unsigned int)(packed[c].size() / 2), packed[c].data());
		float *pVertices = nullptr;
		unsigned int nVertices = 0, *pFaces = nullptr, nFaces = 0;
		pol.tesselate(&pVertices, &nVertices, &pFaces, &nFaces);
		Tessellation t;
		if (pVertices) t.vertices.assign(pVertices, pVertices + 3 * nVertices);
		if (pFaces)    t.faces.assign(pFaces, pFaces + 3 * nFaces);
		free_tesselation(pVertices, pFaces);
		return t;
	}

	// Deux fois l'aire signee du triangle f, en double sur les coordonnees float.
	double twiceSignedArea(const Tessellation &t, size_t f)
	{
		const float *a = &t.vertices[3 * t.faces[3 * f]];
		const float *b = &t.vertices[3 * t.faces[3 * f + 1]];
		const float *c = &t.vertices[3 * t.faces[3 * f + 2]];
		return ((double)b[0] - a[0]) * ((double)c[1] - a[1]) - ((double)b[1] - a[1]) * ((double)c[0] - a[0]);
	}

	// Chaque face a une aire positive (sens trigonometrique), au-dessus d'un
	// millionieme de l'aire de la region ; la somme des aires vaut celle de la
	// region.
	void expectPositiveFacesCovering(const Tessellation &t, double regionArea)
	{
		ASSERT_FALSE(t.faces.empty());
		double sum = 0.0;
		for (size_t f = 0; f < t.faces.size() / 3; ++f)
		{
			const double a2 = twiceSignedArea(t, f);
			EXPECT_GT(a2, 1e-6 * regionArea) << "face " << f;
			sum += 0.5 * a2;
		}
		EXPECT_NEAR(sum, regionArea, 1e-6 * regionArea);
	}

	std::vector<std::vector<float>> scaled(std::vector<std::vector<float>> contours, float s)
	{
		for (std::vector<float> &c : contours)
			for (float &x : c)
				x *= s;
		return contours;
	}
}

TEST(TEST_cgmesh_polygon_tesselate, vertex_on_a_hole_side_leaves_no_zero_area_face)
{
	const std::vector<std::vector<float>> contours = {
		{0.f, 0.f, 1.f, 0.f, 4.f, 0.f, 4.f, 4.f, 0.f, 4.f},   // (1, 0) sous le cote x = 1 du trou
		{1.f, 1.f, 1.f, 3.f, 3.f, 3.f, 3.f, 1.f}};
	const Tessellation t = tessellateContours(contours);

	// Tableau de sommets : les points d'entree, dans l'ordre, sans sommet ajoute.
	std::vector<float> expected;
	for (const std::vector<float> &c : contours)
		for (size_t i = 0; i < c.size(); i += 2)
			expected.insert(expected.end(), {c[i], c[i + 1], 0.f});
	EXPECT_EQ(t.vertices, expected);
	expectPositiveFacesCovering(t, 16.0 - 4.0);
}

// Variantes du meme alignement : deux trous de memes abscisses, chaine de
// sommets alignes sur un seul contour, jambages et appui de memes abscisses.
TEST(TEST_cgmesh_polygon_tesselate, aligned_vertices_of_several_contours_leave_no_zero_area_face)
{
	struct Case { const char *name; std::vector<std::vector<float>> contours; double area; };
	const std::vector<Case> cases = {
		{"two holes same x", {{0,0, 4,0, 4,8, 0,8}, {1,1, 1,3, 3,3, 3,1}, {1,5, 1,7, 3,7, 3,5}}, 32.0 - 8.0},
		{"vertical collinear chain", {{0,0, 4,0, 4,1, 4,2, 4,3, 4,4, 0,4, 2,2}}, 12.0},
		{"jamb and sill same x", {{-4,-8, 4,-8, 4,4, -4,4}, {-2,-6, -2,2, -1,3, 0,2, 0,-6}, {1,-6, 1,2, 2,3, 3,2, 3,-6}},
		 96.0 - 2.0 * 17.0},
	};
	for (const Case &c : cases)
	{
		SCOPED_TRACE(c.name);
		expectPositiveFacesCovering(tessellateContours(c.contours), c.area);
	}
}

// La planeite ne depend pas de l'echelle : le reproducteur et une grille de
// trous alignes, de 1e-4 a 1e4, ne laissent aucune face d'aire nulle.
TEST(TEST_cgmesh_polygon_tesselate, aligned_vertices_leave_no_zero_area_face_at_any_scale)
{
	std::vector<std::vector<float>> grid = {{0,0, 1,0, 3,0, 5,0, 7,0, 8,0, 8,8, 7,8, 5,8, 3,8, 1,8, 0,8}};
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
		{
			const float x = 1.f + 2.f * i, y = 1.f + 2.f * j;
			grid.push_back({x, y, x, y + 1, x + 1, y + 1, x + 1, y});
		}
	struct Case { const char *name; std::vector<std::vector<float>> contours; double area; };
	const std::vector<Case> cases = {
		{"reproducer", {{0,0, 1,0, 4,0, 4,4, 0,4}, {1,1, 1,3, 3,3, 3,1}}, 12.0},
		{"3x3 grid of holes", grid, 64.0 - 9.0},
	};
	for (const Case &c : cases)
		for (const float s : {1e-4f, 1e-2f, 1e2f, 1e4f})
		{
			SCOPED_TRACE(std::string(c.name) + ", echelle " + std::to_string(s));
			expectPositiveFacesCovering(tessellateContours(scaled(c.contours, s)), c.area * (double)s * s);
		}
}

// ---------------------------------------------------------------------------
// flipFlatTriangles, appele sur des triangulations ecrites a la main
// ---------------------------------------------------------------------------
namespace
{
	struct Quad { const char *name; std::vector<float> v; };

	// Deux triangles : A-B-C (indices 0, 1, 2) et A-C-D (0, 2, 3).
	std::vector<unsigned int> quadFaces() { return {0, 1, 2, 0, 2, 3}; }
}

// Une bascule qui retournerait ou aplatirait un triangle est refusee : les
// faces restent celles de l'entree.
TEST(TEST_cgmesh_polygon_tesselate, flip_never_reverses_or_flattens_a_triangle)
{
	const std::vector<Quad> cases = {
		// B a peine sous AC, D loin dans le prolongement : B-C-D serait retourne.
		{"quadrilatere de la revue", {0,0,0, 0.99999f,-1e-7f,0, 1,0,0, 1000,1,0}},
		// A-B-C plat ; D presque aligne au-dela de C : B-C-D serait retourne.
		{"D presque aligne au-dela de C", {0,0,0, 0.99999f,-1e-14f,0, 1,0,0, 2,5e-10f,0}},
		// B a 0,9e-12 sous AC (A-B-C plat), D a peine au-dessus du prolongement :
		// B-C-D serait retourne sans etre plat (double aire -0,45e-12 pour un
		// seuil de 0,25e-12).
		{"B-C-D retourne non plat", {0,0,0, 1.f - 1e-6f,-0.9e-12f,0, 1,0,0, 1.5f,3e-12f,0}},
		// D aligne avec A et C : A-B-D et B-C-D seraient plats.
		{"D aligne avec A et C", {0,0,0, 0.5f,0,0, 1,0,0, 3,0,0}},
		// D a 1e-13 du prolongement de AC : A-C-D, A-B-D et B-C-D seraient plats
		// sans etre nuls.
		{"D presque aligne avec A et C", {0,0,0, 0.5f,0,0, 1,0,0, 2,1e-13f,0}},
		// B a la position de C : B-C-D serait plat.
		{"B confondu avec C", {0,0,0, 1,0,0, 1,0,0, 0.5f,1,0}},
	};
	for (const Quad &q : cases)
	{
		SCOPED_TRACE(q.name);
		std::vector<unsigned int> f = quadFaces();
		EXPECT_EQ(flipFlatTriangles(q.v.data(), f.data(), 2), 0u);
		EXPECT_EQ(f, quadFaces());
	}
}

// Plat se lit par rapport au plus long cote : une double aire de 1e-14 sur un
// cote de 1 est plate (bascule), comme la meme figure reduite a 1e-4 ; une
// double aire de 1e-8 sur un cote de 1 est un triangle mince, laisse tel quel.
TEST(TEST_cgmesh_polygon_tesselate, flatness_is_relative_to_the_longest_side)
{
	{
		SCOPED_TRACE("plat, cote 1");
		const std::vector<float> v = {0,0,0, 0.5f,-1e-14f,0, 1,0,0, 0.5f,1,0};
		std::vector<unsigned int> f = quadFaces();
		EXPECT_EQ(flipFlatTriangles(v.data(), f.data(), 2), 1u);
		EXPECT_EQ(f, (std::vector<unsigned int>{0, 1, 3, 1, 2, 3}));
	}
	{
		SCOPED_TRACE("plat, cote 1e-4");
		const std::vector<float> v = {0,0,0, 5e-5f,-1e-18f,0, 1e-4f,0,0, 5e-5f,1e-4f,0};
		std::vector<unsigned int> f = quadFaces();
		EXPECT_EQ(flipFlatTriangles(v.data(), f.data(), 2), 1u);
		EXPECT_EQ(f, (std::vector<unsigned int>{0, 1, 3, 1, 2, 3}));
	}
	{
		SCOPED_TRACE("mince, cote 1");
		const std::vector<float> v = {0,0,0, 0.5f,-1e-8f,0, 1,0,0, 0.5f,1,0};
		std::vector<unsigned int> f = quadFaces();
		EXPECT_EQ(flipFlatTriangles(v.data(), f.data(), 2), 0u);
		EXPECT_EQ(f, quadFaces());
	}
}

// ---------------------------------------------------------------------------
// Regle d'enroulement, bord de region, bascules de Delaunay
// ---------------------------------------------------------------------------
namespace
{
	using Contours = std::vector<std::vector<float>>;

	Polygon2 polygonOf(const Contours &contours)
	{
		Polygon2 pol;
		pol.alloc_contours((int)contours.size());
		Contours packed = contours;
		for (size_t c = 0; c < packed.size(); ++c)
			pol.add_contour((unsigned int)c, (unsigned int)(packed[c].size() / 2), packed[c].data());
		return pol;
	}

	Tessellation tessellateWith(const Contours &contours, WindingRule rule)
	{
		const Polygon2 pol = polygonOf(contours);
		float *pVertices = nullptr;
		unsigned int nVertices = 0, *pFaces = nullptr, nFaces = 0;
		pol.tesselate(&pVertices, &nVertices, &pFaces, &nFaces, rule);
		Tessellation t;
		if (pVertices) t.vertices.assign(pVertices, pVertices + 3 * nVertices);
		if (pFaces)    t.faces.assign(pFaces, pFaces + 3 * nFaces);
		free_tesselation(pVertices, pFaces);
		return t;
	}

	double signedAreaOf(const Tessellation &t)
	{
		double sum = 0.0;
		for (size_t f = 0; f < t.faces.size() / 3; ++f)
			sum += 0.5 * twiceSignedArea(t, f);
		return sum;
	}

	std::vector<float> square(float x0, float y0, float x1, float y1, bool direct)
	{
		if (direct)
			return {x0, y0, x1, y0, x1, y1, x0, y1};
		return {x0, y0, x0, y1, x1, y1, x1, y0};
	}

	using Point = std::pair<float, float>;

	std::set<Point> pointsOf(const Contours &contours)
	{
		std::set<Point> s;
		for (const std::vector<float> &c : contours)
			for (size_t i = 0; i < c.size(); i += 2)
				s.insert({c[i], c[i + 1]});
		return s;
	}

	// Positions des sommets portes par au moins une face.
	std::set<Point> usedPositions(const Tessellation &t)
	{
		std::set<Point> s;
		for (unsigned int i : t.faces)
			s.insert({t.vertices[3 * i], t.vertices[3 * i + 1]});
		return s;
	}

	double ringArea(const Polygon2 &pol, int c)
	{
		const float *p = pol.get_points((unsigned int)c);
		const int n = pol.get_n_points((unsigned int)c);
		double sum = 0.0;
		for (int i = 0; i < n; ++i)
		{
			const int j = (i + 1) % n;
			sum += (double)p[2 * i] * p[2 * j + 1] - (double)p[2 * j] * p[2 * i + 1];
		}
		return 0.5 * sum;
	}

	std::set<Point> ringPoints(const Polygon2 &pol, int c)
	{
		std::set<Point> s;
		const float *p = pol.get_points((unsigned int)c);
		for (int i = 0; i < pol.get_n_points((unsigned int)c); ++i)
			s.insert({p[2 * i], p[2 * i + 1]});
		return s;
	}

	// Aretes orientees portees par un seul triangle, sans jumelle de sens oppose.
	std::multiset<std::pair<unsigned int, unsigned int>> boundaryEdges(const std::vector<unsigned int> &f)
	{
		std::multiset<std::pair<unsigned int, unsigned int>> directed, out;
		for (size_t t = 0; t < f.size() / 3; ++t)
			for (int e = 0; e < 3; ++e)
				directed.insert({f[3 * t + e], f[3 * t + (e + 1) % 3]});
		for (const auto &e : directed)
			if (directed.count({e.second, e.first}) == 0)
				out.insert(e);
		return out;
	}

	// Detecteur d'arete illegale, independant de delaunayFlips : une arete a-b
	// portee par a-b-c et b-a-d est illegale si d est dans le cercle
	// circonscrit de a-b-c, au-dela de 1e-6 fois le carre de la somme des carres
	// des distances a d (seuil mille fois plus large que celui des bascules).
	int illegalEdges(const std::vector<float> &v, const std::vector<unsigned int> &f)
	{
		std::map<std::pair<unsigned int, unsigned int>, std::pair<size_t, int>> edge;
		for (size_t t = 0; t < f.size() / 3; ++t)
			for (int e = 0; e < 3; ++e)
				edge[{f[3 * t + e], f[3 * t + (e + 1) % 3]}] = {t, e};
		auto X = [&](unsigned int i) { return (double)v[3 * i]; };
		auto Y = [&](unsigned int i) { return (double)v[3 * i + 1]; };
		int illegal = 0;
		for (const auto &kv : edge)
		{
			const unsigned int a = kv.first.first, b = kv.first.second;
			if (a > b) continue;
			const auto twin = edge.find({b, a});
			if (twin == edge.end()) continue;
			const unsigned int c = f[3 * kv.second.first + (kv.second.second + 2) % 3];
			const unsigned int d = f[3 * twin->second.first + (twin->second.second + 2) % 3];
			const double orient = (X(b) - X(a)) * (Y(c) - Y(a)) - (Y(b) - Y(a)) * (X(c) - X(a));
			const double adx = X(a) - X(d), ady = Y(a) - Y(d), bdx = X(b) - X(d), bdy = Y(b) - Y(d);
			const double cdx = X(c) - X(d), cdy = Y(c) - Y(d);
			const double ad = adx * adx + ady * ady, bd = bdx * bdx + bdy * bdy, cd = cdx * cdx + cdy * cdy;
			const double det = ad * (bdx * cdy - cdx * bdy) - bd * (adx * cdy - cdx * ady) + cd * (adx * bdy - bdx * ady);
			const double scale = ad + bd + cd;
			if ((orient > 0.0 ? det : -det) > 1e-6 * scale * scale)
				++illegal;
		}
		return illegal;
	}

	// Chaque face a une double aire positive au-dessus de 1e-10 fois le carre de
	// son plus long cote (ni plate ni retournee).
	void expectProperPositiveFaces(const std::vector<float> &v, const std::vector<unsigned int> &f)
	{
		for (size_t t = 0; t < f.size() / 3; ++t)
		{
			const float *a = &v[3 * f[3 * t]], *b = &v[3 * f[3 * t + 1]], *c = &v[3 * f[3 * t + 2]];
			const double a2 = ((double)b[0] - a[0]) * ((double)c[1] - a[1]) - ((double)b[1] - a[1]) * ((double)c[0] - a[0]);
			auto sq = [](const float *p, const float *q) { const double dx = (double)q[0] - p[0], dy = (double)q[1] - p[1]; return dx * dx + dy * dy; };
			const double l2 = (std::max)(sq(a, b), (std::max)(sq(b, c), sq(c, a)));
			EXPECT_GT(a2, 1e-10 * l2) << "face " << t;
		}
	}

	std::vector<float> circle(float r, int n, bool direct)
	{
		std::vector<float> c;
		for (int k = 0; k < n; ++k)
		{
			const double th = 2.0 * 3.14159265358979323846 * (direct ? k : n - k) / n;
			c.push_back((float)(r * std::cos(th)));
			c.push_back((float)(r * std::sin(th)));
		}
		return c;
	}
}

// La regle par defaut est NonZero, a l'octet : meme tableau de sommets, memes
// faces, sur des polygones a trou, a contours qui se recouvrent et sur la donnee
// du depot.
TEST(TEST_cgmesh_polygon_tesselate, default_rule_is_nonzero_byte_for_byte)
{
	std::vector<Polygon2> polygons = {
		polygonOf({square(0, 0, 4, 4, true), square(1, 1, 3, 3, false)}),
		polygonOf({square(0, 0, 2, 2, true), square(1, 1, 3, 3, true), square(1.5f, -1, 2.5f, 5, false)}),
	};
	Polygon2 fromFile;
	fromFile.input((char *)"./test/data/polygon1.dat");
	ASSERT_GT(fromFile.get_n_contours(), 0) << "./test/data/polygon1.dat introuvable ou illisible";
	polygons.push_back(fromFile);
	for (size_t i = 0; i < polygons.size(); ++i)
	{
		SCOPED_TRACE("polygone " + std::to_string(i));
		float *v0 = nullptr, *v1 = nullptr;
		unsigned int n0 = 0, n1 = 0, *f0 = nullptr, *f1 = nullptr, m0 = 0, m1 = 0;
		polygons[i].tesselate(&v0, &n0, &f0, &m0);
		polygons[i].tesselate(&v1, &n1, &f1, &m1, WindingRule::NonZero);
		ASSERT_EQ(n0, n1);
		ASSERT_EQ(m0, m1);
		EXPECT_EQ(std::vector<float>(v0, v0 + 3 * n0), std::vector<float>(v1, v1 + 3 * n1));
		EXPECT_EQ(std::vector<unsigned int>(f0, f0 + 3 * m0), std::vector<unsigned int>(f1, f1 + 3 * m1));
		free_tesselation(v0, f0);
		free_tesselation(v1, f1);
	}
}

// Positive : un carre direct A = [0, 2]^2 moins un carre B donne a l'envers.
// B interieur a sommet commun : A - B, aire 3 ; B exterieur a sommet commun :
// A seul, aire 4 (NonZero ajoute B : 8) ; B recouvrant : A - B, aire 3
// (NonZero : 6). Aires exactes, faces non plates et directes, aucun sommet a
// une position nouvelle sauf les deux croisements du cas recouvrant.
TEST(TEST_cgmesh_polygon_tesselate, positive_rule_subtracts_reversed_contours_without_their_overflow)
{
	struct Case { const char *name; Contours contours; double positive, nonZero; std::set<Point> created; };
	const std::vector<Case> cases = {
		{"B interieur, sommet commun", {square(0, 0, 2, 2, true), square(1, 1, 2, 2, false)}, 3.0, 3.0, {}},
		{"B exterieur, sommet commun", {square(0, 0, 2, 2, true), square(2, 2, 4, 4, false)}, 4.0, 8.0, {}},
		{"B recouvrant", {square(0, 0, 2, 2, true), square(1, 1, 3, 3, false)}, 3.0, 6.0, {{2.f, 1.f}, {1.f, 2.f}}},
	};
	for (const Case &c : cases)
	{
		SCOPED_TRACE(c.name);
		const Tessellation positive = tessellateWith(c.contours, WindingRule::Positive);
		ASSERT_FALSE(positive.faces.empty());
		EXPECT_NEAR(signedAreaOf(positive), c.positive, 1e-9);
		EXPECT_NEAR(signedAreaOf(tessellateWith(c.contours, WindingRule::NonZero)), c.nonZero, 1e-9);
		expectProperPositiveFaces(positive.vertices, positive.faces);

		std::set<Point> allowed = pointsOf(c.contours);
		allowed.insert(c.created.begin(), c.created.end());
		for (const Point &p : usedPositions(positive))
			EXPECT_TRUE(allowed.count(p)) << "sommet cree en (" << p.first << ", " << p.second << ")";

		// Chaque face est dans A et hors de l'interieur de B.
		for (size_t f = 0; f < positive.faces.size() / 3; ++f)
		{
			double cx = 0.0, cy = 0.0;
			for (int k = 0; k < 3; ++k)
			{
				cx += positive.vertices[3 * positive.faces[3 * f + k]] / 3.0;
				cy += positive.vertices[3 * positive.faces[3 * f + k] + 1] / 3.0;
			}
			const std::vector<float> &b = c.contours[1];
			const bool inB = cx > b[0] && cx < b[4] && cy > b[1] && cy < b[5];
			EXPECT_TRUE(cx > 0.0 && cx < 2.0 && cy > 0.0 && cy < 2.0 && !inB) << "face " << f;
		}
	}
}

// Bord de la reunion de deux carres directs qui se recouvrent : une seule
// boucle, en sens trigonometrique, d'aire 7, par les six coins exterieurs et
// les deux croisements, sans les deux coins interieurs. Bord d'un carre troue
// par un carre inverse : deux boucles, l'enveloppe directe, le trou horaire.
TEST(TEST_cgmesh_polygon_tesselate, boundary_only_gives_the_oriented_loops_of_the_region)
{
	for (const WindingRule rule : {WindingRule::Positive, WindingRule::NonZero})
	{
		SCOPED_TRACE(rule == WindingRule::Positive ? "Positive" : "NonZero");
		Polygon2 loops;
		EXPECT_EQ(polygonOf({square(0, 0, 2, 2, true), square(1, 1, 3, 3, true)}).boundary(rule, loops), 1);
		ASSERT_EQ(loops.get_n_contours(), 1);
		EXPECT_EQ(loops.get_n_points(0), 8);
		EXPECT_NEAR(ringArea(loops, 0), 7.0, 1e-9);
		const std::set<Point> expected = {{0, 0}, {2, 0}, {2, 1}, {3, 1}, {3, 3}, {1, 3}, {1, 2}, {0, 2}};
		EXPECT_EQ(ringPoints(loops, 0), expected);
	}
	{
		SCOPED_TRACE("trou");
		Polygon2 loops;
		EXPECT_EQ(polygonOf({square(0, 0, 4, 4, true), square(1, 1, 3, 3, false)}).boundary(WindingRule::Positive, loops), 2);
		ASSERT_EQ(loops.get_n_contours(), 2);
		const int outer = ringArea(loops, 0) > 0.0 ? 0 : 1;
		EXPECT_NEAR(ringArea(loops, outer), 16.0, 1e-9);
		EXPECT_NEAR(ringArea(loops, 1 - outer), -4.0, 1e-9);
		EXPECT_EQ(ringPoints(loops, outer), pointsOf({square(0, 0, 4, 4, true)}));
		EXPECT_EQ(ringPoints(loops, 1 - outer), pointsOf({square(1, 1, 3, 3, true)}));
	}
	{
		SCOPED_TRACE("carre inverse seul, Positive : region vide");
		Polygon2 loops;
		EXPECT_EQ(polygonOf({square(0, 0, 1, 1, false)}).boundary(WindingRule::Positive, loops), 0);
		EXPECT_EQ(loops.get_n_contours(), 0);
	}
}

// Cerf-volant 0 (0, 0), 1 (3, -0,2), 2 (6, 0), 3 (3, 0,2) coupe par sa longue
// diagonale 0-2 : le detecteur la signale (controle positif) ; une bascule la
// remplace par 1-3, les deux triangles restent directs, l'aire et les aretes de
// bord ne changent pas. Coupe par sa courte diagonale, ou carre cocirculaire :
// aucune bascule.
TEST(TEST_cgmesh_polygon_tesselate, delaunay_flip_replaces_the_long_diagonal_of_a_kite)
{
	const std::vector<float> kite = {0,0,0, 3,-0.2f,0, 6,0,0, 3,0.2f,0};
	{
		SCOPED_TRACE("longue diagonale");
		std::vector<unsigned int> f = {0, 1, 2, 0, 2, 3};
		ASSERT_EQ(illegalEdges(kite, f), 1);
		const auto border = boundaryEdges(f);
		EXPECT_EQ(delaunayFlips(kite.data(), f.data(), 2), 1u);
		EXPECT_EQ(illegalEdges(kite, f), 0);
		EXPECT_EQ(boundaryEdges(f), border);
		expectProperPositiveFaces(kite, f);
		Tessellation t{kite, f};
		EXPECT_NEAR(signedAreaOf(t), 1.2, 1e-6);
		const std::set<std::pair<unsigned int, unsigned int>> diagonal = {{1, 3}, {3, 1}};
		int onDiagonal = 0;
		for (size_t k = 0; k < 2; ++k)
			for (int e = 0; e < 3; ++e)
				onDiagonal += (int)diagonal.count({f[3 * k + e], f[3 * k + (e + 1) % 3]});
		EXPECT_EQ(onDiagonal, 2);
	}
	{
		SCOPED_TRACE("courte diagonale");
		std::vector<unsigned int> f = {0, 1, 3, 1, 2, 3};
		EXPECT_EQ(illegalEdges(kite, f), 0);
		EXPECT_EQ(delaunayFlips(kite.data(), f.data(), 2), 0u);
		EXPECT_EQ(f, (std::vector<unsigned int>{0, 1, 3, 1, 2, 3}));
	}
	{
		SCOPED_TRACE("carre cocirculaire");
		const std::vector<float> sq = {0,0,0, 1,0,0, 1,1,0, 0,1,0};
		std::vector<unsigned int> f = {0, 1, 2, 0, 2, 3};
		EXPECT_EQ(delaunayFlips(sq.data(), f.data(), 2), 0u);
		EXPECT_EQ(f, (std::vector<unsigned int>{0, 1, 2, 0, 2, 3}));
	}
}

// Bandes minces tessellees par glutess (anneau de 48 / 40 segments, bande
// droite de 31 sommets en bas et 2 en haut) : la tessellation brute a des aretes illegales
// (controle positif, sans quoi le cas ne prouverait rien) ; apres
// delaunayFlips (tesselate a deja applique flipFlatTriangles), aucune, la
// passe est complete, avec les memes aretes de bord, le meme nombre de faces,
// la meme aire, et aucune face plate ou retournee.
TEST(TEST_cgmesh_polygon_tesselate, delaunay_flips_on_a_thin_band_keep_its_boundary_and_area)
{
	std::vector<float> strip;
	for (int i = 0; i <= 30; ++i) { strip.push_back((float)i); strip.push_back(0.f); }
	strip.insert(strip.end(), {30.f, 0.4f, 0.f, 0.4f});
	struct Case { const char *name; Contours contours; };
	const std::vector<Case> cases = {
		{"anneau", {circle(10.f, 48, true), circle(9.5f, 40, false)}},
		{"bande droite", {strip}},
	};
	for (const Case &c : cases)
	{
		SCOPED_TRACE(c.name);
		const Tessellation raw = tessellateWith(c.contours, WindingRule::Positive);
		ASSERT_GT(illegalEdges(raw.vertices, raw.faces), 0);
		std::vector<unsigned int> f = raw.faces;
		const unsigned int nf = (unsigned int)(f.size() / 3);
		bool complete = false;
		EXPECT_GT(delaunayFlips(raw.vertices.data(), f.data(), nf, &complete), 0u);
		EXPECT_TRUE(complete);
		EXPECT_EQ(illegalEdges(raw.vertices, f), 0);
		EXPECT_EQ(boundaryEdges(f), boundaryEdges(raw.faces));
		EXPECT_EQ(f.size(), raw.faces.size());
		const double before = signedAreaOf(raw);
		EXPECT_NEAR(signedAreaOf(Tessellation{raw.vertices, f}), before, 1e-9 * std::fabs(before));
		expectProperPositiveFaces(raw.vertices, f);
	}
}

// Anneau mince dense (rayons 10 et 9,5, 12 000 et 9 000 sommets) : Lawson y
// demande un nombre quadratique de bascules, la borne de cout est atteinte.
// La passe le dit (complete faux) ; la triangulation reste valide : memes
// aretes de bord, meme aire, aucune face plate ou retournee.
TEST(TEST_cgmesh_polygon_tesselate, delaunay_flips_report_when_the_cost_bound_is_reached)
{
	const Tessellation raw = tessellateWith({circle(10.f, 12000, true), circle(9.5f, 9000, false)}, WindingRule::Positive);
	std::vector<unsigned int> f = raw.faces;
	bool complete = true;
	EXPECT_GT(delaunayFlips(raw.vertices.data(), f.data(), (unsigned int)(f.size() / 3), &complete), 0u);
	EXPECT_FALSE(complete);
	EXPECT_EQ(boundaryEdges(f), boundaryEdges(raw.faces));
	const double before = signedAreaOf(raw);
	EXPECT_NEAR(signedAreaOf(Tessellation{raw.vertices, f}), before, 1e-9 * std::fabs(before));
	expectProperPositiveFaces(raw.vertices, f);
}

// Grille rectangulaire non dyadique (x = 0,37 + 0,1 i, y = 0,37 + 0,13 j,
// 20 x 20 cellules, chacune coupee par sa diagonale montante) : les quatre
// sommets d'une cellule sont cocirculaires, aucune diagonale n'est illegale.
// Le seuil relatif du test du cercle empeche que l'arrondi en decide : aucune
// bascule, faces inchangees.
TEST(TEST_cgmesh_polygon_tesselate, delaunay_flips_leave_cocircular_grid_cells_alone)
{
	const int n = 21;
	std::vector<float> v;
	for (int j = 0; j < n; ++j)
		for (int i = 0; i < n; ++i)
			v.insert(v.end(), {(float)(0.37 + 0.1 * i), (float)(0.37 + 0.13 * j), 0.f});
	std::vector<unsigned int> f;
	for (int j = 0; j + 1 < n; ++j)
		for (int i = 0; i + 1 < n; ++i)
		{
			const unsigned int a = j * n + i, b = a + 1, c = a + n + 1, d = a + n;
			f.insert(f.end(), {a, b, c, a, c, d});
		}
	const std::vector<unsigned int> before = f;
	bool complete = false;
	EXPECT_EQ(delaunayFlips(v.data(), f.data(), (unsigned int)(f.size() / 3), &complete), 0u);
	EXPECT_TRUE(complete);
	EXPECT_EQ(f, before);
}

// Paire repliee : 0-1-2 direct et 1-0-3 retourne (3 du meme cote de 0-1 que
// 2). 3 est dans le cercle de 0-1-2, mais une bascule rendrait un triangle de
// sens contraire a l'autre : refusee, faces inchangees.
TEST(TEST_cgmesh_polygon_tesselate, delaunay_flips_never_flip_a_folded_pair)
{
	const std::vector<float> v = {0,0,0, 4,0,0, 2,3,0, 2,1,0};
	std::vector<unsigned int> f = {0, 1, 2, 1, 0, 3};
	EXPECT_EQ(delaunayFlips(v.data(), f.data(), 2), 0u);
	EXPECT_EQ(f, (std::vector<unsigned int>{0, 1, 2, 1, 0, 3}));
}

// Bord de deux carres directs [0, 2]^2 et [1, 3]^2 sous chaque regle : Odd,
// les deux L (aire 6, deux boucles) ; NonZero et Positive, la reunion (7) ;
// Negative, rien ; AbsGeqTwo, l'intersection (1).
TEST(TEST_cgmesh_polygon_tesselate, boundary_follows_each_winding_rule)
{
	const Polygon2 two = polygonOf({square(0, 0, 2, 2, true), square(1, 1, 3, 3, true)});
	struct Case { const char *name; WindingRule rule; int loops; double area; };
	const std::vector<Case> cases = {
		{"Odd", WindingRule::Odd, 2, 6.0},
		{"NonZero", WindingRule::NonZero, 1, 7.0},
		{"Positive", WindingRule::Positive, 1, 7.0},
		{"Negative", WindingRule::Negative, 0, 0.0},
		{"AbsGeqTwo", WindingRule::AbsGeqTwo, 1, 1.0},
	};
	for (const Case &c : cases)
	{
		SCOPED_TRACE(c.name);
		Polygon2 loops;
		EXPECT_EQ(two.boundary(c.rule, loops), c.loops);
		ASSERT_EQ(loops.get_n_contours(), c.loops);
		double area = 0.0;
		for (int k = 0; k < c.loops; ++k)
			area += ringArea(loops, k);
		EXPECT_NEAR(area, c.area, 1e-9);
	}
}
