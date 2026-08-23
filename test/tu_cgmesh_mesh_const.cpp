#include <gtest/gtest.h>

#include <cstdio>
#include <vector>

#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_raycast.h"
#include "../src/cgmesh/octree.h"

// ============================================================================
//  Surface de LECTURE de Mesh : appelable depuis un const Mesh&
// ============================================================================
//
// Ce fichier fait porter la verification au COMPILATEUR : chacune des methodes
// ci-dessous est invoquee sur une reference constante, donc le seul fait que
// l'unite compile etablit qu'elle est declaree const. Une regression -- un
// `const` retire d'une declaration, ou une lecture qui se met a muter -- casse
// la construction, pas une assertion.
//
// Les quinze methodes propres a Mesh et les trois heritees de Geometry sont
// couvertes ici. Les valeurs verifiees restent volontairement minimales : la
// propriete testee est la QUALIFICATION, pas le calcul, chacune de ces methodes
// ayant deja ses propres tests ailleurs.

namespace {

// Un cube unite, tout-triangles : deux materiaux, de quoi exercer les chemins
// materiau (GetFaceMaterialId, GetMaterialId(nom)) sans quitter la geometrie la
// plus simple qui ait une aire et des aretes partagees.
void MakeCube (Mesh &m)
{
	m.Init (8, 12);

	const float v[24] = {
		0.f, 0.f, 0.f,
		1.f, 0.f, 0.f,
		1.f, 1.f, 0.f,
		0.f, 1.f, 0.f,
		0.f, 0.f, 1.f,
		1.f, 0.f, 1.f,
		1.f, 1.f, 1.f,
		0.f, 1.f, 1.f,
	};
	m.SetVertices (8, v);

	const unsigned int tris[36] = {
		0,2,1, 0,3,2,   // z-
		4,5,6, 4,6,7,   // z+
		0,1,5, 0,5,4,   // y-
		2,3,7, 2,7,6,   // y+
		1,2,6, 1,6,5,   // x+
		0,4,7, 0,7,3,   // x-
	};
	for (unsigned int i = 0; i < 12; i++)
		m.SetFace (i, tris[3*i], tris[3*i+1], tris[3*i+2]);

	MaterialColor *red = new MaterialColor (255, 0, 0);
	red->SetName ("rouge");
	m.Material_Add (red);
	MaterialColor *blue = new MaterialColor (0, 0, 255);
	blue->SetName ("bleu");
	m.Material_Add (blue);
	m.ApplyMaterial (0);

	m.ComputeNormals ();
	m.computebbox ();
}

// Les dix-huit appels, tous sur une reference CONSTANTE. C'est cette signature
// qui porte le critere : la remplacer par `Mesh&` viderait le fichier de son
// sens.
void ReadOnlySurface (const Mesh &m)
{
	// --- 15 methodes propres a Mesh ---
	m.Dump ();

	const std::vector<unsigned int> tris = m.GetTriangles ();
	EXPECT_EQ (tris.size (), 3u * 12u);

	const std::vector<unsigned int> tri2 = m.BuildTriangulation ();
	EXPECT_EQ (tri2, tris);

	const Mesh::PolygonRenderData rd = m.BuildPolygonRenderData ();
	EXPECT_EQ (rd.indices.size (), 3u * 12u);

	EXPECT_EQ (m.GetFaceMaterialId (0), 0);

	Vector3f bar;
	m.GetFaceBarycenter (0, bar);

	EXPECT_EQ (m.save ("const_surface.obj"), 0);
	EXPECT_EQ (m.export_stl_binary ("const_surface.stl"), 0);

	EXPECT_NEAR (m.GetFaceArea (0), 0.5f, 1e-5f);
	EXPECT_NEAR (m.GetArea (), 6.f, 1e-5f);

	float *areas = m.GetAreas ();
	ASSERT_NE (areas, nullptr);
	EXPECT_NEAR (areas[0], 0.5f, 1e-5f);
	free (areas);

	float *cumul = m.GetCumulativeAreas ();
	ASSERT_NE (cumul, nullptr);
	EXPECT_NEAR (cumul[11], 6.f, 1e-5f);
	free (cumul);

	int verticesinfaces[8];
	EXPECT_EQ (m.stats_vertices_in_faces (verticesinfaces, 8), 0);
	EXPECT_EQ (verticesinfaces[3], 12);

	EXPECT_EQ (m.GetMaterialId ("bleu"), 1);
	EXPECT_EQ (m.GetMaterialId ("inconnu"), -1);

	EXPECT_EQ (m.CountEdges (), 18u);

	// --- 3 methodes heritees de Geometry ---
	const Vector3f o (0.5f, 0.5f, 5.f);
	const Vector3f d (0.f, 0.f, -1.f);
	EXPECT_TRUE (m.GetIntersectionBboxWithRay (o, d));

	float t = 0.f;
	Vector3f hit, nrm;
	EXPECT_EQ (m.GetIntersectionWithRay (o, d, &t, hit, nrm), 1);
	EXPECT_EQ (m.GetIntersectionWithSegment (o, Vector3f (0.5f, 0.5f, -5.f), &t, hit, nrm), 0);
}

} // namespace

