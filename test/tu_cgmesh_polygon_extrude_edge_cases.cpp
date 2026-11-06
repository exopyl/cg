#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include <cgmath/geometry.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "../src/cgmesh/polygon2.h"
#include "../src/cgmesh/polygon_extrude.h"
#include "../src/cgmesh/profile_sweep.h"

// ===========================================================================
//  Cas limites des primitives de cgmesh employees par cgarchi
// ===========================================================================
//
// tessellateToMesh, extrudeToMesh, appendMesh (polygon_extrude.h) et
// sweepProfileAlongArc (profile_sweep.h). Les cas nominaux sont couverts par
// tu_cgarchi_architecture_gothic.cpp ; le validateur lui-meme par
// tu_cgmesh_mesh_validation.cpp. Ici : polygone vide, hauteur nulle, fermeture
// et orientation la ou elles sont promises, fusion de maillages.

namespace
{

// Carre unite, sens trigonometrique (contour exterieur).
Polygon2 UnitSquare (float x0 = 0.f)
{
	Polygon2 p;
	float pts[8] = {x0, 0.f, x0 + 1.f, 0.f, x0 + 1.f, 1.f, x0, 1.f};
	p.add_contour (0, 4, pts);
	return p;
}

MeshValidationReport Validate (const Mesh &m)
{
	MeshValidationOptions options;
	options.weldTolerance = 1e-6;
	return ValidateMesh (m, options);
}

} // namespace

TEST(TEST_cgmesh_polygon_extrude_edge_cases, empty_polygon_is_refused_and_out_is_unchanged)
{
	Polygon2 empty;
	Mesh out;
	Polygon2 square = UnitSquare ();
	tessellateToMesh (square, out);
	const unsigned nv = out.GetNVertices ();
	const unsigned nf = out.GetNFaces ();
	ASSERT_GT(nf, 0u);

	EXPECT_THROW(tessellateToMesh (empty, out), std::runtime_error);
	EXPECT_THROW(extrudeToMesh (empty, out, 0., 1.), std::runtime_error);
	EXPECT_EQ(out.GetNVertices (), nv);
	EXPECT_EQ(out.GetNFaces (), nf);
}

TEST(TEST_cgmesh_polygon_extrude_edge_cases, extruded_square_is_a_closed_outward_shell_of_its_volume)
{
	Polygon2 square = UnitSquare ();
	Mesh m;
	extrudeToMesh (square, m, -1., 2.);
	const MeshValidationReport r = Validate (m);
	EXPECT_TRUE(r.IsValid ());
	EXPECT_EQ(r.components, 1u);
	EXPECT_EQ(r.Count (MeshDefect::BoundaryEdge), 0u);
	EXPECT_EQ(r.Count (MeshDefect::InconsistentOrientation), 0u);
	EXPECT_EQ(r.Count (MeshDefect::InvertedShell), 0u);
	EXPECT_NEAR(r.signedVolume, 3.0, 1e-6);   // normales vers l'exterieur
	EXPECT_NEAR(r.bbMin[2], -1.0, 1e-6);
	EXPECT_NEAR(r.bbMax[2], 2.0, 1e-6);
}

// Hauteur nulle : pas de refus ; le maillage rendu est plat et ses defauts sont
// visibles du validateur (parois degenerees, capots confondus).
TEST(TEST_cgmesh_polygon_extrude_edge_cases, zero_height_extrusion_is_flat_and_flagged)
{
	Polygon2 square = UnitSquare ();
	Mesh cap;
	tessellateToMesh (square, cap);
	const unsigned capFaces = cap.GetNFaces ();

	Mesh m;
	ASSERT_NO_THROW(extrudeToMesh (square, m, 1., 1.));
	const MeshValidationReport r = Validate (m);
	EXPECT_FALSE(r.IsValid ());
	EXPECT_EQ(r.Count (MeshDefect::DegenerateFace), 2u * 4u);    // deux triangles par paroi
	EXPECT_EQ(r.Count (MeshDefect::DuplicateFace), capFaces);    // capot bas confondu avec le haut
	EXPECT_NEAR(r.signedVolume, 0.0, 1e-9);
	EXPECT_NEAR(r.bbMax[2] - r.bbMin[2], 0.0, 1e-9);
}

TEST(TEST_cgmesh_polygon_extrude_edge_cases, append_onto_an_empty_mesh_copies_and_appending_an_empty_mesh_is_a_no_op)
{
	Polygon2 square = UnitSquare ();
	Mesh solid;
	extrudeToMesh (square, solid, 0., 1.);

	Mesh dest;
	appendMesh (dest, solid);
	EXPECT_EQ(dest.GetNVertices (), solid.GetNVertices ());
	EXPECT_EQ(dest.GetNFaces (), solid.GetNFaces ());
	EXPECT_NEAR(Validate (dest).signedVolume, 1.0, 1e-6);

	const Mesh empty;
	appendMesh (dest, empty);
	EXPECT_EQ(dest.GetNVertices (), solid.GetNVertices ());
	EXPECT_EQ(dest.GetNFaces (), solid.GetNFaces ());
}

TEST(TEST_cgmesh_polygon_extrude_edge_cases, appended_faces_index_the_appended_vertices)
{
	Polygon2 left = UnitSquare (0.f);
	Polygon2 right = UnitSquare (3.f);
	Mesh a, b;
	extrudeToMesh (left, a, 0., 1.);
	extrudeToMesh (right, b, 0., 1.);
	appendMesh (a, b);

	MeshValidationOptions options;
	options.weldTolerance = 1e-6;
	options.expectedComponents = 2;
	const MeshValidationReport r = ValidateMesh (a, options);
	EXPECT_TRUE(r.IsValid ());
	EXPECT_EQ(r.components, 2u);
	EXPECT_NEAR(r.signedVolume, 2.0, 1e-6);
	EXPECT_NEAR(r.bbMin[0], 0.0, 1e-6);
	EXPECT_NEAR(r.bbMax[0], 4.0, 1e-6);
}

// Balayage le long d'un arc : tube ouvert sans capots, la fermeture n'est pas
// promise ; seules les deux sections d'extremite sont des bords.
TEST(TEST_cgmesh_polygon_extrude_edge_cases, swept_tube_is_open_only_at_its_two_ends)
{
	const Arc arc (Circle (Vector2d (0., 0.), 10.), 0., 1.5, true);
	const std::vector<Vector2d> profile = rectangularProfile ();
	Mesh m;
	sweepProfileAlongArc (arc, profile, 1., 1., m);
	const MeshValidationReport r = Validate (m);
	EXPECT_EQ(r.Count (MeshDefect::InvalidIndex), 0u);
	EXPECT_EQ(r.Count (MeshDefect::DegenerateFace), 0u);
	EXPECT_EQ(r.Count (MeshDefect::NonManifoldEdge), 0u);
	EXPECT_EQ(r.Count (MeshDefect::InconsistentOrientation), 0u);
	EXPECT_EQ(r.Count (MeshDefect::BoundaryEdge), 2u * profile.size ());
	EXPECT_EQ(r.components, 1u);
}
