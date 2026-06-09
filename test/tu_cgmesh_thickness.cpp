#include <gtest/gtest.h>

#include <vector>

#include "../src/cgmesh/cgmesh.h"
#include "../src/cgmesh/thickness.h"

namespace
{
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

	// Closed, outward-oriented box [0,4]x[0,4]x[0,1]. The bottom face is fanned
	// into 4 triangles around an OFF-centre vertex (#8 at (1.5,2,0)) so its
	// inward ray (+z) lands in the interior of the flat top face, away from any
	// edge — giving a clean expected thickness of 1.0 (the z-gap). The top face
	// is a flat quad (2 triangles); the 4 sides are quads (2 triangles each).
	Mesh* makeClosedBox()
	{
		std::vector<float> verts = {
			0,0,0,   4,0,0,   4,4,0,   0,4,0,   // 0..3 bottom corners
			0,0,1,   4,0,1,   4,4,1,   0,4,1,   // 4..7 top corners
			1.5f,2,0                            // 8 bottom off-centre
		};
		std::vector<unsigned int> faces = {
			// bottom (outward -z), fanned around vertex 8
			8,1,0,  8,2,1,  8,3,2,  8,0,3,
			// top (outward +z)
			4,5,6,  4,6,7,
			// side y=0 (outward -y)
			0,1,5,  0,5,4,
			// side x=4 (outward +x)
			1,2,6,  1,6,5,
			// side y=4 (outward +y)
			2,3,7,  2,7,6,
			// side x=0 (outward -x)
			3,0,4,  3,4,7
		};
		return makeMesh(verts, faces);
	}

	const unsigned int kBottomCentre = 8u;

	// K copies of the closed box, translated along x by a gap wide enough that
	// no vertex's inward ray reaches a neighbour. Produces 14*K triangles —
	// enough to force the octree past its single-leaf threshold, exercising
	// multi-leaf traversal + AABB pruning. Bottom-centre vertex of copy k is
	// at index 8 + 9*k and must still report thickness ~1.0.
	Mesh* makeReplicatedBoxes(unsigned int K)
	{
		const float baseV[27] = {
			0,0,0,   4,0,0,   4,4,0,   0,4,0,
			0,0,1,   4,0,1,   4,4,1,   0,4,1,
			1.5f,2,0
		};
		const unsigned int baseF[42] = {
			8,1,0,  8,2,1,  8,3,2,  8,0,3,
			4,5,6,  4,6,7,
			0,1,5,  0,5,4,
			1,2,6,  1,6,5,
			2,3,7,  2,7,6,
			3,0,4,  3,4,7
		};
		std::vector<float> verts;
		std::vector<unsigned int> faces;
		for (unsigned int k = 0; k < K; ++k) {
			const float dx = 10.0f * k;        // gap of 6 between 4-wide boxes
			for (int i = 0; i < 9; ++i) {
				verts.push_back(baseV[3*i]   + dx);
				verts.push_back(baseV[3*i+1]);
				verts.push_back(baseV[3*i+2]);
			}
			for (int i = 0; i < 42; ++i)
				faces.push_back(baseF[i] + 9u * k);
		}
		return makeMesh(verts, faces);
	}
}

// ---------------------------------------------------------------------------
// Correctness
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_thickness, BottomCentreVertexMatchesGap)
{
	Mesh *m = makeClosedBox();

	std::vector<float> thick;
	std::vector<char>  defined;
	ASSERT_TRUE(MeshAlgoThickness::ComputeWallThickness(*m, thick, defined));

	ASSERT_EQ(thick.size(), m->GetNVertices());
	ASSERT_EQ(defined.size(), m->GetNVertices());

	// Vertex 8 has a -z normal; its inward ray spans the z-gap = 1.0.
	EXPECT_EQ(defined[kBottomCentre], (char)1);
	EXPECT_NEAR(thick[kBottomCentre], 1.0f, 1e-3f);

	delete m;
}

TEST(TEST_cgmesh_thickness, NoSelfIntersectionAtStartVertex)
{
	// If the start vertex's own incident faces were counted, the thickness
	// would collapse to ~0. The guard must keep it at the real gap.
	Mesh *m = makeClosedBox();

	std::vector<float> thick;
	std::vector<char>  defined;
	ASSERT_TRUE(MeshAlgoThickness::ComputeWallThickness(*m, thick, defined));

	EXPECT_GT(thick[kBottomCentre], 0.9f);
	delete m;
}

TEST(TEST_cgmesh_thickness, AllValuesWithinBounds)
{
	Mesh *m = makeClosedBox();

	std::vector<float> thick;
	std::vector<char>  defined;
	ASSERT_TRUE(MeshAlgoThickness::ComputeWallThickness(*m, thick, defined));

	const float diag = m->bbox_diagonal_length();
	for (unsigned int i = 0; i < m->GetNVertices(); ++i)
	{
		if (!defined[i])
			continue;
		EXPECT_GT(thick[i], 0.0f);
		EXPECT_LE(thick[i], diag + 1e-3f);   // never exceeds the bbox diagonal
	}
	delete m;
}

