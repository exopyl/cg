#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

// ---------------------------------------------------------------------------
// Helper: build a regular tetrahedron
// vertices: (1,1,1), (1,-1,-1), (-1,1,-1), (-1,-1,1)
// ---------------------------------------------------------------------------
static float tetra_pts[] = {
	1.f, 1.f, 1.f,
	1.f, -1.f, -1.f,
	-1.f, 1.f, -1.f,
	-1.f, -1.f, 1.f
};

// ---------------------------------------------------------------------------
// Helper: build a unit cube (8 vertices)
// ---------------------------------------------------------------------------
static float cube_pts[] = {
	0.f, 0.f, 0.f,
	1.f, 0.f, 0.f,
	1.f, 1.f, 0.f,
	0.f, 1.f, 0.f,
	0.f, 0.f, 1.f,
	1.f, 0.f, 1.f,
	1.f, 1.f, 1.f,
	0.f, 1.f, 1.f
};

// ---------------------------------------------------------------------------
// Helper: free convex hull output
// ---------------------------------------------------------------------------
static void free_hull(float *vertices, int *faces)
{
	if (vertices) free(vertices);
	if (faces) free(faces);
}

// ===========================================================================
// Tetrahedron tests (minimal convex hull: 4 vertices, 4 faces)
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: Tetrahedron -- compute does not crash
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_chull, Tetrahedron_Compute)
{
	// context
	Chull3D ch(tetra_pts, 4);

	// action
	ch.compute();

	// expectations
	EXPECT_EQ(ch.get_n_vertices(), 4);
	EXPECT_EQ(ch.get_n_faces(), 4);
}

// ---------------------------------------------------------------------------
// TEST: Tetrahedron -- get_convex_hull returns valid data
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_chull, Tetrahedron_GetConvexHull)
{
	// context
	Chull3D ch(tetra_pts, 4);
	ch.compute();
	float *vertices = nullptr;
	int nVertices = 0;
	int *faces = nullptr;
	int nFaces = 0;

	// action
	int ret = ch.get_convex_hull(&vertices, &nVertices, &faces, &nFaces);

	// expectations
	EXPECT_EQ(ret, 0);
	EXPECT_EQ(nVertices, 4);
	EXPECT_EQ(nFaces, 4);
	EXPECT_NE(vertices, nullptr);
	EXPECT_NE(faces, nullptr);

	// all face indices should be in [0, nVertices)
	for (int i = 0; i < nFaces * 3; i++)
		EXPECT_GE(faces[i], 0);
	for (int i = 0; i < nFaces * 3; i++)
		EXPECT_LT(faces[i], nVertices);

	free_hull(vertices, faces);
}

// ---------------------------------------------------------------------------
// TEST: Tetrahedron -- get_convex_hull_indices
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_chull, Tetrahedron_GetConvexHullIndices)
{
	// context
	Chull3D ch(tetra_pts, 4);
	ch.compute();
	int *indices = nullptr;
	int nIndices = 0;

	// action
	int ret = ch.get_convex_hull_indices(&indices, &nIndices);

	// expectations -- all 4 input vertices are on the hull
	EXPECT_EQ(ret, 0);
	EXPECT_EQ(nIndices, 4);
	EXPECT_NE(indices, nullptr);

	// each index should be in [0, 4)
	for (int i = 0; i < nIndices; i++)
	{
		EXPECT_GE(indices[i], 0);
		EXPECT_LT(indices[i], 4);
	}

	free(indices);
}

// ===========================================================================
// Cube tests (8 vertices, 12 triangular faces)
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: Cube -- vertex and face counts
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_chull, Cube_Counts)
{
	// context
	Chull3D ch(cube_pts, 8);

	// action
	ch.compute();

	// expectations -- cube hull: 8 vertices, 12 triangular faces (6 quads * 2)
	EXPECT_EQ(ch.get_n_vertices(), 8);
	EXPECT_EQ(ch.get_n_faces(), 12);
}

// ---------------------------------------------------------------------------
// TEST: Cube -- all input vertices are on the hull
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_chull, Cube_AllVerticesOnHull)
{
	// context
	Chull3D ch(cube_pts, 8);
	ch.compute();
	int *indices = nullptr;
	int nIndices = 0;

	// action
	ch.get_convex_hull_indices(&indices, &nIndices);

	// expectations
	EXPECT_EQ(nIndices, 8);

	free(indices);
}

