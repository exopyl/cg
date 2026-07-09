#include <gtest/gtest.h>

#include <vector>
#include <cmath>
#include <cstdlib>

#include "../src/cgmesh/cgmesh.h"
#include "../src/cgmesh/bvh.h"
#include "../src/cgmesh/mesh_metrics.h"

//
// Reusable geometry/metrics primitives:
//   - cgmath  : point_triangle_distance2 (closest-point-on-triangle)
//   - cgmesh  : BVH::closest_distance2    (proximity query)
//   - cgmesh  : mesh_hausdorff            (mesh comparison)
//

TEST(TEST_cgmesh_metrics, point_triangle_distance)
{
	Vector3f a, b, c;
	a.Set ( 0.f, 0.f, 0.f);
	b.Set ( 1.f, 0.f, 0.f);
	c.Set ( 0.f, 1.f, 0.f);

	// Point straight above the interior -> distance is the height.
	Vector3f p;
	p.Set ( 0.25f, 0.25f, 2.f);
	EXPECT_NEAR(point_triangle_distance2(p, a, b, c), 4.f, 1e-4f);

	// Beyond vertex A -> closest is A.
	p.Set ( -1.f, -1.f, 0.f);
	EXPECT_NEAR(point_triangle_distance2(p, a, b, c), 2.f, 1e-4f);

	// Off edge AB -> closest is on the edge.
	p.Set ( 0.5f, -2.f, 0.f);
	EXPECT_NEAR(point_triangle_distance2(p, a, b, c), 4.f, 1e-4f);

	// In-plane interior -> distance 0, and closest point returned.
	Vector3f cl;
	p.Set ( 0.2f, 0.2f, 0.f);
	EXPECT_NEAR(point_triangle_distance2(p, a, b, c, cl), 0.f, 1e-5f);
	EXPECT_NEAR(cl[0], 0.2f, 1e-5f);
	EXPECT_NEAR(cl[1], 0.2f, 1e-5f);
}

// Flat z-plane grid as a triangle Mesh.
static Mesh *flat_grid(int N, float z)
{
	int side = N + 1;
	std::vector<float> verts;
	for (int j = 0; j < side; j++)
		for (int i = 0; i < side; i++)
		{
			verts.push_back((float)i / N);
			verts.push_back((float)j / N);
			verts.push_back(z);
		}
	std::vector<unsigned int> faces;
	for (int j = 0; j < N; j++)
		for (int i = 0; i < N; i++)
		{
			unsigned int v00 = j * side + i, v10 = j * side + i + 1;
			unsigned int v01 = (j + 1) * side + i, v11 = (j + 1) * side + i + 1;
			faces.push_back(v00); faces.push_back(v10); faces.push_back(v11);
			faces.push_back(v00); faces.push_back(v11); faces.push_back(v01);
		}
	Mesh *m = new Mesh();
	m->SetVertices((unsigned int)(verts.size() / 3), verts.data());
	m->SetFaces((unsigned int)(faces.size() / 3), 3, faces.data());
	return m;
}

TEST(TEST_cgmesh_metrics, bvh_closest_distance)
{
	Mesh *m = flat_grid(8, 0.f);
	BVH bvh;
	bvh.build(*m);

	Vector3f p;
	p.Set ( 0.5f, 0.5f, 0.7f); // above the plane
	EXPECT_NEAR(sqrtf(bvh.closest_distance2(p)), 0.7f, 1e-4f);

	Vector3f cl;
	p.Set ( 0.3f, 0.4f, -0.25f);
	float d = sqrtf(bvh.closest_distance2(p, &cl));
	EXPECT_NEAR(d, 0.25f, 1e-4f);
	EXPECT_NEAR(cl[2], 0.f, 1e-4f); // closest point lies on the z=0 plane

	delete m;
}

TEST(TEST_cgmesh_metrics, hausdorff_plane_vs_displaced)
{
	Mesh *a = flat_grid(8, 0.f);
	Mesh *b = flat_grid(8, 0.f);
	HausdorffResult same = mesh_hausdorff(*a, *b);
	EXPECT_NEAR(same.symmetric, 0.f, 1e-4f);

	Mesh *c = flat_grid(8, 0.3f); // same grid lifted by 0.3
	HausdorffResult disp = mesh_hausdorff(*a, *c);
	EXPECT_NEAR(disp.symmetric, 0.3f, 1e-4f);

	delete a;
	delete b;
	delete c;
}

TEST(TEST_cgmesh_metrics, hausdorff_flat_decimation_is_small)
{
	// Decimating a flat plane introduces (almost) no geometric error.
	Mesh_half_edge *he = new Mesh_half_edge();
	Mesh *ref = flat_grid(16, 0.f);
	he->m_pMesh->SetVertices(ref->m_nVertices, ref->m_pVertices.data());
	std::vector<unsigned int> tris = ref->GetTriangles();
	he->m_pMesh->SetFaces(ref->m_nFaces, 3, tris.data());
	he->create_half_edge();

	he->simplify(0.3f, {false}); // geometry-only, features off

	float rel = mesh_hausdorff_relative(*ref, *he->m_pMesh);
	printf("flat decimation relative Hausdorff : %g\n", rel);
	EXPECT_LT(rel, 1e-3f);

	delete ref;
	delete he;
}