TEST(TEST_cgmesh_thickness, OpenMeshLeaksToUndefined)
{
	// A single triangle is open: the inward ray escapes, no opposite face.
	std::vector<float> verts = { 0,0,0,  1,0,0,  0,1,0 };
	std::vector<unsigned int> faces = { 0,1,2 };
	Mesh *m = makeMesh(verts, faces);

	std::vector<float> thick;
	std::vector<char>  defined;
	ASSERT_TRUE(MeshAlgoThickness::ComputeWallThickness(*m, thick, defined));

	for (unsigned int i = 0; i < m->GetNVertices(); ++i)
		EXPECT_EQ(defined[i], (char)0) << "vertex " << i << " should be undefined on an open mesh";

	delete m;
}

TEST(TEST_cgmesh_thickness, EmptyMeshReturnsFalse)
{
	Mesh *m = new Mesh();
	m->Init();

	std::vector<float> thick;
	std::vector<char>  defined;
	EXPECT_FALSE(MeshAlgoThickness::ComputeWallThickness(*m, thick, defined));

	delete m;
}

TEST(TEST_cgmesh_thickness, NonTriangleMeshReturnsFalse)
{
	// A single quad face: the algorithm only handles triangle meshes.
	std::vector<float> verts = { 0,0,0,  1,0,0,  1,1,0,  0,1,0 };
	Mesh *m = new Mesh();
	m->Init();
	m->SetVertices(4, const_cast<float*>(verts.data()));
	std::vector<unsigned int> quad = { 0,1,2,3 };
	m->SetFaces(1, 4, const_cast<unsigned int*>(quad.data()));

	std::vector<float> thick;
	std::vector<char>  defined;
	EXPECT_FALSE(MeshAlgoThickness::ComputeWallThickness(*m, thick, defined));

	delete m;
}

// ---------------------------------------------------------------------------
// Visualisation (V1)
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_thickness, OctreeMultiLeafReplicatedBoxes)
{
	// Many triangles -> multi-leaf octree. Each box's bottom-centre vertex
	// must still measure the z-gap (1.0), proving the octree traversal +
	// AABB pruning find the same nearest hit as a full scan.
	const unsigned int K = 10;
	Mesh *m = makeReplicatedBoxes(K);

	std::vector<float> thick;
	std::vector<char>  defined;
	ASSERT_TRUE(MeshAlgoThickness::ComputeWallThickness(*m, thick, defined));

	for (unsigned int k = 0; k < K; ++k) {
		const unsigned int bc = 8u + 9u * k;
		EXPECT_EQ(defined[bc], (char)1) << "box " << k << " bottom-centre undefined";
		EXPECT_NEAR(thick[bc], 1.0f, 1e-3f) << "box " << k;
	}
	delete m;
}

// ---------------------------------------------------------------------------
// Shape Diameter Function (M1)
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_thickness, SdfSingleRayDegeneratesToWallThickness)
{
	// numRays=1, cone=0, no smoothing -> a single inward-normal ray, i.e.
	// exactly the M2 wall thickness.
	Mesh *m = makeClosedBox();

	std::vector<float> wall, sdf;
	std::vector<char>  wallDef, sdfDef;
	ASSERT_TRUE(MeshAlgoThickness::ComputeWallThickness(*m, wall, wallDef));
	ASSERT_TRUE(MeshAlgoThickness::ComputeShapeDiameter(*m, sdf, sdfDef,
	                                                    /*numRays*/1, /*cone*/0.f, /*smooth*/0));

	EXPECT_EQ(sdfDef[kBottomCentre], (char)1);
	EXPECT_NEAR(sdf[kBottomCentre], wall[kBottomCentre], 1e-4f);
	EXPECT_NEAR(sdf[kBottomCentre], 1.0f, 1e-3f);
	delete m;
}

TEST(TEST_cgmesh_thickness, SdfConeStaysWithinGeometricBounds)
{
	// On the thin slab, the bottom-centre cone (axis +z) cannot measure less
	// than the perpendicular gap (1.0, the nearest opposite face) and the
	// slanted rays cap near 1/cos(60deg)=2.0. Robust mean must land in between.
	Mesh *m = makeClosedBox();

	std::vector<float> sdf;
	std::vector<char>  defined;
	ASSERT_TRUE(MeshAlgoThickness::ComputeShapeDiameter(*m, sdf, defined,
	                                                    /*numRays*/16, /*cone*/60.f, /*smooth*/0));

	EXPECT_EQ(defined[kBottomCentre], (char)1);
	EXPECT_GE(sdf[kBottomCentre], 1.0f - 1e-3f);   // never below the true gap
	EXPECT_LE(sdf[kBottomCentre], 2.1f);           // bounded by the cone slant
	delete m;
}

