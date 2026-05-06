#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <set>
#include <vector>

#include "../src/cgmesh/cgmesh.h"
#include "../src/cgmesh/subdivision_sqrt3.h"

namespace
{

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

static Mesh_half_edge *
make_unit_cube ()
{
	const float V[8 * 3] = {
		0,0,0,  1,0,0,  1,1,0,  0,1,0,
		0,0,1,  1,0,1,  1,1,1,  0,1,1
	};
	const unsigned int F[12 * 3] = {
		0, 2, 1,   0, 3, 2,
		4, 5, 6,   4, 6, 7,
		0, 1, 5,   0, 5, 4,
		2, 3, 7,   2, 7, 6,
		0, 4, 7,   0, 7, 3,
		1, 2, 6,   1, 6, 5
	};

	Mesh_half_edge *he = new Mesh_half_edge ();
	he->m_pMesh->Init ();
	he->m_pMesh->SetVertices (8, const_cast<float*>(V));
	he->m_pMesh->SetFaces (12, 3, const_cast<unsigned int*>(F));
	he->create_half_edge ();
	(void)he->GetCheMesh ();
	return he;
}

static Mesh_half_edge *
make_tetrahedron ()
{
	const float s = 1.0f / std::sqrt(3.0f);
	const float V[4 * 3] = {
		 s,  s,  s,
		-s, -s,  s,
		-s,  s, -s,
		 s, -s, -s
	};
	const unsigned int F[4 * 3] = {
		0, 1, 2,
		0, 3, 1,
		0, 2, 3,
		1, 3, 2
	};

	Mesh_half_edge *he = new Mesh_half_edge ();
	he->m_pMesh->Init ();
	he->m_pMesh->SetVertices (4, const_cast<float*>(V));
	he->m_pMesh->SetFaces (4, 3, const_cast<unsigned int*>(F));
	he->create_half_edge ();
	(void)he->GetCheMesh ();
	return he;
}

// Single XY-plane triangle ; all 3 edges are boundary edges.
// Used to test : (a) centroid placement, (b) boundary smoothing of corners.
static Mesh_half_edge *
make_flat_triangle ()
{
	const float V[3 * 3] = {
		0.0f, 0.0f, 0.0f,
		3.0f, 0.0f, 0.0f,
		0.0f, 3.0f, 0.0f
	};
	const unsigned int F[3] = { 0, 1, 2 };

	Mesh_half_edge *he = new Mesh_half_edge ();
	he->m_pMesh->Init ();
	he->m_pMesh->SetVertices (3, const_cast<float*>(V));
	he->m_pMesh->SetFaces (1, 3, const_cast<unsigned int*>(F));
	he->create_half_edge ();
	(void)he->GetCheMesh ();
	return he;
}

static std::array<float,3>
get_vertex (Mesh *m, int i)
{
	return { m->m_pVertices[3*i], m->m_pVertices[3*i+1], m->m_pVertices[3*i+2] };
}

static int
count_unique_edges (Mesh *m)
{
	std::set<std::pair<int,int>> uniq;
	for (unsigned int f = 0; f < m->m_nFaces; ++f)
	{
		Face *F = m->m_pFaces[f];
		if (F->GetNVertices() != 3) continue;
		int a = F->GetVertex(0), b = F->GetVertex(1), c = F->GetVertex(2);
		auto add = [&](int u, int v) {
			if (u > v) std::swap (u, v);
			uniq.insert ({u, v});
		};
		add (a, b); add (b, c); add (c, a);
	}
	return (int)uniq.size();
}

// True if the mesh contains an edge between vertices a and b (any direction).
static bool
has_edge (Mesh *m, int a, int b)
{
	for (unsigned int f = 0; f < m->m_nFaces; ++f)
	{
		Face *F = m->m_pFaces[f];
		if (F->GetNVertices() != 3) continue;
		int x = F->GetVertex(0), y = F->GetVertex(1), z = F->GetVertex(2);
		if ((x == a && y == b) || (y == a && x == b)) return true;
		if ((y == a && z == b) || (z == a && y == b)) return true;
		if ((z == a && x == b) || (x == a && z == b)) return true;
	}
	return false;
}

} // anon

// ============================================================================
// Tests
// ============================================================================