// Le maillage est bien vu comme constant, et la lecture ne le mute pas : la
// revision de geometrie est le temoin, puisque toute ecriture de Mesh l'incremente.
TEST (TEST_cgmesh_mesh_const, read_only_surface_is_callable_on_const_mesh)
{
	Mesh m;
	MakeCube (m);

	const uint64_t revisionBefore = m.GetRevision ();

	const Mesh &cm = m;
	ReadOnlySurface (cm);

	EXPECT_EQ (m.GetRevision (), revisionBefore);
	EXPECT_EQ (m.GetNVertices (), 8u);
	EXPECT_EQ (m.GetNFaces (), 12u);

	std::remove ("const_surface.obj");
	std::remove ("const_surface.mtl");
	std::remove ("const_surface.stl");
}

// Recolte de P11 : BuildRaycastOctree prend desormais un const Mesh&, ce que
// l'absence de const sur GetTriangles() interdisait.
TEST (TEST_cgmesh_mesh_const, raycast_octree_builds_from_const_mesh)
{
	Mesh m;
	MakeCube (m);

	const Mesh &cm = m;
	const std::unique_ptr<Octree> octree = BuildRaycastOctree (cm);
	ASSERT_NE (octree, nullptr);

	const Vector3f o (0.5f, 0.5f, 5.f);
	const Vector3f d (0.f, 0.f, -1.f);
	float t = 0.f;
	Vector3f hit, nrm;
	EXPECT_EQ (GetIntersectionWithRay (cm, *octree, o, d, &t, hit, nrm), 1);
	EXPECT_NEAR (hit[2], 1.f, 1e-4f);
}

// Un Geometry* qui designe un Mesh reste interrogeable a travers un pointeur
// CONSTANT : c'est ce que les trois virtuelles de Geometry viennent de gagner.
TEST (TEST_cgmesh_mesh_const, geometry_interface_is_const)
{
	Mesh m;
	MakeCube (m);

	const Geometry *g = &m;
	const Vector3f o (0.5f, 0.5f, 5.f);
	const Vector3f d (0.f, 0.f, -1.f);
	EXPECT_TRUE (g->GetIntersectionBboxWithRay (o, d));

	float t = 0.f;
	Vector3f hit, nrm;
	EXPECT_EQ (g->GetIntersectionWithRay (o, d, &t, hit, nrm), 1);

	// Les quatre autres sous-classes de Geometry, chacune interrogee a travers
	// une reference CONSTANTE : c'est la propagation de la virtuelle qui est
	// testee, la valeur rendue n'etant la que pour ancrer le comportement.
	Triangle triangle;
	triangle.Init (0.f, 0.f, 1.f,
	               2.f, 0.f, 1.f,
	               0.f, 2.f, 1.f);
	const Triangle &ctriangle = triangle;
	EXPECT_EQ (ctriangle.GetIntersectionWithRay (o, d, &t, hit, nrm), 1);

	Sphere sphere;
	sphere.SetCenter (0.5f, 0.5f, 0.f);
	const Sphere &csphere = sphere;
	EXPECT_EQ (csphere.GetIntersectionWithRay (o, d, &t, hit, nrm), 1);

	Plane plane (Vector3f (0.f, 0.f, 1.f), 0.f);
	const Plane &cplane = plane;
	EXPECT_EQ (cplane.GetIntersectionWithRay (o, d, &t, hit, nrm), 1);

	Torus torus;
	const Torus &ctorus = torus;
	ctorus.GetIntersectionWithSegment (o, Vector3f (0.5f, 0.5f, -5.f), &t, hit, nrm);
}
