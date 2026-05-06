#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <set>
#include <unordered_set>
#include <vector>

#include "../src/cgmesh/cgmesh.h"
#include "../src/cgmesh/subdivision_loop.h"

namespace
{

// Build the unit cube [0,1]^3 as a closed triangulated surface :
//   8 vertices (cube corners)
//  12 triangles (2 per face, 6 faces)
//  18 unique undirected edges (12 cube edges + 6 face diagonals)
//
// All triangles are oriented outward (CCW seen from outside).
// This is a valid 2-manifold without boundary, so every vertex is interior.
// Vertex valences are mixed (3 corners with valence 4, 3 corners with valence 5,
// 2 corners with valence 6) depending on diagonal choices.
//
static Mesh_half_edge *
make_unit_cube ()
{
	const float V[8 * 3] = {
		0,0,0,  // 0
		1,0,0,  // 1
		1,1,0,  // 2
		0,1,0,  // 3
		0,0,1,  // 4
		1,0,1,  // 5
		1,1,1,  // 6
		0,1,1   // 7
	};

	// 6 faces, each split along a fixed diagonal. Outward winding.
	const unsigned int F[12 * 3] = {
		// bottom z=0  (normal -z) : viewed from below CCW
		0, 2, 1,
		0, 3, 2,
		// top z=1     (normal +z) : viewed from above CCW
		4, 5, 6,
		4, 6, 7,
		// front y=0   (normal -y) : viewed from -y CCW
		0, 1, 5,
		0, 5, 4,
		// back y=1    (normal +y) : viewed from +y CCW
		2, 3, 7,
		2, 7, 6,
		// left x=0    (normal -x) : viewed from -x CCW
		0, 4, 7,
		0, 7, 3,
		// right x=1   (normal +x) : viewed from +x CCW
		1, 2, 6,
		1, 6, 5
	};

	Mesh_half_edge *he = new Mesh_half_edge ();
	he->m_pMesh->Init ();
	he->m_pMesh->SetVertices (8, const_cast<float*>(V));
	he->m_pMesh->SetFaces (12, 3, const_cast<unsigned int*>(F));
	// Force half-edge build.
	he->create_half_edge ();
	(void)he->GetCheMesh ();
	return he;
}

// Get a vertex (x,y,z) from the mesh.
static std::array<float,3>
get_vertex (Mesh *m, int i)
{
	return { m->m_pVertices[3*i], m->m_pVertices[3*i+1], m->m_pVertices[3*i+2] };
}

// Compare two vec3 for near-equality.
static bool
near_eq (const std::array<float,3> &a, const std::array<float,3> &b, float eps = 1e-5f)
{
	return std::fabs(a[0]-b[0]) < eps
	    && std::fabs(a[1]-b[1]) < eps
	    && std::fabs(a[2]-b[2]) < eps;
}

// Compute axis-aligned bounding box.
struct BBox { std::array<float,3> mn, mx; };
static BBox
bbox (Mesh *m)
{
	BBox b;
	b.mn = { m->m_pVertices[0], m->m_pVertices[1], m->m_pVertices[2] };
	b.mx = b.mn;
	for (unsigned int i = 1; i < m->m_nVertices; ++i)
	{
		for (int k = 0; k < 3; ++k)
		{
			float x = m->m_pVertices[3*i+k];
			if (x < b.mn[k]) b.mn[k] = x;
			if (x > b.mx[k]) b.mx[k] = x;
		}
	}
	return b;
}

// Sum of triangle areas (surface area of the mesh).
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

// Centroid of all vertex positions.
static std::array<double,3>
vertex_centroid (Mesh *m)
{
	std::array<double,3> c { 0,0,0 };
	for (unsigned int i = 0; i < m->m_nVertices; ++i)
	{
		c[0] += m->m_pVertices[3*i];
		c[1] += m->m_pVertices[3*i+1];
		c[2] += m->m_pVertices[3*i+2];
	}
	if (m->m_nVertices > 0)
	{
		c[0] /= m->m_nVertices;
		c[1] /= m->m_nVertices;
		c[2] /= m->m_nVertices;
	}
	return c;
}

// Count unique undirected edges in a triangle mesh.
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

TEST(TEST_cgmesh_subdivision_loop, CubeBaseline)
{
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	EXPECT_EQ (m->m_nVertices, 8u);
	EXPECT_EQ (m->m_nFaces, 12u);
	EXPECT_EQ (count_unique_edges (m), 18);
	// Surface area of the unit cube : 6.
	EXPECT_NEAR (total_area (m), 6.0, 1e-5);

	delete he;
}

TEST(TEST_cgmesh_subdivision_loop, MidpointModeCounts)
{
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	const int nv0 = m->m_nVertices;
	const int nf0 = m->m_nFaces;
	const int ne0 = count_unique_edges (m);

	MeshAlgoSubdivisionLoop algo;
	EXPECT_FALSE (algo.GetUseWarrenMask ());     // default off
	ASSERT_TRUE (algo.Apply (he));

	// 1->4 split : faces *= 4, vertices += unique_edges, edges *= 2 + 3*old_faces.
	EXPECT_EQ ((int)m->m_nFaces, 4 * nf0);
	EXPECT_EQ ((int)m->m_nVertices, nv0 + ne0);

	// All 8 original cube corners must remain at their original positions.
	const float corners[8][3] = {
		{0,0,0},{1,0,0},{1,1,0},{0,1,0},
		{0,0,1},{1,0,1},{1,1,1},{0,1,1}
	};
	for (int i = 0; i < 8; ++i)
	{
		EXPECT_NEAR (m->m_pVertices[3*i+0], corners[i][0], 1e-5f);
		EXPECT_NEAR (m->m_pVertices[3*i+1], corners[i][1], 1e-5f);
		EXPECT_NEAR (m->m_pVertices[3*i+2], corners[i][2], 1e-5f);
	}

	// New vertices (from index 8 onward) must lie inside the unit cube
	// (they are midpoints of edges between cube corners on the unit cube surface).
	for (unsigned int i = 8; i < m->m_nVertices; ++i)
	{
		EXPECT_GE (m->m_pVertices[3*i+0], 0.0f - 1e-5f);
		EXPECT_LE (m->m_pVertices[3*i+0], 1.0f + 1e-5f);
		EXPECT_GE (m->m_pVertices[3*i+1], 0.0f - 1e-5f);
		EXPECT_LE (m->m_pVertices[3*i+1], 1.0f + 1e-5f);
		EXPECT_GE (m->m_pVertices[3*i+2], 0.0f - 1e-5f);
		EXPECT_LE (m->m_pVertices[3*i+2], 1.0f + 1e-5f);
	}

	// Surface area is preserved by midpoint refinement (no smoothing).
	EXPECT_NEAR (total_area (m), 6.0, 1e-4);

	// Bounding box must remain [0,1]^3.
	BBox bb = bbox (m);
	for (int k = 0; k < 3; ++k)
	{
		EXPECT_NEAR (bb.mn[k], 0.0f, 1e-5f);
		EXPECT_NEAR (bb.mx[k], 1.0f, 1e-5f);
	}

	delete he;
}

TEST(TEST_cgmesh_subdivision_loop, MidpointPositionsAreEdgeMidpoints)
{
	// After one midpoint-mode subdivision, every new vertex (index >= 8) must
	// be the midpoint of two original cube corners.
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	const float corners[8][3] = {
		{0,0,0},{1,0,0},{1,1,0},{0,1,0},
		{0,0,1},{1,0,1},{1,1,1},{0,1,1}
	};

	MeshAlgoSubdivisionLoop algo;
	ASSERT_TRUE (algo.Apply (he));

	for (unsigned int i = 8; i < m->m_nVertices; ++i)
	{
		float x = m->m_pVertices[3*i+0];
		float y = m->m_pVertices[3*i+1];
		float z = m->m_pVertices[3*i+2];
		bool found = false;
		for (int a = 0; a < 8 && !found; ++a)
			for (int b = a+1; b < 8 && !found; ++b)
			{
				float mx = 0.5f * (corners[a][0] + corners[b][0]);
				float my = 0.5f * (corners[a][1] + corners[b][1]);
				float mz = 0.5f * (corners[a][2] + corners[b][2]);
				if (std::fabs(mx - x) < 1e-5f
				 && std::fabs(my - y) < 1e-5f
				 && std::fabs(mz - z) < 1e-5f)
					found = true;
			}
		EXPECT_TRUE (found) << "new vertex " << i << " (" << x << "," << y << "," << z
		                    << ") is not the midpoint of any two cube corners";
	}

	delete he;
}

TEST(TEST_cgmesh_subdivision_loop, MidpointModeIterations)
{
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	int nv = m->m_nVertices;
	int nf = m->m_nFaces;

	MeshAlgoSubdivisionLoop algo;
	for (int it = 0; it < 3; ++it)
	{
		ASSERT_TRUE (algo.Apply (he));
		// faces *= 4 each iteration
		EXPECT_EQ ((int)m->m_nFaces, 4 * nf);
		nf = m->m_nFaces;
		// vertex count strictly increases
		EXPECT_GT ((int)m->m_nVertices, nv);
		nv = m->m_nVertices;
		// surface area preserved
		EXPECT_NEAR (total_area (m), 6.0, 1e-3);
	}

	delete he;
}

TEST(TEST_cgmesh_subdivision_loop, WarrenModeCounts)
{
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	const int nv0 = m->m_nVertices;
	const int nf0 = m->m_nFaces;
	const int ne0 = count_unique_edges (m);

	MeshAlgoSubdivisionLoop algo;
	algo.SetUseWarrenMask (true);
	ASSERT_TRUE (algo.Apply (he));

	// Same combinatorial result as midpoint mode : 4f, +ne vertices.
	EXPECT_EQ ((int)m->m_nFaces, 4 * nf0);
	EXPECT_EQ ((int)m->m_nVertices, nv0 + ne0);

	delete he;
}

TEST(TEST_cgmesh_subdivision_loop, WarrenShrinksTowardCentroid)
{
	// Warren smoothing on a closed cube :
	//  - every original corner is interior (closed mesh), so its position
	//    contracts toward the average of its neighbors ;
	//  - surface area decreases.
	//
	// Notes :
	//   * The vertex centroid is NOT preserved exactly because our cube
	//     triangulation has asymmetric corner valences (3, 4, 5, 6).
	//   * The bbox does NOT shrink after a single iteration : the new midpoint
	//     of a face diagonal is computed from 4 vertices that are all on the
	//     same face plane, so the midpoint stays on that face. Bbox shrinking
	//     requires multiple iterations (see WarrenIterationsConverge).
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	// Cache original corner positions before subdivision.
	std::vector<std::array<float,3>> corners_before;
	for (int i = 0; i < 8; ++i)
		corners_before.push_back (get_vertex (m, i));

	MeshAlgoSubdivisionLoop algo;
	algo.SetUseWarrenMask (true);
	ASSERT_TRUE (algo.Apply (he));

	// All 8 original corners must have moved strictly inward (none stays on boundary).
	for (int i = 0; i < 8; ++i)
	{
		auto p = get_vertex (m, i);
		EXPECT_GT (p[0], 0.0f + 1e-5f) << "corner " << i << " kept x=0";
		EXPECT_LT (p[0], 1.0f - 1e-5f) << "corner " << i << " kept x=1";
		EXPECT_GT (p[1], 0.0f + 1e-5f) << "corner " << i << " kept y=0";
		EXPECT_LT (p[1], 1.0f - 1e-5f) << "corner " << i << " kept y=1";
		EXPECT_GT (p[2], 0.0f + 1e-5f) << "corner " << i << " kept z=0";
		EXPECT_LT (p[2], 1.0f - 1e-5f) << "corner " << i << " kept z=1";
		// Non-trivial movement : at least 0.05 from original.
		float dx = p[0] - corners_before[i][0];
		float dy = p[1] - corners_before[i][1];
		float dz = p[2] - corners_before[i][2];
		float d  = std::sqrt (dx*dx + dy*dy + dz*dz);
		EXPECT_GT (d, 0.05f) << "corner " << i << " barely moved";
	}

	// Centroid stays inside the cube and close to its center.
	auto c1 = vertex_centroid (m);
	EXPECT_NEAR (c1[0], 0.5, 0.05);
	EXPECT_NEAR (c1[1], 0.5, 0.05);
	EXPECT_NEAR (c1[2], 0.5, 0.05);

	// Surface area must DECREASE (smoothing shrinks the surface).
	EXPECT_LT (total_area (m), 6.0);

	delete he;
}

TEST(TEST_cgmesh_subdivision_loop, WarrenMidpointStencilOnCube)
{
	// Pick a specific edge of the cube : (0,0,0) -> (1,0,0) (cube edge at y=0,z=0).
	// In our triangulation the two adjacent triangles are :
	//   bottom face : 0-2-1  (uses edge 0->1 reversed as 1->0; opposite vertex = 2)
	//   wait: bottom is [0,2,1]; its edges are (0,2),(2,1),(1,0). So the cube edge 0-1 is
	//         the half-edge 1->0 in this face, with opposite vertex 2.
	//   front face  : 0-1-5 ; edges (0,1),(1,5),(5,0). Half-edge 0->1 has opposite vertex 5.
	// So for the undirected edge {0,1} : opposite vertices are V2 = vertex 2, V3 = vertex 5.
	// Warren stencil (interior) :
	//   M = (3/8)(V0 + V1) + (1/8)(V2 + V3)
	//     = (3/8)((0,0,0)+(1,0,0)) + (1/8)((1,1,0)+(1,0,1))
	//     = (3/8)(1,0,0) + (1/8)(2,1,1)
	//     = (3/8 + 2/8, 0 + 1/8, 0 + 1/8)
	//     = (5/8, 1/8, 1/8)
	const float expected[3] = { 5.0f/8.0f, 1.0f/8.0f, 1.0f/8.0f };

	Mesh_half_edge *he = make_unit_cube ();
	MeshAlgoSubdivisionLoop algo;
	algo.SetUseWarrenMask (true);
	ASSERT_TRUE (algo.Apply (he));

	Mesh *m = he->m_pMesh;
	bool found = false;
	for (unsigned int i = 8; i < m->m_nVertices; ++i)
	{
		float x = m->m_pVertices[3*i+0];
		float y = m->m_pVertices[3*i+1];
		float z = m->m_pVertices[3*i+2];
		if (std::fabs(x - expected[0]) < 1e-4f
		 && std::fabs(y - expected[1]) < 1e-4f
		 && std::fabs(z - expected[2]) < 1e-4f)
		{
			found = true;
			break;
		}
	}
	EXPECT_TRUE (found) << "Warren midpoint (5/8, 1/8, 1/8) for cube edge {0,0,0}-{1,0,0} "
	                       "not present in the subdivided mesh";

	delete he;
}

TEST(TEST_cgmesh_subdivision_loop, WarrenIterationsConverge)
{
	// Multiple Warren iterations : surface area monotonically decreases,
	// bbox monotonically shrinks. (Centroid is not preserved exactly by
	// asymmetric triangulations — see WarrenShrinksTowardCentroid.)
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	double area_prev = total_area (m);
	BBox   bb_prev   = bbox (m);

	MeshAlgoSubdivisionLoop algo;
	algo.SetUseWarrenMask (true);

	for (int it = 0; it < 3; ++it)
	{
		ASSERT_TRUE (algo.Apply (he));

		double area_now = total_area (m);
		EXPECT_LT (area_now, area_prev + 1e-6) << "iter " << it << " : area not monotonic";
		area_prev = area_now;

		BBox bb_now = bbox (m);
		for (int k = 0; k < 3; ++k)
		{
			EXPECT_GE (bb_now.mn[k], bb_prev.mn[k] - 1e-5f)
				<< "iter " << it << " axis " << k << " : min retreated";
			EXPECT_LE (bb_now.mx[k], bb_prev.mx[k] + 1e-5f)
				<< "iter " << it << " axis " << k << " : max expanded";
		}
		bb_prev = bb_now;
	}

	// After 3 iterations the mesh fits comfortably inside the unit cube :
	// bbox extent is noticeably below 1.0.
	BBox bb = bbox (m);
	for (int k = 0; k < 3; ++k)
	{
		EXPECT_LT (bb.mx[k] - bb.mn[k], 1.0f - 0.05f) << "axis " << k
			<< " : Warren iterations should shrink the cube noticeably";
	}

	delete he;
}

TEST(TEST_cgmesh_subdivision_loop, WarrenAndMidpointDisagreeOnCorners)
{
	// Corners are preserved in midpoint mode but moved in Warren mode.
	Mesh_half_edge *he_mid = make_unit_cube ();
	Mesh_half_edge *he_war = make_unit_cube ();

	MeshAlgoSubdivisionLoop algo_mid;
	MeshAlgoSubdivisionLoop algo_war;
	algo_war.SetUseWarrenMask (true);

	ASSERT_TRUE (algo_mid.Apply (he_mid));
	ASSERT_TRUE (algo_war.Apply (he_war));

	// Vertex 0 (= cube corner (0,0,0)) is at exactly the origin in midpoint mode...
	auto v0_mid = get_vertex (he_mid->m_pMesh, 0);
	EXPECT_TRUE (near_eq (v0_mid, {0,0,0}));

	// ... but is moved (strictly inside the unit cube) in Warren mode.
	auto v0_war = get_vertex (he_war->m_pMesh, 0);
	EXPECT_FALSE (near_eq (v0_war, {0,0,0}, 1e-3f));
	EXPECT_GT (v0_war[0], 0.0f);
	EXPECT_GT (v0_war[1], 0.0f);
	EXPECT_GT (v0_war[2], 0.0f);
	EXPECT_LT (v0_war[0], 1.0f);
	EXPECT_LT (v0_war[1], 1.0f);
	EXPECT_LT (v0_war[2], 1.0f);

	delete he_mid;
	delete he_war;
}

TEST(TEST_cgmesh_subdivision_loop, FaceIndicesAreInRange)
{
	// After subdivision, every new face index must be a valid vertex index.
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	MeshAlgoSubdivisionLoop algo;
	algo.SetUseWarrenMask (true);
	ASSERT_TRUE (algo.Apply (he));

	for (unsigned int f = 0; f < m->m_nFaces; ++f)
	{
		Face *F = m->m_pFaces[f];
		ASSERT_EQ (F->GetNVertices(), 3);
		for (int k = 0; k < 3; ++k)
		{
			int v = F->GetVertex(k);
			EXPECT_GE (v, 0);
			EXPECT_LT ((unsigned)v, m->m_nVertices);
		}
		// No degenerate triangles (no repeated index).
		int a = F->GetVertex(0), b = F->GetVertex(1), c = F->GetVertex(2);
		EXPECT_NE (a, b);
		EXPECT_NE (b, c);
		EXPECT_NE (a, c);
	}

	delete he;
}

TEST(TEST_cgmesh_subdivision_loop, EulerCharacteristicPreserved)
{
	// Closed manifold => V - E + F = 2 must be preserved across iterations.
	Mesh_half_edge *he = make_unit_cube ();
	Mesh *m = he->m_pMesh;

	auto chi = [](Mesh *mm) {
		return (int)mm->m_nVertices - count_unique_edges (mm) + (int)mm->m_nFaces;
	};
	EXPECT_EQ (chi (m), 2);

	MeshAlgoSubdivisionLoop algo;
	algo.SetUseWarrenMask (true);
	ASSERT_TRUE (algo.Apply (he));
	EXPECT_EQ (chi (m), 2);

	ASSERT_TRUE (algo.Apply (he));
	EXPECT_EQ (chi (m), 2);

	delete he;
}
