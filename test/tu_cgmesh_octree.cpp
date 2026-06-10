#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <vector>

#include "../src/cgmesh/octree.h"

namespace
{
	// A 3x3x3 integer grid (27 points, spacing 1, bbox [0,2]^3, centre (1,1,1)).
	std::vector<float> grid333()
	{
		std::vector<float> p;
		for (int x = 0; x < 3; ++x)
		for (int y = 0; y < 3; ++y)
		for (int z = 0; z < 3; ++z)
			p.insert(p.end(), { (float)x, (float)y, (float)z });
		return p;
	}

	// traverse() callback that accumulates leaf contents.
	struct LeafIndexCounter : Octree::Callback
	{
		int leaves = 0, totalIndices = 0;
		bool operator()(Octree *o, void *) override
		{
			if (o->IsLeaf()) { leaves++; totalIndices += (int)o->GetNIndices(); }
			return true;   // visit the whole tree
		}
	};

	struct LeafTriCounter : Octree::Callback
	{
		int leaves = 0, totalTris = 0;
		bool operator()(Octree *o, void *) override
		{
			if (o->IsLeaf()) { leaves++; totalTris += (int)o->GetNTriangles(); }
			return true;
		}
	};
}

// ---------------------------------------------------------------------------
// Anti-degeneration guard (BuildForTriangles)
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_octree, GuardCapsDepthOnDegenerateInput)
{
	// 50 identical large triangles, all crossing the node centre: every split
	// duplicates the whole set into the touched octants without reducing the
	// count -> maxChild == nTriangles -> the guard makes the root a leaf.
	std::vector<float> pts = { 0,0,0,  10,10,0,  0,0,10 };
	std::vector<unsigned int> tris;
	for (int i = 0; i < 50; ++i) { tris.push_back(0); tris.push_back(1); tris.push_back(2); }

	Octree oct;
	oct.BuildForTriangles(pts.data(), 3, /*maxTriangles*/4, /*maxDepth*/8, tris.data(), 50);

	EXPECT_EQ(oct.GetMaxDepth(), 0);   // would be 8 without the guard
	EXPECT_EQ(oct.GetNLeaves(), 1);
}

TEST(TEST_cgmesh_octree, NormalSubdivisionStillHappens)
{
	// 64 small, well-separated triangles -> a centre split cleanly separates
	// them -> guard does not fire, the tree subdivides past maxTriangles.
	std::vector<float> pts;
	std::vector<unsigned int> tris;
	unsigned int idx = 0;
	for (int x = 0; x < 4; ++x)
	for (int y = 0; y < 4; ++y)
	for (int z = 0; z < 4; ++z)
	{
		const float ox = x*10.f, oy = y*10.f, oz = z*10.f;
		pts.insert(pts.end(), { ox,oy,oz,  ox+1,oy,oz,  ox,oy+1,oz });
		tris.push_back(idx); tris.push_back(idx+1); tris.push_back(idx+2);
		idx += 3;
	}

	Octree oct;
	oct.BuildForTriangles(pts.data(), (int)(pts.size()/3), 4, 8, tris.data(), (int)(tris.size()/3));

	EXPECT_GT(oct.GetMaxDepth(), 0);
	EXPECT_GT(oct.GetNLeaves(), 1);
}