// ---------------------------------------------------------------------------
// TEST: Cube -- face indices are valid
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_chull, Cube_ValidFaceIndices)
{
	// context
	Chull3D ch(cube_pts, 8);
	ch.compute();
	float *vertices = nullptr;
	int nVertices = 0;
	int *faces = nullptr;
	int nFaces = 0;

	// action
	ch.get_convex_hull(&vertices, &nVertices, &faces, &nFaces);

	// expectations
	for (int i = 0; i < nFaces * 3; i++)
	{
		EXPECT_GE(faces[i], 0) << "face index " << i;
		EXPECT_LT(faces[i], nVertices) << "face index " << i;
	}

	free_hull(vertices, faces);
}

// ===========================================================================
// Interior points test
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: Cube with interior points -- hull should still be 8 vertices
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_chull, CubeWithInteriorPoints_HullUnchanged)
{
	// context -- cube + 4 interior points
	float pts[] = {
		0.f, 0.f, 0.f,
		1.f, 0.f, 0.f,
		1.f, 1.f, 0.f,
		0.f, 1.f, 0.f,
		0.f, 0.f, 1.f,
		1.f, 0.f, 1.f,
		1.f, 1.f, 1.f,
		0.f, 1.f, 1.f,
		// interior points
		0.5f, 0.5f, 0.5f,
		0.3f, 0.3f, 0.3f,
		0.7f, 0.2f, 0.8f,
		0.1f, 0.9f, 0.4f
	};

	Chull3D ch(pts, 12);

	// action
	ch.compute();

	// expectations -- hull is still the cube
	EXPECT_EQ(ch.get_n_vertices(), 8);
	EXPECT_EQ(ch.get_n_faces(), 12);
}

// ===========================================================================
// Coplanar points test
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: Cube with coplanar points on faces -- more hull vertices
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_chull, CubeWithCoplanarPoints)
{
	// context -- cube + 1 extra point on a face edge midpoint
	// The point (0.5, 0, 0) lies on the face z=0 edge, it's coplanar
	float pts[] = {
		0.f, 0.f, 0.f,
		1.f, 0.f, 0.f,
		1.f, 1.f, 0.f,
		0.f, 1.f, 0.f,
		0.f, 0.f, 1.f,
		1.f, 0.f, 1.f,
		1.f, 1.f, 1.f,
		0.f, 1.f, 1.f,
		0.5f, 0.f, 0.f  // coplanar on bottom face edge
	};

	Chull3D ch(pts, 9);

	// action
	ch.compute();

	// expectations -- the hull should still be valid
	EXPECT_GE(ch.get_n_vertices(), 8);
	EXPECT_GE(ch.get_n_faces(), 12);
}

// ===========================================================================
// Sphere-like point cloud
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: Random points on a sphere -- hull is convex
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_chull, SpherePoints_ConvexHull)
{
	// context -- 26 points on a unit sphere (icosahedron-like distribution)
	const int n = 26;
	float pts[3 * n];
	int idx = 0;
	// generate points on sphere using latitude/longitude
	for (int lat = -2; lat <= 2; lat++)
	{
		float phi = (float)lat * 3.14159f / 5.f;
		int nLon = (lat == -2 || lat == 2) ? 1 : 5;
		for (int lon = 0; lon < nLon; lon++)
		{
			float theta = 2.f * 3.14159f * lon / nLon;
			pts[3 * idx]     = cosf(phi) * cosf(theta);
			pts[3 * idx + 1] = cosf(phi) * sinf(theta);
			pts[3 * idx + 2] = sinf(phi);
			idx++;
		}
	}
	int actualN = idx;

	Chull3D ch(pts, actualN);

	// action
	ch.compute();

	// expectations
	// all points are on the sphere surface, so all should be on the hull
	EXPECT_EQ(ch.get_n_vertices(), actualN);
	// Euler formula for convex hull: F = 2*V - 4
	EXPECT_EQ(ch.get_n_faces(), 2 * actualN - 4);
}