TEST(TEST_cgmesh_subdivision_sqrt3, CubeCounts)
{
	// 1->3 split : nv 8 -> 8 + 12 = 20 ; nf 12 -> 36.
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	const int nv0 = (int)m->m_nVertices;
	const int nf0 = (int)m->m_nFaces;

	MeshAlgoSubdivisionSqrt3 algo;
	EXPECT_TRUE (algo.GetSmoothOriginal ());   // default on
	ASSERT_TRUE (algo.Apply (he));

	EXPECT_EQ ((int)m->m_nVertices, nv0 + nf0);
	EXPECT_EQ ((int)m->m_nFaces, 3 * nf0);

	delete he;
}

TEST(TEST_cgmesh_subdivision_sqrt3, TetrahedronCounts)
{
	Mesh_half_edge *he = make_tetrahedron ();
	Mesh *m = he->m_pMesh;

	const int nv0 = (int)m->m_nVertices;
	const int nf0 = (int)m->m_nFaces;

	MeshAlgoSubdivisionSqrt3 algo;
	ASSERT_TRUE (algo.Apply (he));

	EXPECT_EQ ((int)m->m_nVertices, nv0 + nf0);
	EXPECT_EQ ((int)m->m_nFaces, 3 * nf0);

	delete he;
}

TEST(TEST_cgmesh_subdivision_sqrt3, NoSmoothingPreservesCorners)
{
	// With m_smoothOriginal = false, original cube corners must be unchanged.
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	const float corners[8][3] = {
		{0,0,0},{1,0,0},{1,1,0},{0,1,0},
		{0,0,1},{1,0,1},{1,1,1},{0,1,1}
	};

	MeshAlgoSubdivisionSqrt3 algo;
	algo.SetSmoothOriginal (false);
	ASSERT_TRUE (algo.Apply (he));

	for (int i = 0; i < 8; ++i)
	{
		EXPECT_NEAR (m->m_pVertices[3*i+0], corners[i][0], 1e-5f);
		EXPECT_NEAR (m->m_pVertices[3*i+1], corners[i][1], 1e-5f);
		EXPECT_NEAR (m->m_pVertices[3*i+2], corners[i][2], 1e-5f);
	}

	delete he;
}

TEST(TEST_cgmesh_subdivision_sqrt3, SmoothingMovesCorners)
{
	// With m_smoothOriginal = true (default), original cube corners are
	// moved off the cube boundary by Kobbelt's α(n) mask.
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	std::vector<std::array<float,3>> originals;
	for (int i = 0; i < 8; ++i) originals.push_back (get_vertex (m, i));

	MeshAlgoSubdivisionSqrt3 algo;
	algo.SetSmoothOriginal (true);
	ASSERT_TRUE (algo.Apply (he));

	for (int i = 0; i < 8; ++i)
	{
		auto p = get_vertex (m, i);
		float dx = p[0] - originals[i][0];
		float dy = p[1] - originals[i][1];
		float dz = p[2] - originals[i][2];
		float d  = std::sqrt (dx*dx + dy*dy + dz*dz);
		EXPECT_GT (d, 0.05f) << "corner " << i << " barely moved";
		// Corners stay strictly inside the cube.
		EXPECT_GT (p[0], 0.0f + 1e-5f); EXPECT_LT (p[0], 1.0f - 1e-5f);
		EXPECT_GT (p[1], 0.0f + 1e-5f); EXPECT_LT (p[1], 1.0f - 1e-5f);
		EXPECT_GT (p[2], 0.0f + 1e-5f); EXPECT_LT (p[2], 1.0f - 1e-5f);
	}

	delete he;
}