// Separated triangles must be distributed without loss or duplication.
TEST(TEST_cgmesh_octree, BuildForTrianglesDistributesWithoutLoss)
{
	// One small triangle near each of the 8 octant corners of bbox ~[0.3,1.7]^3
	// (centre ~1). Each lies fully inside one octant -> assigned once.
	std::vector<float> pts;
	std::vector<unsigned int> tris;
	unsigned int idx = 0;
	for (int sx = 0; sx < 2; ++sx)
	for (int sy = 0; sy < 2; ++sy)
	for (int sz = 0; sz < 2; ++sz)
	{
		const float cx = sx ? 1.5f : 0.5f, cy = sy ? 1.5f : 0.5f, cz = sz ? 1.5f : 0.5f;
		pts.insert(pts.end(), { cx,cy,cz,  cx+0.1f,cy,cz,  cx,cy+0.1f,cz });
		tris.push_back(idx); tris.push_back(idx+1); tris.push_back(idx+2);
		idx += 3;
	}

	Octree oct;
	oct.BuildForTriangles(pts.data(), (int)(pts.size()/3), /*maxTriangles*/1, /*maxDepth*/10,
	                      tris.data(), 8);

	LeafTriCounter cb;
	oct.traverse(&cb, nullptr);
	EXPECT_EQ(cb.leaves, 8);
	EXPECT_EQ(cb.totalTris, 8);        // no triangle lost, none duplicated
	EXPECT_EQ(oct.GetNLeaves(), 8);
}

// ---------------------------------------------------------------------------
// Build (points) + bounds + neighbour queries
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_octree, BoundsAndCentre)
{
	std::vector<float> p = grid333();
	Octree oct;
	oct.Build(p.data(), 27, /*maxPoints*/2, /*maxDepth*/10);

	float mn[3], mx[3], c[3];
	oct.GetMinMax(mn, mx);
	oct.GetCenter(c);
	EXPECT_FLOAT_EQ(mn[0], 0.f); EXPECT_FLOAT_EQ(mn[1], 0.f); EXPECT_FLOAT_EQ(mn[2], 0.f);
	EXPECT_FLOAT_EQ(mx[0], 2.f); EXPECT_FLOAT_EQ(mx[1], 2.f); EXPECT_FLOAT_EQ(mx[2], 2.f);
	EXPECT_FLOAT_EQ(c[0], 1.f);  EXPECT_FLOAT_EQ(c[1], 1.f);  EXPECT_FLOAT_EQ(c[2], 1.f);
}

TEST(TEST_cgmesh_octree, KNeighboursRadiusCount)
{
	std::vector<float> p = grid333();
	Octree oct;
	oct.Build(p.data(), 27, 2, 10);        // forces subdivision -> tests recursion

	float pt[3] = { 1.f, 1.f, 1.f };
	// Within radius 1.1 of the centre: the centre itself + the 6 axis neighbours
	// at distance 1 (the 12 edge neighbours are at sqrt(2) ~ 1.41).
	EXPECT_EQ(oct.GetKNeighbours(pt, 1.1f), 7);
	// Radius 0.5: only the centre point.
	EXPECT_EQ(oct.GetKNeighbours(pt, 0.5f), 1);
	// Radius covering everything.
	EXPECT_EQ(oct.GetKNeighbours(pt, 100.f), 27);
}

TEST(TEST_cgmesh_octree, GetClosestPointsCollectsWithinRadius)
{
	std::vector<float> p = grid333();
	Octree oct;
	oct.Build(p.data(), 27, 2, 10);

	float pt[3] = { 1.f, 1.f, 1.f };
	float *out = nullptr; unsigned int n = 0;
	const int res = oct.GetClosestPoints(pt, 1.1f, &out, &n);

	EXPECT_EQ(res, 7);
	EXPECT_EQ(n, 7u);
	ASSERT_NE(out, nullptr);
	for (unsigned int i = 0; i < n; ++i)
	{
		const float dx = out[3*i] - 1.f, dy = out[3*i+1] - 1.f, dz = out[3*i+2] - 1.f;
		EXPECT_LT(std::sqrt(dx*dx + dy*dy + dz*dz), 1.1f);
	}
	free(out);
}

