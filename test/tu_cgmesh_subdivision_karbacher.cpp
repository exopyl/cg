#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <set>
#include <vector>

#include "../src/cgmesh/cgmesh.h"
#include "../src/cgmesh/subdivision_karbacher.h"

namespace
{

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

// Build the unit cube [0,1]^3 as a closed triangulated surface (8 v, 12 f).
// Per-vertex normals are computed from face normals via Mesh::ComputeNormals.
static Mesh_half_edge *
make_unit_cube ()
{
	const float V[8 * 3] = {
		0,0,0,  1,0,0,  1,1,0,  0,1,0,
		0,0,1,  1,0,1,  1,1,1,  0,1,1
	};
	const unsigned int F[12 * 3] = {
		0, 2, 1,   0, 3, 2,                 // bottom
		4, 5, 6,   4, 6, 7,                 // top
		0, 1, 5,   0, 5, 4,                 // front
		2, 3, 7,   2, 7, 6,                 // back
		0, 4, 7,   0, 7, 3,                 // left
		1, 2, 6,   1, 6, 5                  // right
	};

	Mesh_half_edge *he = new Mesh_half_edge ();
	he->m_pMesh->Init ();
	he->m_pMesh->SetVertices (8, const_cast<float*>(V));
	he->m_pMesh->SetFaces (12, 3, const_cast<unsigned int*>(F));
	he->m_pMesh->ComputeNormals ();
	he->create_half_edge ();
	(void)he->GetCheMesh ();
	return he;
}

// Build a regular tetrahedron centered at the origin.
// 4 vertices, 4 triangles, all faces oriented outward.
// Vertex normals are computed.
static Mesh_half_edge *
make_tetrahedron ()
{
	// Standard regular tetrahedron coords (radius 1 from center).
	const float s = 1.0f / std::sqrt(3.0f);
	const float V[4 * 3] = {
		 s,  s,  s,    // 0
		-s, -s,  s,    // 1
		-s,  s, -s,    // 2
		 s, -s, -s     // 3
	};
	// Outward winding for each face.
	const unsigned int F[4 * 3] = {
		0, 1, 2,   // top-ish
		0, 3, 1,
		0, 2, 3,
		1, 3, 2
	};

	Mesh_half_edge *he = new Mesh_half_edge ();
	he->m_pMesh->Init ();
	he->m_pMesh->SetVertices (4, const_cast<float*>(V));
	he->m_pMesh->SetFaces (4, 3, const_cast<unsigned int*>(F));
	he->m_pMesh->ComputeNormals ();
	he->create_half_edge ();
	(void)he->GetCheMesh ();
	return he;
}

// Build a single XY-plane triangle with all 3 vertex normals = (0, 0, 1).
// Used to test the flat-surface case where Karbacher's normal displacement
// term degenerates to zero, leaving the new vertex at the exact centroid.
static Mesh_half_edge *
make_flat_triangle ()
{
	const float V[3 * 3] = {
		0.0f, 0.0f, 0.0f,
		3.0f, 0.0f, 0.0f,
		0.0f, 3.0f, 0.0f
	};
	const float N[3 * 3] = {
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f
	};
	const unsigned int F[3] = { 0, 1, 2 };

	Mesh_half_edge *he = new Mesh_half_edge ();
	he->m_pMesh->Init ();
	he->m_pMesh->SetVertices (3, const_cast<float*>(V));
	he->m_pMesh->SetVertexNormals (3, const_cast<float*>(N));
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

struct BBox { std::array<float,3> mn, mx; };
static BBox
bbox (Mesh *m)
{
	BBox b;
	b.mn = { m->m_pVertices[0], m->m_pVertices[1], m->m_pVertices[2] };
	b.mx = b.mn;
	for (unsigned int i = 1; i < m->m_nVertices; ++i)
		for (int k = 0; k < 3; ++k)
		{
			float x = m->m_pVertices[3*i+k];
			if (x < b.mn[k]) b.mn[k] = x;
			if (x > b.mx[k]) b.mx[k] = x;
		}
	return b;
}

static double
total_area (Mesh *m)
{
	double s = 0.0;
	for (unsigned int f = 0; f < m->m_nFaces; ++f)
	{
		Face *F = m->m_pFaces[f];
		if (F->GetNVertices() != 3) continue;
		int a = F->GetVertex(0), b = F->GetVertex(1), c = F->GetVertex(2);
		double ax = m->m_pVertices[3*a],   ay = m->m_pVertices[3*a+1], az = m->m_pVertices[3*a+2];
		double bx = m->m_pVertices[3*b],   by = m->m_pVertices[3*b+1], bz = m->m_pVertices[3*b+2];
		double cx = m->m_pVertices[3*c],   cy = m->m_pVertices[3*c+1], cz = m->m_pVertices[3*c+2];
		double ux = bx-ax, uy = by-ay, uz = bz-az;
		double vx = cx-ax, vy = cy-ay, vz = cz-az;
		double nx = uy*vz - uz*vy, ny = uz*vx - ux*vz, nz = ux*vy - uy*vx;
		s += 0.5 * std::sqrt (nx*nx + ny*ny + nz*nz);
	}
	return s;
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

} // anon

// ============================================================================
// Tests
// ============================================================================

TEST(TEST_cgmesh_subdivision_karbacher, FlatTriangleNewVertexIsCentroid)
{
	// For a flat triangle (all 3 vertex normals identical and orthogonal to
	// the plane), Karbacher's normal-based displacement is zero by construction
	// (cosalpha = 1 -> sqrt(...) factor -> 0, and posv_i is in-plane so
	// npos . posv_i = 0). The new vertex must therefore land exactly at the
	// triangle's barycenter.
	Mesh_half_edge *he = make_flat_triangle ();
	Mesh *m = he->m_pMesh;

	MeshAlgoSubdivisionKarbacher algo;
	ASSERT_TRUE (algo.Apply (he));

	// 1->3 split : nv = 3 + 1 = 4, nf = 3 * 1 = 3
	ASSERT_EQ ((int)m->m_nVertices, 4);
	ASSERT_EQ ((int)m->m_nFaces, 3);

	// Original 3 vertices must be unchanged.
	auto v0 = get_vertex (m, 0); auto v1 = get_vertex (m, 1); auto v2 = get_vertex (m, 2);
	EXPECT_NEAR (v0[0], 0.0f, 1e-5f); EXPECT_NEAR (v0[1], 0.0f, 1e-5f); EXPECT_NEAR (v0[2], 0.0f, 1e-5f);
	EXPECT_NEAR (v1[0], 3.0f, 1e-5f); EXPECT_NEAR (v1[1], 0.0f, 1e-5f); EXPECT_NEAR (v1[2], 0.0f, 1e-5f);
	EXPECT_NEAR (v2[0], 0.0f, 1e-5f); EXPECT_NEAR (v2[1], 3.0f, 1e-5f); EXPECT_NEAR (v2[2], 0.0f, 1e-5f);

	// New vertex (index 3) must be exactly at centroid (1, 1, 0).
	auto v3 = get_vertex (m, 3);
	EXPECT_NEAR (v3[0], 1.0f, 1e-4f);
	EXPECT_NEAR (v3[1], 1.0f, 1e-4f);
	EXPECT_NEAR (v3[2], 0.0f, 1e-4f);

	delete he;
}

TEST(TEST_cgmesh_subdivision_karbacher, TetrahedronCounts)
{
	// 1->3 split on a tetrahedron : nv 4 -> 4+4=8 ; nf 4 -> 4*3=12.
	Mesh_half_edge *he = make_tetrahedron ();
	Mesh *m = he->m_pMesh;

	const int nv0 = (int)m->m_nVertices;
	const int nf0 = (int)m->m_nFaces;

	MeshAlgoSubdivisionKarbacher algo;
	ASSERT_TRUE (algo.Apply (he));

	EXPECT_EQ ((int)m->m_nVertices, nv0 + nf0);
	EXPECT_EQ ((int)m->m_nFaces, 3 * nf0);

	delete he;
}

TEST(TEST_cgmesh_subdivision_karbacher, TetrahedronOriginalVerticesPreserved)
{
	// Karbacher's Apply copies the original v[] verbatim into v_new[] before
	// inserting new centroid vertices. The original vertices must be unchanged.
	Mesh_half_edge *he = make_tetrahedron ();
	Mesh *m = he->m_pMesh;

	std::vector<std::array<float,3>> originals;
	for (int i = 0; i < 4; ++i) originals.push_back (get_vertex (m, i));

	MeshAlgoSubdivisionKarbacher algo;
	ASSERT_TRUE (algo.Apply (he));

	for (int i = 0; i < 4; ++i)
	{
		auto p = get_vertex (m, i);
		EXPECT_NEAR (p[0], originals[i][0], 1e-5f) << "vertex " << i;
		EXPECT_NEAR (p[1], originals[i][1], 1e-5f) << "vertex " << i;
		EXPECT_NEAR (p[2], originals[i][2], 1e-5f) << "vertex " << i;
	}

	delete he;
}

TEST(TEST_cgmesh_subdivision_karbacher, CubeCounts)
{
	// 1->3 split on the unit cube : nv 8 -> 8+12=20 ; nf 12 -> 36.
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	const int nv0 = (int)m->m_nVertices;
	const int nf0 = (int)m->m_nFaces;

	MeshAlgoSubdivisionKarbacher algo;
	ASSERT_TRUE (algo.Apply (he));

	EXPECT_EQ ((int)m->m_nVertices, nv0 + nf0);
	EXPECT_EQ ((int)m->m_nFaces, 3 * nf0);

	delete he;
}

TEST(TEST_cgmesh_subdivision_karbacher, CubeOriginalCornersPreserved)
{
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	const float corners[8][3] = {
		{0,0,0},{1,0,0},{1,1,0},{0,1,0},
		{0,0,1},{1,0,1},{1,1,1},{0,1,1}
	};

	MeshAlgoSubdivisionKarbacher algo;
	ASSERT_TRUE (algo.Apply (he));

	for (int i = 0; i < 8; ++i)
	{
		EXPECT_NEAR (m->m_pVertices[3*i+0], corners[i][0], 1e-5f);
		EXPECT_NEAR (m->m_pVertices[3*i+1], corners[i][1], 1e-5f);
		EXPECT_NEAR (m->m_pVertices[3*i+2], corners[i][2], 1e-5f);
	}

	delete he;
}

TEST(TEST_cgmesh_subdivision_karbacher, NewVerticesNearTriangleBarycenters)
{
	// Each new vertex (index >= nv0) must lie close to the barycenter of one
	// of the ORIGINAL faces. The displacement is in the normal direction and
	// for the unit cube remains modest (< 0.4 in absolute distance).
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	// Capture original face barycenters before subdivision.
	std::vector<std::array<double,3>> bary;
	for (unsigned int f = 0; f < m->m_nFaces; ++f)
	{
		Face *F = m->m_pFaces[f];
		int a = F->GetVertex(0), b = F->GetVertex(1), c = F->GetVertex(2);
		double bx = (m->m_pVertices[3*a+0] + m->m_pVertices[3*b+0] + m->m_pVertices[3*c+0]) / 3.0;
		double by = (m->m_pVertices[3*a+1] + m->m_pVertices[3*b+1] + m->m_pVertices[3*c+1]) / 3.0;
		double bz = (m->m_pVertices[3*a+2] + m->m_pVertices[3*b+2] + m->m_pVertices[3*c+2]) / 3.0;
		bary.push_back ({bx, by, bz});
	}

	const int nv0 = (int)m->m_nVertices;

	MeshAlgoSubdivisionKarbacher algo;
	ASSERT_TRUE (algo.Apply (he));

	// For each new vertex, find the closest original-face barycenter and check
	// the distance is small.
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
		EXPECT_LT (best, 0.4) << "new vertex " << i << " is too far from any face barycenter";
	}

	delete he;
}

TEST(TEST_cgmesh_subdivision_karbacher, NewVerticesInsideUnitCube)
{
	// Karbacher displaces new vertices in the (averaged) normal direction.
	// On a convex closed mesh with normals pointing outward, displacement is
	// outward — but moderate. New vertices remain within a small expansion
	// of the original bounding box.
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	const int nv0 = (int)m->m_nVertices;

	MeshAlgoSubdivisionKarbacher algo;
	ASSERT_TRUE (algo.Apply (he));

	// All new vertex coords stay finite and within an envelope around [0,1]^3.
	for (unsigned int i = nv0; i < m->m_nVertices; ++i)
	{
		float x = m->m_pVertices[3*i+0];
		float y = m->m_pVertices[3*i+1];
		float z = m->m_pVertices[3*i+2];
		EXPECT_TRUE (std::isfinite (x));
		EXPECT_TRUE (std::isfinite (y));
		EXPECT_TRUE (std::isfinite (z));
		EXPECT_GE (x, -1.0f);  EXPECT_LE (x, 2.0f);
		EXPECT_GE (y, -1.0f);  EXPECT_LE (y, 2.0f);
		EXPECT_GE (z, -1.0f);  EXPECT_LE (z, 2.0f);
	}

	delete he;
}

TEST(TEST_cgmesh_subdivision_karbacher, FaceIndicesValid)
{
	// All faces must be triangles ; all indices must reference valid vertices ;
	// no degenerate face (no repeated vertex index).
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	MeshAlgoSubdivisionKarbacher algo;
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

TEST(TEST_cgmesh_subdivision_karbacher, EulerCharacteristicPreservedOnCube)
{
	// Closed manifold => V - E + F = 2 must be preserved.
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	auto chi = [](Mesh *mm) {
		return (int)mm->m_nVertices - count_unique_edges (mm) + (int)mm->m_nFaces;
	};
	EXPECT_EQ (chi (m), 2);

	MeshAlgoSubdivisionKarbacher algo;
	ASSERT_TRUE (algo.Apply (he));
	EXPECT_EQ (chi (m), 2);

	delete he;
}

TEST(TEST_cgmesh_subdivision_karbacher, EulerCharacteristicPreservedOnTetrahedron)
{
	Mesh_half_edge *he = make_tetrahedron ();
	Mesh *m = he->m_pMesh;

	auto chi = [](Mesh *mm) {
		return (int)mm->m_nVertices - count_unique_edges (mm) + (int)mm->m_nFaces;
	};
	EXPECT_EQ (chi (m), 2);

	MeshAlgoSubdivisionKarbacher algo;
	ASSERT_TRUE (algo.Apply (he));
	EXPECT_EQ (chi (m), 2);

	delete he;
}

TEST(TEST_cgmesh_subdivision_karbacher, AreaIncreasesOnConvex)
{
	// On a convex closed mesh (cube), Karbacher displaces the new centroids
	// outward along the averaged normal. The total triangulated area must
	// therefore increase relative to the original cube surface (= 6.0).
	//
	// Note : the displacement is bounded ; we expect a modest but definite
	// increase, not an explosion.
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	const double area0 = total_area (m);
	EXPECT_NEAR (area0, 6.0, 1e-4);

	MeshAlgoSubdivisionKarbacher algo;
	ASSERT_TRUE (algo.Apply (he));

	const double area1 = total_area (m);
	EXPECT_GT (area1, area0);             // strict increase
	EXPECT_LT (area1, 1.5 * area0);       // bounded (no runaway)

	delete he;
}

TEST(TEST_cgmesh_subdivision_karbacher, MultipleIterationsRequireFreshTopology)
{
	// Karbacher modifies the cached half-edge structure in place but does NOT
	// recompute vertex normals. Iterating naively after a single Apply() would
	// read past the (stale) m_pVertexNormals array. To iterate safely the
	// caller must :
	//   1. recompute vertex normals (mesh now has new vertices)
	//   2. invalidate the half-edge cache so it is rebuilt from the new mesh
	//
	// This test verifies the documented usage pattern works correctly.
	Mesh_half_edge *he = make_tetrahedron ();
	Mesh *m = he->m_pMesh;

	const int nf0 = (int)m->m_nFaces;
	const int nv0 = (int)m->m_nVertices;

	MeshAlgoSubdivisionKarbacher algo;
	ASSERT_TRUE (algo.Apply (he));

	// After iter 1 : nv0+nf0 vertices, 3*nf0 faces.
	EXPECT_EQ ((int)m->m_nVertices, nv0 + nf0);
	EXPECT_EQ ((int)m->m_nFaces, 3 * nf0);

	// Prepare for iter 2 : recompute normals (was stale-sized) ; rebuild HE.
	m->ComputeNormals ();
	he->create_half_edge ();
	(void)he->GetCheMesh ();

	const int nf1 = (int)m->m_nFaces;
	const int nv1 = (int)m->m_nVertices;

	ASSERT_TRUE (algo.Apply (he));

	// After iter 2 : counts grow by 1->3 again.
	EXPECT_EQ ((int)m->m_nVertices, nv1 + nf1);
	EXPECT_EQ ((int)m->m_nFaces, 3 * nf1);

	// Closed manifold property preserved.
	auto chi = [](Mesh *mm) {
		return (int)mm->m_nVertices - count_unique_edges (mm) + (int)mm->m_nFaces;
	};
	EXPECT_EQ (chi (m), 2);

	delete he;
}