TEST(TEST_cgmesh_subdivision_sqrt3, CentroidPlacement)
{
	// New vertices (index >= nv0) must be at face centroids of the original
	// triangulation (regardless of smoothing : centroid is computed from
	// pre-smoothed vertex positions in step 1).
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	const int nv0 = (int)m->m_nVertices;

	// Capture original face centroids.
	std::vector<std::array<double,3>> bary;
	for (unsigned int f = 0; f < m->m_nFaces; ++f)
	{
		Face *F = m->m_pFaces[f];
		int a = F->GetVertex(0), b = F->GetVertex(1), c = F->GetVertex(2);
		bary.push_back ({
			(m->m_pVertices[3*a]   + m->m_pVertices[3*b]   + m->m_pVertices[3*c])   / 3.0,
			(m->m_pVertices[3*a+1] + m->m_pVertices[3*b+1] + m->m_pVertices[3*c+1]) / 3.0,
			(m->m_pVertices[3*a+2] + m->m_pVertices[3*b+2] + m->m_pVertices[3*c+2]) / 3.0
		});
	}

	MeshAlgoSubdivisionSqrt3 algo;
	algo.SetSmoothOriginal (true);
	ASSERT_TRUE (algo.Apply (he));

	// Each of the 12 new vertices must coincide with one of the 12 captured centroids.
	for (unsigned int i = nv0; i < m->m_nVertices; ++i)
	{
		double x = m->m_pVertices[3*i+0];
		double y = m->m_pVertices[3*i+1];
		double z = m->m_pVertices[3*i+2];
		double best = 1e9;
		for (auto &b : bary)
		{
			double dx = x - b[0], dy = y - b[1], dz = z - b[2];
			double d  = std::sqrt (dx*dx + dy*dy + dz*dz);
			if (d < best) best = d;
		}
		EXPECT_LT (best, 1e-5) << "new vertex " << i << " is not at any face centroid";
	}

	delete he;
}

TEST(TEST_cgmesh_subdivision_sqrt3, EdgesBetweenOldCornersAreFlipped)
{
	// On a closed cube, every original edge is interior and gets flipped : after
	// subdivision, NO edge connects two original cube corners directly.
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	MeshAlgoSubdivisionSqrt3 algo;
	algo.SetSmoothOriginal (false);   // doesn't matter for connectivity
	ASSERT_TRUE (algo.Apply (he));

	for (int a = 0; a < 8; ++a)
		for (int b = a+1; b < 8; ++b)
		{
			EXPECT_FALSE (has_edge (m, a, b))
				<< "original cube edge {" << a << "," << b << "} should have been flipped";
		}

	delete he;
}

TEST(TEST_cgmesh_subdivision_sqrt3, BoundaryEdgesNotFlipped)
{
	// On a single triangle (all 3 edges are boundary), no edge should be flipped.
	// The 3 new triangles must each contain one of the original boundary edges.
	Mesh_half_edge *he = make_flat_triangle ();
	Mesh *m = he->m_pMesh;

	MeshAlgoSubdivisionSqrt3 algo;
	algo.SetSmoothOriginal (false);
	ASSERT_TRUE (algo.Apply (he));

	// nv : 3 -> 4, nf : 1 -> 3
	ASSERT_EQ ((int)m->m_nVertices, 4);
	ASSERT_EQ ((int)m->m_nFaces, 3);

	// All 3 original edges {0,1}, {1,2}, {2,0} must STILL be present
	// (boundary edges aren't flipped).
	EXPECT_TRUE (has_edge (m, 0, 1));
	EXPECT_TRUE (has_edge (m, 1, 2));
	EXPECT_TRUE (has_edge (m, 2, 0));

	delete he;
}

TEST(TEST_cgmesh_subdivision_sqrt3, FlatTriangleCentroid)
{
	// New vertex (index 3) of the flat triangle must be at exact centroid (1, 1, 0).
	Mesh_half_edge *he = make_flat_triangle ();
	Mesh *m = he->m_pMesh;

	MeshAlgoSubdivisionSqrt3 algo;
	algo.SetSmoothOriginal (false);   // keep original corners for clean test
	ASSERT_TRUE (algo.Apply (he));

	auto v3 = get_vertex (m, 3);
	EXPECT_NEAR (v3[0], 1.0f, 1e-5f);
	EXPECT_NEAR (v3[1], 1.0f, 1e-5f);
	EXPECT_NEAR (v3[2], 0.0f, 1e-5f);

	delete he;
}