TEST(TEST_cgmesh_octree, GetSumNeighboursAccumulates)
{
	std::vector<float> p = grid333();
	Octree oct;
	oct.Build(p.data(), 27, 2, 10);

	float pt[3] = { 1.f, 1.f, 1.f };
	float accum[3] = { 0.f, 0.f, 0.f };
	const int count = oct.GetSumNeighbours(pt, 1.1f, accum);

	EXPECT_EQ(count, 7);
	// centre (1,1,1) + 6 axis neighbours -> componentwise sum 7 by symmetry.
	EXPECT_FLOAT_EQ(accum[0], 7.f);
	EXPECT_FLOAT_EQ(accum[1], 7.f);
	EXPECT_FLOAT_EQ(accum[2], 7.f);
}

// ---------------------------------------------------------------------------
// BuildWithIndices + traverse + index query
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_octree, BuildWithIndicesTraverseVisitsAllPoints)
{
	std::vector<float> p = grid333();
	Octree oct;
	oct.BuildWithIndices(p.data(), 27, /*maxPoints*/2, /*maxDepth*/10,
	                     /*pIndices*/nullptr, /*nIndices*/0);   // auto 0..26

	LeafIndexCounter cb;
	oct.traverse(&cb, nullptr);
	EXPECT_EQ(cb.leaves, oct.GetNLeaves());
	EXPECT_EQ(cb.totalIndices, 27);        // every index stored in exactly one leaf
}

TEST(TEST_cgmesh_octree, GetClosestIndicesPointsWithinRadius)
{
	std::vector<float> p = grid333();
	Octree oct;
	oct.BuildWithIndices(p.data(), 27, 2, 10, nullptr, 0);

	float pt[3] = { 1.f, 1.f, 1.f };
	unsigned int *out = nullptr; unsigned int n = 0;
	const int res = oct.GetClosestIndicesPoints(p.data(), pt, 1.1f, &out, &n);

	EXPECT_EQ(res, 7);
	EXPECT_EQ(n, 7u);
	ASSERT_NE(out, nullptr);
	for (unsigned int i = 0; i < n; ++i)
	{
		const unsigned int vi = out[i];
		ASSERT_LT(vi, 27u);
		const float dx = p[3*vi] - 1.f, dy = p[3*vi+1] - 1.f, dz = p[3*vi+2] - 1.f;
		EXPECT_LT(std::sqrt(dx*dx + dy*dy + dz*dz), 1.1f);
	}
	free(out);
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_octree, MaxDepthZeroGivesSingleLeaf)
{
	std::vector<float> p = grid333();
	Octree oct;
	oct.Build(p.data(), 27, /*maxPoints*/2, /*maxDepth*/0);   // depth cap at root

	EXPECT_EQ(oct.GetMaxDepth(), 0);
	EXPECT_EQ(oct.GetNLeaves(), 1);
	float pt[3] = { 1.f, 1.f, 1.f };
	EXPECT_EQ(oct.GetKNeighbours(pt, 1.1f), 7);   // single-leaf query still works
}

TEST(TEST_cgmesh_octree, SinglePoint)
{
	std::vector<float> p = { 1.f, 1.f, 1.f };
	Octree oct;
	oct.Build(p.data(), 1, 1, 5);

	float pt[3] = { 1.f, 1.f, 1.f };
	EXPECT_EQ(oct.GetKNeighbours(pt, 0.5f), 1);
	float far[3] = { 9.f, 9.f, 9.f };
	EXPECT_EQ(oct.GetKNeighbours(far, 0.5f), 0);
}

TEST(TEST_cgmesh_octree, CoincidentPointsTerminateAtMaxDepth)
{
	// 5 identical points never separate -> recursion must stop at maxDepth
	// (the depth cap), not loop forever, and the query still finds all of them.
	std::vector<float> p;
	for (int i = 0; i < 5; ++i) p.insert(p.end(), { 1.f, 1.f, 1.f });

	Octree oct;
	oct.Build(p.data(), 5, /*maxPoints*/1, /*maxDepth*/4);

	EXPECT_EQ(oct.GetMaxDepth(), 4);
	float pt[3] = { 1.f, 1.f, 1.f };
	EXPECT_EQ(oct.GetKNeighbours(pt, 0.5f), 5);
}
