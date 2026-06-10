#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "../src/cgmesh/cgmesh.h"
#include "../src/cgmesh/bvh.h"

namespace
{
	Mesh* makeMesh(const std::vector<float> &verts, const std::vector<unsigned int> &faces)
	{
		Mesh *m = new Mesh();
		m->Init();
		m->SetVertices((unsigned int)(verts.size() / 3), const_cast<float*>(verts.data()));
		m->SetFaces((unsigned int)(faces.size() / 3), 3, const_cast<unsigned int*>(faces.data()));
		return m;
	}
}

// A single triangle in the z = 5 plane; an axis-aligned ray straight up from
// below it must report the exact plane distance (5). Exercises build + nearest
// + the slab test's axis-parallel branch (dir x,y == 0).
TEST(TEST_cgmesh_bvh, SingleTriangleHitDistance)
{
	std::vector<float> v = { 0,0,5,  2,0,5,  0,2,5 };
	std::vector<unsigned int> f = { 0,1,2 };
	Mesh *m = makeMesh(v, f);

	BVH bvh; bvh.build(*m);
	float o[3] = { 0.3f, 0.3f, 0.f }, d[3] = { 0.f, 0.f, 1.f };
	EXPECT_NEAR(bvh.nearest(o, d, 1e-4f), 5.0f, 1e-4f);
	delete m;
}

// A ray whose path lies outside the triangle's footprint hits nothing.
TEST(TEST_cgmesh_bvh, RayMissReturnsMinusOne)
{
	std::vector<float> v = { 0,0,5,  2,0,5,  0,2,5 };
	std::vector<unsigned int> f = { 0,1,2 };
	Mesh *m = makeMesh(v, f);

	BVH bvh; bvh.build(*m);
	float o[3] = { 5.f, 5.f, 0.f }, d[3] = { 0.f, 0.f, 1.f };   // outside footprint
	EXPECT_LT(bvh.nearest(o, d, 1e-4f), 0.0f);
	// Same triangle, ray pointing away from it -> no hit either.
	float dn[3] = { 0.f, 0.f, -1.f };
	float oin[3] = { 0.3f, 0.3f, 0.f };
	EXPECT_LT(bvh.nearest(oin, dn, 1e-4f), 0.0f);
	delete m;
}

// Two parallel triangles; nearest must return the closer one (no culling, so
// orientation is irrelevant).
TEST(TEST_cgmesh_bvh, NearestSelectsClosest)
{
	std::vector<float> v = {
		0,0,3,  4,0,3,  0,4,3,     // near plane z=3
		0,0,7,  4,0,7,  0,4,7      // far  plane z=7
	};
	std::vector<unsigned int> f = { 0,1,2,  3,4,5 };
	Mesh *m = makeMesh(v, f);

	BVH bvh; bvh.build(*m);
	float o[3] = { 0.5f, 0.5f, 0.f }, d[3] = { 0.f, 0.f, 1.f };
	EXPECT_NEAR(bvh.nearest(o, d, 1e-4f), 3.0f, 1e-4f);
	delete m;
}

// tMin must reject a hit that is too close (e.g. the start vertex's own faces),
// so the next valid intersection is returned instead.
TEST(TEST_cgmesh_bvh, TMinRejectsTooCloseHit)
{
	std::vector<float> v = {
		0,0,0.001f,  4,0,0.001f,  0,4,0.001f,   // almost at the origin
		0,0,5,       4,0,5,       0,4,5
	};
	std::vector<unsigned int> f = { 0,1,2,  3,4,5 };
	Mesh *m = makeMesh(v, f);

	BVH bvh; bvh.build(*m);
	float o[3] = { 0.5f, 0.5f, 0.f }, d[3] = { 0.f, 0.f, 1.f };
	EXPECT_NEAR(bvh.nearest(o, d, /*tMin*/0.01f), 5.0f, 1e-4f);   // 0.001 hit skipped
	delete m;
}

// An oblique ray (all direction components non-zero) must hit a planar triangle
// at the exact Euclidean distance — exercises the general slab branch.
TEST(TEST_cgmesh_bvh, ObliqueRayExactDistance)
{
	std::vector<float> v = { -10,-10,5,  10,-10,5,  0,15,5 };   // wide z=5 triangle
	std::vector<unsigned int> f = { 0,1,2 };
	Mesh *m = makeMesh(v, f);

	BVH bvh; bvh.build(*m);
	const float inv = 1.0f / std::sqrt(27.0f);
	float o[3] = { 0.f, 0.f, 0.f };
	float d[3] = { 1.f * inv, 1.f * inv, 5.f * inv };           // normalised (1,1,5)
	// Reaches z=5 at parametric t = 5 / dir_z = sqrt(27).
	EXPECT_NEAR(bvh.nearest(o, d, 1e-4f), std::sqrt(27.0f), 1e-3f);
	delete m;
}

// Many parallel planes -> a multi-node BVH. Validates traversal/pruning and
// tMin selection against analytically known distances.
TEST(TEST_cgmesh_bvh, StackedPlanesMultiNode)
{
	const unsigned int N = 30;            // 60 triangles -> deep BVH (LEAF=4)
	std::vector<float> v;
	std::vector<unsigned int> f;
	for (unsigned int k = 1; k <= N; ++k)
	{
		const unsigned int base = (unsigned int)(v.size() / 3);
		const float z = (float)k;
		v.insert(v.end(), { 0,0,z,  10,0,z,  10,10,z,  0,10,z });
		f.insert(f.end(), { base, base+1, base+2,  base, base+2, base+3 });
	}
	Mesh *m = makeMesh(v, f);

	BVH bvh; bvh.build(*m);
	float o[3] = { 5.f, 5.f, 0.f }, d[3] = { 0.f, 0.f, 1.f };
	EXPECT_NEAR(bvh.nearest(o, d, 1e-4f), 1.0f, 1e-4f);   // first plane (z=1)
	EXPECT_NEAR(bvh.nearest(o, d, 5.5f),  6.0f, 1e-4f);   // tMin skips z<=5.5 -> z=6
	float dn[3] = { 0.f, 0.f, -1.f };
	EXPECT_LT(bvh.nearest(o, dn, 1e-4f), 0.0f);           // nothing below z=0
	delete m;
}

// Degenerate input: a BVH built from an empty mesh answers every ray with -1.
TEST(TEST_cgmesh_bvh, EmptyMeshReturnsMinusOne)
{
	Mesh *m = new Mesh();
	m->Init();

	BVH bvh; bvh.build(*m);
	float o[3] = { 0.f, 0.f, 0.f }, d[3] = { 0.f, 0.f, 1.f };
	EXPECT_LT(bvh.nearest(o, d, 1e-4f), 0.0f);
	delete m;
}