TEST(TEST_cgmesh_subdivision_sqrt3, FlatTriangleBoundarySmoothing)
{
	// Boundary smoothing on the flat triangle :
	//   vertex 0 has boundary neighbors V_L = vertex 2 = (0,3,0), V_R = vertex 1 = (3,0,0)
	//   (or vice versa — depending on walk direction)
	//   V_0' = (4/27)(0,3,0) + (19/27)(0,0,0) + (4/27)(3,0,0) = (12/27, 12/27, 0)
	//        ≈ (0.4444, 0.4444, 0)
	const float exp_x = 12.0f / 27.0f;
	const float exp_y = 12.0f / 27.0f;

	Mesh_half_edge *he = make_flat_triangle ();
	Mesh *m = he->m_pMesh;

	MeshAlgoSubdivisionSqrt3 algo;
	algo.SetSmoothOriginal (true);
	ASSERT_TRUE (algo.Apply (he));

	auto v0 = get_vertex (m, 0);
	EXPECT_NEAR (v0[0], exp_x, 1e-4f);
	EXPECT_NEAR (v0[1], exp_y, 1e-4f);
	EXPECT_NEAR (v0[2], 0.0f, 1e-4f);

	delete he;
}

TEST(TEST_cgmesh_subdivision_sqrt3, FaceIndicesValid)
{
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	MeshAlgoSubdivisionSqrt3 algo;
	ASSERT_TRUE (algo.Apply (he));

	for (unsigned int f = 0; f < m->m_nFaces; ++f)
	{
		Face *F = m->m_pFaces[f];
		ASSERT_EQ (F->GetNVertices(), 3);
		int a = F->GetVertex(0), b = F->GetVertex(1), c = F->GetVertex(2);
		EXPECT_GE (a, 0); EXPECT_LT ((unsigned)a, m->m_nVertices);
		EXPECT_GE (b, 0); EXPECT_LT ((unsigned)b, m->m_nVertices);
		EXPECT_GE (c, 0); EXPECT_LT ((unsigned)c, m->m_nVertices);
		EXPECT_NE (a, b);
		EXPECT_NE (b, c);
		EXPECT_NE (a, c);
	}

	delete he;
}

TEST(TEST_cgmesh_subdivision_sqrt3, EulerCharacteristicPreservedOnCube)
{
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	auto chi = [](Mesh *mm) {
		return (int)mm->m_nVertices - count_unique_edges (mm) + (int)mm->m_nFaces;
	};
	EXPECT_EQ (chi (m), 2);

	MeshAlgoSubdivisionSqrt3 algo;
	ASSERT_TRUE (algo.Apply (he));
	EXPECT_EQ (chi (m), 2);

	ASSERT_TRUE (algo.Apply (he));
	EXPECT_EQ (chi (m), 2);

	delete he;
}

TEST(TEST_cgmesh_subdivision_sqrt3, MultipleIterationsGrowAs3Power)
{
	// Face count grows as 3^n per iteration.
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	MeshAlgoSubdivisionSqrt3 algo;
	algo.SetSmoothOriginal (false);   // keep things clean — focus on counts

	int nf_expected = 12;
	for (int it = 0; it < 3; ++it)
	{
		ASSERT_TRUE (algo.Apply (he));
		nf_expected *= 3;
		EXPECT_EQ ((int)m->m_nFaces, nf_expected) << "iter " << it;
	}

	delete he;
}

TEST(TEST_cgmesh_subdivision_sqrt3, NewVerticesHaveValenceSix)
{
	// On a closed mesh, after one √3 iteration the new (centroid) vertices
	// are connected to : 3 old vertices (their source triangle's corners) +
	// 3 new vertices (the centroids of the 3 adjacent triangles, via flipped
	// edges) = valence 6.
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	const int nv0 = (int)m->m_nVertices;

	MeshAlgoSubdivisionSqrt3 algo;
	algo.SetSmoothOriginal (false);
	ASSERT_TRUE (algo.Apply (he));

	// For each new vertex (index >= nv0), count incident faces (= valence
	// in our face-based connectivity check).
	std::vector<std::set<int>> nbrs (m->m_nVertices);
	for (unsigned int f = 0; f < m->m_nFaces; ++f)
	{
		Face *F = m->m_pFaces[f];
		int a = F->GetVertex(0), b = F->GetVertex(1), c = F->GetVertex(2);
		nbrs[a].insert(b); nbrs[a].insert(c);
		nbrs[b].insert(a); nbrs[b].insert(c);
		nbrs[c].insert(a); nbrs[c].insert(b);
	}

	for (int v = nv0; v < (int)m->m_nVertices; ++v)
	{
		EXPECT_EQ ((int)nbrs[v].size(), 6) << "new vertex " << v
			<< " has valence " << nbrs[v].size() << " (expected 6)";
	}

	delete he;
}
