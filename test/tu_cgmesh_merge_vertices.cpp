#include <gtest/gtest.h>

#include <chrono>
#include <random>
#include <vector>

#include "../src/cgmesh/cgmesh.h"

namespace
{
	// Build a Mesh from raw vertex/face arrays.
	Mesh* makeMesh(const std::vector<float> &verts, const std::vector<unsigned int> &faces)
	{
		Mesh *m = new Mesh();
		m->Init();
		const unsigned int nv = (unsigned int)(verts.size() / 3);
		const unsigned int nf = (unsigned int)(faces.size() / 3);
		m->SetVertices(nv, const_cast<float*>(verts.data()));
		m->SetFaces(nf, 3, const_cast<unsigned int*>(faces.data()));
		return m;
	}
}

// ---------------------------------------------------------------------------
// Correctness
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_merge_vertices, ExactDuplicatesAreMerged)
{
	// Two coincident vertices, one shared by two triangles.
	std::vector<float> verts = {
		0,0,0,   1,0,0,   1,0,0,   0,1,0,   1,1,0
	};
	// Triangles : (0,1,3) and (2,4,3) — vertex 1 and 2 are coincident.
	std::vector<unsigned int> faces = {
		0, 1, 3,
		2, 4, 3
	};
	Mesh *m = makeMesh(verts, faces);

	int merged = m->MergeVertices(1e-6f);

	EXPECT_EQ(merged, 1);                          // one duplicate removed
	EXPECT_EQ(m->m_nVertices, 4u);                 // 5 -> 4
	EXPECT_EQ(m->m_nFaces, 2u);                    // both triangles still valid

	delete m;
}

TEST(TEST_cgmesh_merge_vertices, NearDuplicatesWithinToleranceMerge)
{
	// Two vertices at distance 0.01 apart.
	std::vector<float> verts = {
		0, 0, 0,
		0.01f, 0, 0,
		1, 0, 0,
		0, 1, 0
	};
	std::vector<unsigned int> faces = { 0, 1, 2,  1, 3, 2 };
	Mesh *m = makeMesh(verts, faces);

	int merged = m->MergeVertices(0.05f);          // tolerance > 0.01

	EXPECT_EQ(merged, 1);
	EXPECT_EQ(m->m_nVertices, 3u);

	delete m;
}

TEST(TEST_cgmesh_merge_vertices, NearDuplicatesOutsideToleranceKept)
{
	std::vector<float> verts = {
		0, 0, 0,
		0.1f, 0, 0,
		1, 0, 0
	};
	std::vector<unsigned int> faces = { 0, 1, 2 };
	Mesh *m = makeMesh(verts, faces);

	int merged = m->MergeVertices(0.05f);          // tolerance < 0.1

	EXPECT_EQ(merged, 0);
	EXPECT_EQ(m->m_nVertices, 3u);

	delete m;
}

TEST(TEST_cgmesh_merge_vertices, DegenerateFacesRemoved)
{
	// Vertices 0 and 1 are duplicates. Triangle (0,1,2) becomes degenerate
	// after the merge and must be dropped.
	std::vector<float> verts = {
		0, 0, 0,
		0, 0, 0,
		1, 0, 0,
		0, 1, 0
	};
	std::vector<unsigned int> faces = {
		0, 1, 2,        // becomes (0,0,2) -> degenerate
		0, 2, 3,        // (0,2,3) -> still valid
	};
	Mesh *m = makeMesh(verts, faces);

	int merged = m->MergeVertices(1e-6f);

	EXPECT_EQ(merged, 1);
	EXPECT_EQ(m->m_nVertices, 3u);
	EXPECT_EQ(m->m_nFaces, 1u);                    // degenerate dropped

	delete m;
}

TEST(TEST_cgmesh_merge_vertices, BoundaryCellsAreProbed)
{
	// Two vertices straddling a hash-grid cell boundary at distance < tolerance.
	// With cell size = tolerance, they fall in adjacent cells and the algorithm
	// must probe the 27-neighbourhood to find the duplicate.
	const float tol = 0.1f;
	std::vector<float> verts = {
		tol * 0.999f, 0, 0,             // just below cell boundary
		tol * 1.001f, 0, 0              // just above cell boundary, 0.002 away
	};
	std::vector<unsigned int> faces = { /* none */ };
	Mesh *m = makeMesh(verts, faces);

	int merged = m->MergeVertices(tol);

	EXPECT_EQ(merged, 1)
		<< "Vertices straddling a cell boundary must still be merged when within tolerance";
	EXPECT_EQ(m->m_nVertices, 1u);

	delete m;
}

TEST(TEST_cgmesh_merge_vertices, FaceIndicesAreRemappedConsistently)
{
	// Build a quad as 2 triangles using 5 vertices where v2 == v0 (duplicates).
	std::vector<float> verts = {
		0, 0, 0,        // 0
		1, 0, 0,        // 1
		0, 0, 0,        // 2 == 0
		0, 1, 0,        // 3
		1, 1, 0         // 4
	};
	std::vector<unsigned int> faces = {
		0, 1, 3,
		2, 4, 3
	};
	Mesh *m = makeMesh(verts, faces);
	m->MergeVertices(1e-6f);

	ASSERT_EQ(m->m_nFaces, 2u);
	// After merge, vertex 2 should be remapped to 0.
	for (unsigned int f = 0; f < m->m_nFaces; ++f)
		for (int k = 0; k < 3; ++k)
		{
			int v = m->m_pFaces[f]->GetVertex(k);
			EXPECT_GE(v, 0);
			EXPECT_LT((unsigned)v, m->m_nVertices);
		}

	delete m;
}

// ---------------------------------------------------------------------------
// Performance — sanity that the spatial-hash optimization scales.
// O(N^2) baseline on 50k vertices would take seconds ; O(N) average should
// finish in a few hundred milliseconds at most.
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_merge_vertices, ScalesToFiftyThousandVertices)
{
	const unsigned int N = 50000;

	// Half unique random vertices, half exact duplicates.
	std::mt19937 rng(42);
	std::uniform_real_distribution<float> dist(-100.0f, 100.0f);
	std::vector<float> verts;
	verts.reserve(3 * N);
	const unsigned int nUnique = N / 2;
	for (unsigned int i = 0; i < nUnique; ++i)
	{
		verts.push_back(dist(rng));
		verts.push_back(dist(rng));
		verts.push_back(dist(rng));
	}
	// Append duplicates (each unique vertex appears once more).
	for (unsigned int i = 0; i < nUnique; ++i)
	{
		verts.push_back(verts[3*i+0]);
		verts.push_back(verts[3*i+1]);
		verts.push_back(verts[3*i+2]);
	}
	std::vector<unsigned int> faces;
	Mesh *m = makeMesh(verts, faces);

	auto t0 = std::chrono::steady_clock::now();
	int merged = m->MergeVertices(1e-6f);
	auto t1 = std::chrono::steady_clock::now();
	double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
	std::cout << "[merge_vertices] " << N << " vertices (" << nUnique
	          << " unique + " << nUnique << " duplicates) merged in " << ms << " ms\n";

	EXPECT_EQ(merged, (int)nUnique);
	EXPECT_EQ(m->m_nVertices, nUnique);
	// Sanity time bound : on a debug build we want < 5 seconds. The naive
	// O(N^2) algorithm would take ~250 seconds on this size.
	EXPECT_LT(ms, 5000.0)
		<< "MergeVertices took " << ms << " ms ; expected < 5000 (O(N) implementation)";

	delete m;
}