TEST(TEST_cgmesh_thickness, SdfIsDeterministic)
{
	// Sampling is deterministic (no RNG): identical input -> identical output.
	Mesh *m = makeClosedBox();

	std::vector<float> a, b;
	std::vector<char>  ad, bd;
	ASSERT_TRUE(MeshAlgoThickness::ComputeShapeDiameter(*m, a, ad, 16, 60.f, 1));
	ASSERT_TRUE(MeshAlgoThickness::ComputeShapeDiameter(*m, b, bd, 16, 60.f, 1));

	ASSERT_EQ(a.size(), b.size());
	for (size_t i = 0; i < a.size(); ++i)
		EXPECT_FLOAT_EQ(a[i], b[i]) << "vertex " << i;
	delete m;
}

TEST(TEST_cgmesh_thickness, SdfSmoothingKeepsDefinedAndBounded)
{
	Mesh *m = makeClosedBox();

	std::vector<float> sdf;
	std::vector<char>  defined;
	ASSERT_TRUE(MeshAlgoThickness::ComputeShapeDiameter(*m, sdf, defined, 16, 60.f, /*smooth*/3));

	const float diag = m->bbox_diagonal_length();
	for (unsigned int i = 0; i < m->GetNVertices(); ++i)
		if (defined[i]) {
			EXPECT_GT(sdf[i], 0.0f);
			EXPECT_LE(sdf[i], diag + 1e-3f);
		}
	EXPECT_EQ(defined[kBottomCentre], (char)1);
	delete m;
}

TEST(TEST_cgmesh_thickness, SdfRejectsEmptyAndNonTriangle)
{
	std::vector<float> v; std::vector<char> d;

	Mesh *empty = new Mesh(); empty->Init();
	EXPECT_FALSE(MeshAlgoThickness::ComputeShapeDiameter(*empty, v, d));
	delete empty;

	std::vector<float> verts = { 0,0,0,  1,0,0,  1,1,0,  0,1,0 };
	std::vector<unsigned int> quad = { 0,1,2,3 };
	Mesh *m = new Mesh(); m->Init();
	m->SetVertices(4, const_cast<float*>(verts.data()));
	m->SetFaces(1, 4, const_cast<unsigned int*>(quad.data()));
	EXPECT_FALSE(MeshAlgoThickness::ComputeShapeDiameter(*m, v, d));
	delete m;
}

TEST(TEST_cgmesh_thickness, ColorizeFillsVertexColors)
{
	Mesh *m = makeClosedBox();

	std::vector<float> thick;
	std::vector<char>  defined;
	ASSERT_TRUE(MeshAlgoThickness::ColorizeWallThickness(*m, thick, defined));

	// The existing colour-map path must have produced one RGB triple/vertex.
	EXPECT_EQ(m->m_pVertexColors.size(), 3u * m->GetNVertices());
	delete m;
}

TEST(TEST_cgmesh_thickness, ColorizeThinIsRedThickIsBlue)
{
	// Manufacturing convention: thinnest vertex -> red (r>b), thickest -> blue.
	Mesh *m = makeClosedBox();

	std::vector<float> t; std::vector<char> d;
	ASSERT_TRUE(MeshAlgoThickness::ComputeWallThickness(*m, t, d));

	int imin = -1, imax = -1;
	for (unsigned int i = 0; i < m->GetNVertices(); ++i)
		if (d[i]) {
			if (imin < 0 || t[i] < t[imin]) imin = (int)i;
			if (imax < 0 || t[i] > t[imax]) imax = (int)i;
		}
	ASSERT_GE(imin, 0); ASSERT_GE(imax, 0);
	ASSERT_GT(t[imax], t[imin]);             // there is a thickness spread

	ASSERT_TRUE(MeshAlgoThickness::ColorizeWallThickness(*m, t, d));   // auto scale
	const std::vector<float> &c = m->m_pVertexColors;
	EXPECT_GT(c[3*imin],     c[3*imin + 2]); // thin  -> red  (R > B)
	EXPECT_GT(c[3*imax + 2], c[3*imax]);     // thick -> blue (B > R)
	delete m;
}

TEST(TEST_cgmesh_thickness, ColorizeRespectsExplicitScale)
{
	// With an explicit scale above the real thickness, the bottom-centre
	// (~1.0, below scaleMin) clamps to the thin/red end.
	Mesh *m = makeClosedBox();

	std::vector<float> t; std::vector<char> d;
	ASSERT_TRUE(MeshAlgoThickness::ColorizeWallThickness(*m, t, d,
	                                                     /*scaleMin*/1.5f, /*scaleMax*/1.6f));
	const std::vector<float> &c = m->m_pVertexColors;
	EXPECT_GT(c[3*kBottomCentre], c[3*kBottomCentre + 2]);   // clamped -> red
	delete m;
}