// ===========================================================================
// No degenerate faces
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: Cube -- all triangle faces have distinct vertices
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_chull, Cube_NoDegenerateFaces)
{
	// context
	Chull3D ch(cube_pts, 8);
	ch.compute();
	float *vertices = nullptr;
	int nVertices = 0;
	int *faces = nullptr;
	int nFaces = 0;
	ch.get_convex_hull(&vertices, &nVertices, &faces, &nFaces);

	// action & expectations
	for (int i = 0; i < nFaces; i++)
	{
		int i0 = faces[3 * i];
		int i1 = faces[3 * i + 1];
		int i2 = faces[3 * i + 2];
		EXPECT_NE(i0, i1) << "face " << i << " degenerate";
		EXPECT_NE(i1, i2) << "face " << i << " degenerate";
		EXPECT_NE(i0, i2) << "face " << i << " degenerate";
	}

	free_hull(vertices, faces);
}

// ===========================================================================
// Hull vertices are a subset of input
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: Cube with interior -- hull vertices match input coordinates
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_chull, CubeWithInterior_HullVerticesMatchInput)
{
	// context
	float pts[] = {
		0.f, 0.f, 0.f,
		1.f, 0.f, 0.f,
		1.f, 1.f, 0.f,
		0.f, 1.f, 0.f,
		0.f, 0.f, 1.f,
		1.f, 0.f, 1.f,
		1.f, 1.f, 1.f,
		0.f, 1.f, 1.f,
		0.5f, 0.5f, 0.5f  // interior
	};
	Chull3D ch(pts, 9);
	ch.compute();
	float *hullVerts = nullptr;
	int nHullVerts = 0;
	int *faces = nullptr;
	int nFaces = 0;
	ch.get_convex_hull(&hullVerts, &nHullVerts, &faces, &nFaces);

	// action & expectations
	// each hull vertex should match one of the 8 cube corners
	for (int h = 0; h < nHullVerts; h++)
	{
		float hx = hullVerts[3 * h];
		float hy = hullVerts[3 * h + 1];
		float hz = hullVerts[3 * h + 2];
		bool found = false;
		for (int i = 0; i < 8; i++)
		{
			if (fabsf(hx - pts[3 * i]) < 1e-5f &&
			    fabsf(hy - pts[3 * i + 1]) < 1e-5f &&
			    fabsf(hz - pts[3 * i + 2]) < 1e-5f)
			{
				found = true;
				break;
			}
		}
		EXPECT_TRUE(found) << "hull vertex " << h << " (" << hx << "," << hy << "," << hz
		                   << ") not found among cube corners";
	}

	free_hull(hullVerts, faces);
}

// ===========================================================================
// Export OBJ
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: export_obj -- produces a valid file
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_chull, ExportObj_ProducesFile)
{
	// context
	Chull3D ch(cube_pts, 8);
	ch.compute();
	char *filename = (char *)"chull_cube.obj";

	// action
	ch.export_obj(filename);

	// expectations
	FILE *f = fopen(filename, "r");
	EXPECT_NE(f, nullptr);
	if (f)
		fclose(f);
}

// ---------------------------------------------------------------------------
// TEST: export_obj -- OBJ has correct vertex and face counts
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_chull, ExportObj_CorrectCounts)
{
	// context
	Chull3D ch(cube_pts, 8);
	ch.compute();
	char *filename = (char *)"chull_cube_counts.obj";
	ch.export_obj(filename);

	// action -- parse
	FILE *f = fopen(filename, "r");
	ASSERT_NE(f, nullptr);
	unsigned int nV = 0, nF = 0;
	char line[256];
	while (fgets(line, sizeof(line), f))
	{
		if (line[0] == 'v' && line[1] == ' ')
			nV++;
		else if (line[0] == 'f' && line[1] == ' ')
			nF++;
	}
	fclose(f);

	// expectations
	EXPECT_EQ(nV, 8u);
	EXPECT_EQ(nF, 12u);
}

// ===========================================================================
// Euler formula invariant
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: Euler formula F = 2V - 4 for convex polyhedron (triangulated)
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_chull, EulerFormula_Tetrahedron)
{
	// context
	Chull3D ch(tetra_pts, 4);
	ch.compute();

	// action
	int V = ch.get_n_vertices();
	int F = ch.get_n_faces();

	// expectations -- for a triangulated convex hull: F = 2V - 4
	EXPECT_EQ(F, 2 * V - 4);
}

TEST(TEST_cgmesh_chull, EulerFormula_Cube)
{
	// context
	Chull3D ch(cube_pts, 8);
	ch.compute();

	// action
	int V = ch.get_n_vertices();
	int F = ch.get_n_faces();

	// expectations
	EXPECT_EQ(F, 2 * V - 4);
}
