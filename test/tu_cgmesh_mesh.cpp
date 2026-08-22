#include <gtest/gtest.h>

#include "../src/cgmesh/mesh.h"

#include <cmath>
#include <type_traits>
#include <vector>

// ============================================================================
//  Tests de CARACTERISATION de l'API de faces de Mesh
// ============================================================================
//
// Le filet de l'API de faces de Mesh, dont le stockage est destine a changer
// (passage a un stockage plat). Ailleurs, cette API n'est eprouvee
// qu'indirectement, a travers les algorithmes et les IO.
//
// CONTRAINTE QUI DECIDE DE L'UTILITE DE CE FICHIER :
//
//   Il ne passe que par des METHODES publiques, jamais par un membre.
//   Pas de m_pFaces, pas de m_pVertices, pas de GetFace() -> Face*.
//
// Un test qui manipulerait m_pFaces[i]->m_pVertices serait a reecrire avec le
// stockage et n'aurait donc rien protege. Ici, un changement de stockage doit
// passer sans qu'une seule ligne ne change ; c'est exactement ce qui le valide.
//
// La contrainte est tenue par le COMPILATEUR : le stockage des faces est prive et
// il n'existe aucun accesseur qui en rende un pointeur.
//
// Corollaire : ne rien affirmer sur l'identite ou l'adresse d'une Face, type
// destine a disparaitre.
//
// « Caracterisation » et non « specification » : plusieurs assertions ci-dessous
// figent un comportement DISCUTABLE (cf. GetFaceArea). Le but n'est pas de dire
// ce qui devrait etre, mais de detecter tout changement involontaire.
//
// ============================================================================

namespace {

// ---------------------------------------------------------------------------
//  Fixture : un maillage MIXTE, triangles + quads, dont un quad CONCAVE
// ---------------------------------------------------------------------------
//
// Le cas mixte est celui que la conservation des n-gons rend critique (decision
// R2) : un maillage tout-triangles ne dirait rien des chemins qui comptent.
//
// Le quad concave n'est pas une coquetterie : la triangulation de cgmesh route
// les faces convexes vers un eventail et les concaves vers glutess
// (cf. forEachFaceTriangle, mesh.cpp). Sans lui, la moitie du code de
// triangulation resterait hors du filet. Un quad suffit a etre concave -- pas
// besoin d'un pentagone, ce qui permet de tout construire avec les seules
// surcharges publiques SetFace().
//
//   Deux triangles + un carre :        Le dard (concave en D) :
//
//     3---4---5                          C
//     | \ |   |                         / \
//     |  \|   |                        / D \        D est RENTRE dans ABC,
//     0---1---2                       A-----B       donc l'angle en D est reflexe
//
// Sommets 0..5 pour la partie gauche, 6..9 pour le dard.
//
// Remplissage par REFERENCE : `return m;` compilerait, mais en copiant.
void MakeMixedMesh (Mesh &m)
{
	m.Init (10, 4);

	float v[30] = {
		0.f, 0.f, 0.f,   // 0
		1.f, 0.f, 0.f,   // 1
		2.f, 0.f, 0.f,   // 2
		0.f, 1.f, 0.f,   // 3
		1.f, 1.f, 0.f,   // 4
		2.f, 1.f, 0.f,   // 5

		3.0f,  0.00f, 0.f,   // 6  A
		4.0f,  0.00f, 0.f,   // 7  B
		3.5f,  1.00f, 0.f,   // 8  C
		3.5f,  0.25f, 0.f,   // 9  D -- a l'interieur de ABC
	};
	m.SetVertices (10, v);

	m.SetFace (0, 0, 1, 3);            // triangle, aire 0.5
	m.SetFace (1, 1, 4, 3);            // triangle, aire 0.5
	m.SetFace (2, 1, 2, 5, 4);         // quad convexe, carre unite
	m.SetFace (3, 6, 7, 8, 9);         // quad CONCAVE (dard), reflexe en 9
}

// Un maillage entierement triangulaire, pour les cas ou le mixte n'apporte rien.
void MakeTriangleMesh (Mesh &m)
{
	m.Init (4, 2);

	float v[12] = {
		0.f, 0.f, 0.f,
		1.f, 0.f, 0.f,
		1.f, 1.f, 0.f,
		0.f, 1.f, 0.f,
	};
	m.SetVertices (4, v);

	m.SetFace (0, 0, 1, 2);
	m.SetFace (1, 0, 2, 3);
}

} // namespace

// ============================================================================
//  Semantique de copie, figee par des assertions statiques
// ============================================================================
//
// Mesh est copiable, et sans aucune copie ecrite a la main : chaque membre est un
// type valeur. C'est ce qui garantit qu'un membre ajoute demain sera copie sans
// que personne n'y pense.
//
// Les trois traits sont necessaires : ils ferment trois portes distinctes.

static_assert (std::is_copy_constructible_v<Mesh>,
               "Mesh est copiable depuis la phase 4.");

static_assert (std::is_constructible_v<Mesh, Mesh &>,
               "... y compris depuis une lvalue non const.");

static_assert (std::is_copy_assignable_v<Mesh>,
               "Mesh est assignable depuis la phase 4.");

// ============================================================================
//  Topologie
// ============================================================================

TEST (TEST_cgmesh_mesh, fixture_reports_mixed_arity)
{
	Mesh m;
	MakeMixedMesh (m);

	EXPECT_EQ (m.GetNVertices (), 10u);
	EXPECT_EQ (m.GetNFaces (),     4u);

	EXPECT_EQ (m.GetFaceNVertices (0), 3);
	EXPECT_EQ (m.GetFaceNVertices (1), 3);
	EXPECT_EQ (m.GetFaceNVertices (2), 4);
	EXPECT_EQ (m.GetFaceNVertices (3), 4);

	EXPECT_FALSE (m.IsTriangleMesh ()) << "deux quads sont presents";
}

TEST (TEST_cgmesh_mesh, face_vertices_roundtrip)
{
	Mesh m;
	MakeMixedMesh (m);

	EXPECT_EQ (m.GetFaceVertex (0, 0), 0);
	EXPECT_EQ (m.GetFaceVertex (0, 1), 1);
	EXPECT_EQ (m.GetFaceVertex (0, 2), 3);

	EXPECT_EQ (m.GetFaceVertex (2, 0), 1);
	EXPECT_EQ (m.GetFaceVertex (2, 1), 2);
	EXPECT_EQ (m.GetFaceVertex (2, 2), 5);
	EXPECT_EQ (m.GetFaceVertex (2, 3), 4);
}

TEST (TEST_cgmesh_mesh, set_face_changes_arity_in_place)
{
	Mesh m;
	MakeTriangleMesh (m);
	ASSERT_TRUE (m.IsTriangleMesh ());

	// Reecrire une face en quad doit changer son arite -- c'est le chemin par
	// lequel un maillage devient mixte.
	m.SetFace (0, 0, 1, 2, 3);

	EXPECT_EQ (m.GetFaceNVertices (0), 4);
	EXPECT_EQ (m.GetFaceVertex (0, 3), 3);
	EXPECT_FALSE (m.IsTriangleMesh ());
}

TEST (TEST_cgmesh_mesh, is_triangle_mesh_true_when_all_triangles)
{
	Mesh m;
	MakeTriangleMesh (m);
	EXPECT_TRUE (m.IsTriangleMesh ());
}

TEST (TEST_cgmesh_mesh, count_edges_on_mixed_mesh)
{
	Mesh m;
	MakeMixedMesh (m);

	// Aretes distinctes, non orientees :
	//   F0 (0,1,3)     : 0-1, 1-3, 0-3
	//   F1 (1,4,3)     : 1-4, 3-4, 1-3   (1-3 partagee avec F0)
	//   F2 (1,2,5,4)   : 1-2, 2-5, 4-5, 1-4 (1-4 partagee avec F1)
	//   F3 (6,7,8,9)   : 6-7, 7-8, 8-9, 6-9
	// soit 3 + 2 + 3 + 4 = 12
	EXPECT_EQ (m.CountEdges (), 12u);
}

TEST (TEST_cgmesh_mesh, flip_faces_reverses_vertex_order)
{
	Mesh m;
	MakeMixedMesh (m);
	m.FlipFaces ();

	// Le triangle (0,1,3) devient (3,1,0) : premier et dernier echanges.
	EXPECT_EQ (m.GetFaceVertex (0, 0), 3);
	EXPECT_EQ (m.GetFaceVertex (0, 2), 0);

	// Le quad (1,2,5,4) devient (4,5,2,1).
	EXPECT_EQ (m.GetFaceVertex (2, 0), 4);
	EXPECT_EQ (m.GetFaceVertex (2, 1), 5);
	EXPECT_EQ (m.GetFaceVertex (2, 2), 2);
	EXPECT_EQ (m.GetFaceVertex (2, 3), 1);
}

// ============================================================================
//  Attributs par face
// ============================================================================

TEST (TEST_cgmesh_mesh, face_material_id_roundtrip)
{
	Mesh m;
	MakeMixedMesh (m);

	m.SetFaceMaterialId (0, 7);
	m.SetFaceMaterialId (3, 2);

	EXPECT_EQ (m.GetFaceMaterialId (0), 7);
	EXPECT_EQ (m.GetFaceMaterialId (3), 2);

	// Une face non estampillee garde sa valeur d'origine, distincte de celles
	// que l'on vient de poser.
	EXPECT_NE (m.GetFaceMaterialId (1), 7);
}

// ============================================================================
//  Mesures
// ============================================================================

TEST (TEST_cgmesh_mesh, get_face_area_on_triangles)
{
	Mesh m;
	MakeMixedMesh (m);

	EXPECT_NEAR (m.GetFaceArea (0), 0.5f, 1e-5f);
	EXPECT_NEAR (m.GetFaceArea (1), 0.5f, 1e-5f);
}

TEST (TEST_cgmesh_mesh, get_face_area_of_a_quad_uses_only_its_first_triangle)
{
	Mesh m;
	MakeMixedMesh (m);

	// CARACTERISATION D'UN COMPORTEMENT DISCUTABLE, fige ici pour qu'un
	// changement ne passe pas inapercu : GetFaceArea ne lit que les trois
	// premiers sommets de la face (mesh.cpp:1056-1068). Sur le carre unite
	// (1,2,5,4) elle rend donc 0.5 -- l'aire du triangle (1,2,5) -- et non 1.0.
	//
	// Ce n'est PAS une specification : c'est ce que le code fait. Si ce point est
	// corrige, ce test doit changer sciemment.
	EXPECT_NEAR (m.GetFaceArea (2), 0.5f, 1e-5f)
	    << "moitie du carre unite : seuls les 3 premiers sommets sont lus";
}

TEST (TEST_cgmesh_mesh, computebbox_covers_every_vertex)
{
	Mesh m;
	MakeMixedMesh (m);
	m.computebbox ();

	float vmin[3], vmax[3];
	m.bbox ().GetMinMax (vmin, vmax);

	EXPECT_NEAR (vmin[0], 0.f, 1e-5f);
	EXPECT_NEAR (vmax[0], 4.f, 1e-5f);
	EXPECT_NEAR (vmin[1], 0.f, 1e-5f);
	EXPECT_NEAR (vmax[1], 1.f, 1e-5f);
}

// ============================================================================
//  Triangulation -- les chemins que le CSR devra preserver
// ============================================================================

TEST (TEST_cgmesh_mesh, build_triangulation_covers_every_face)
{
	Mesh m;
	MakeMixedMesh (m);

	// Somme de (N-2) triangles par face : 1 + 1 + 2 + 2 = 6, donc 18 indices.
	// Le quad concave passe par glutess, le convexe par un eventail : les deux
	// doivent rendre deux triangles.
	std::vector<unsigned int> tris = m.BuildTriangulation ();
	EXPECT_EQ (tris.size (), 18u);

	for (unsigned int i : tris)
		EXPECT_LT (i, m.GetNVertices ()) << "aucun indice ne sort du maillage";
}

TEST (TEST_cgmesh_mesh, get_triangles_matches_build_triangulation)
{
	Mesh m;
	MakeMixedMesh (m);

	// GetTriangles delegue a BuildTriangulation (mesh.cpp) : la duplication
	// d'API doit rester sans divergence de comportement.
	EXPECT_EQ (m.GetTriangles (), m.BuildTriangulation ());
}

TEST (TEST_cgmesh_mesh, triangulate_makes_the_mesh_triangular)
{
	Mesh m;
	MakeMixedMesh (m);
	ASSERT_FALSE (m.IsTriangleMesh ());

	m.Triangulate ();

	EXPECT_TRUE (m.IsTriangleMesh ());
	EXPECT_EQ (m.GetNFaces (), 6u) << "2 triangles + 2 quads -> 2 + 4";

	for (unsigned int f = 0; f < m.GetNFaces (); ++f)
		EXPECT_EQ (m.GetFaceNVertices (f), 3);
}

TEST (TEST_cgmesh_mesh, triangulate_is_idempotent)
{
	Mesh m;
	MakeMixedMesh (m);
	m.Triangulate ();
	const unsigned int n = m.GetNFaces ();

	m.Triangulate ();   // annonce sans effet sur un maillage deja triangulaire

	EXPECT_EQ (m.GetNFaces (), n);
}

TEST (TEST_cgmesh_mesh, triangulate_preserves_face_material)
{
	Mesh m;
	MakeMixedMesh (m);
	m.SetFaceMaterialId (2, 5);   // le quad convexe

	m.Triangulate ();

	// Les sous-triangles issus d'une face heritent de son materiau. On ne peut
	// pas presumer de leur position dans le tableau : on verifie qu'au moins
	// deux faces le portent, soit les deux moities du quad.
	unsigned int tagged = 0;
	for (unsigned int f = 0; f < m.GetNFaces (); ++f)
		if (m.GetFaceMaterialId (f) == 5) ++tagged;

	EXPECT_EQ (tagged, 2u);
}

// ============================================================================
//  Chemin de rendu
// ============================================================================

TEST (TEST_cgmesh_mesh, build_polygon_render_data_on_a_mixed_mesh)
{
	Mesh m;
	MakeMixedMesh (m);
	m.ComputeNormals ();

	Mesh::PolygonRenderData rd = m.BuildPolygonRenderData ();

	ASSERT_FALSE (rd.indices.empty ());
	EXPECT_EQ (rd.indices.size () % 3, 0u) << "des triangles, donc un multiple de 3";

	// Les n-gons ajoutent des sommets d'expansion pour porter la normale de
	// polygone : le rendu en compte donc PLUS que la topologie (mesh.h:178-187).
	const std::size_t renderVertices = rd.positions.size () / 3;
	EXPECT_GT (renderVertices, (std::size_t)m.GetNVertices ())
	    << "deux quads doivent produire des slots d'expansion";

	EXPECT_EQ (rd.normals.size (), rd.positions.size ());

	for (unsigned int i : rd.indices)
		EXPECT_LT ((std::size_t)i, renderVertices);
}

TEST (TEST_cgmesh_mesh, render_data_of_a_triangle_mesh_has_no_expansion)
{
	Mesh m;
	MakeTriangleMesh (m);
	m.ComputeNormals ();

	Mesh::PolygonRenderData rd = m.BuildPolygonRenderData ();

	// « A mesh of pure triangles emits exactly m_nVertices render-vertices »
	// (mesh.h:186) : aucun cout de duplication quand il n'y a pas de n-gon.
	EXPECT_EQ (rd.positions.size () / 3, (std::size_t)m.GetNVertices ());
}

// ============================================================================
//  Revision
// ============================================================================
//
// ⚠ Le compteur est incremente A LA MAIN, par une vingtaine de sites, alors que
// bien plus de fichiers ecrivent dans la geometrie : il n'est PAS digne de foi, et
// aucun cache indexe dessus ne l'est non plus. Ces tests figent les increments qui
// fonctionnent, pour qu'un passage a l'increment systematique ne les casse pas.

TEST (TEST_cgmesh_mesh, revision_changes_when_the_geometry_is_triangulated)
{
	Mesh m;
	MakeMixedMesh (m);
	const uint64_t before = m.GetRevision ();

	m.Triangulate ();

	EXPECT_NE (m.GetRevision (), before)
	    << "Triangulate remplace les faces : la revision doit bouger";
}

TEST (TEST_cgmesh_mesh, tensors_are_invalid_on_a_fresh_mesh)
{
	Mesh m;
	MakeMixedMesh (m);
	EXPECT_FALSE (m.AreTensorsValid ()) << "aucun tenseur n'a ete calcule";
}

// ============================================================================
//  Attributs par sommet et changement de compte
// ============================================================================
//
// Regle : un attribut indexe par sommet (couleurs, normales) reste valide quand
// les POSITIONS changent, et devient invalide quand le NOMBRE de sommets change.
// Ces tests fixent les trois operations qui changent ce nombre.

namespace {

void MakeColoredQuadMesh (Mesh &m)
{
	m.Init (4, 1);
	float v[12] = { 0.f,0.f,0.f,  1.f,0.f,0.f,  1.f,1.f,0.f,  0.f,1.f,0.f };
	m.SetVertices (4, v);
	m.SetFace (0, 0, 1, 2, 3);
	m.InitVertexColors (0.25f, 0.5f, 0.75f);
}

} // namespace

TEST (TEST_cgmesh_mesh, set_vertices_keeps_colors_when_the_count_is_unchanged)
{
	Mesh m;
	MakeColoredQuadMesh (m);
	ASSERT_EQ (m.GetVertexColors ().size (), 3u * m.GetNVertices ());

	// Memes sommets, deplaces : les couleurs restent valides.
	float moved[12] = { 5.f,0.f,0.f,  6.f,0.f,0.f,  6.f,1.f,0.f,  5.f,1.f,0.f };
	m.SetVertices (4, moved);

	ASSERT_EQ (m.GetVertexColors ().size (), 3u * m.GetNVertices ());
	EXPECT_FLOAT_EQ (m.GetVertexColors ()[0], 0.25f);
}

TEST (TEST_cgmesh_mesh, set_vertices_drops_colors_when_the_count_changes)
{
	Mesh m;
	MakeColoredQuadMesh (m);

	float three[9] = { 0.f,0.f,0.f,  1.f,0.f,0.f,  1.f,1.f,0.f };
	m.SetVertices (3, three);

	EXPECT_EQ (m.GetNVertices (), 3u);
	EXPECT_TRUE (m.GetVertexColors ().empty ())
		<< "des couleurs pour 4 sommets sur un maillage de 3 feraient lire hors bornes";
	EXPECT_TRUE (m.GetVertexNormals ().empty ());
}

// DeleteVertices compacte les attributs en place. Sans redimensionnement, leur
// taille resterait celle d'avant, et toute operation qui teste le parallelisme a
// l'egalite exacte les abandonnerait en silence.
TEST (TEST_cgmesh_mesh, delete_vertices_keeps_the_attributes_vertex_parallel)
{
	Mesh m;
	m.Init (4, 0);
	float v[12] = { 0.f,0.f,0.f,  1.f,0.f,0.f,  2.f,0.f,0.f,  3.f,0.f,0.f };
	m.SetVertices (4, v);
	m.InitVertexColors (1.f, 0.f, 0.f);
	ASSERT_EQ (m.GetVertexColors ().size (), 12u);

	// Supprime les sommets dont x >= 2 : il en reste deux.
	m.DeleteVertices ([](float x, float, float) -> int { return x >= 2.f ? 1 : 0; });

	ASSERT_EQ (m.GetNVertices (), 2u);
	EXPECT_EQ (m.GetVertexColors ().size (), 3u * m.GetNVertices ());
	EXPECT_EQ (m.GetVertexNormals ().size (), 3u * m.GetNVertices ());
}

TEST (TEST_cgmesh_mesh, append_drops_the_per_vertex_attributes)
{
	Mesh a;
	MakeColoredQuadMesh (a);
	Mesh b;
	MakeColoredQuadMesh (b);

	ASSERT_EQ (a.Append (&b), 1);

	EXPECT_EQ (a.GetNVertices (), 8u);
	EXPECT_TRUE (a.GetVertexColors ().empty ())
		<< "les couleurs ne couvraient que les 4 premiers sommets";
}

// ============================================================================
//  Mesh::FaceRef / Mesh::ConstFaceRef
// ============================================================================
//
// Le contrat que le passage a un stockage plat devra continuer a honorer : une
// reference rend ce qui a ete ecrit, et hors bornes ou sur un trou elle rend une
// reference invalide au lieu d'un comportement indefini.
//
// Ces tests n'affirment RIEN sur un Face* -- conformement a l'en-tete de ce
// fichier. Deux references sur la meme face ne sont pas comparees entre elles :
// ce serait une tautologie. On compare aux VALEURS posees.

TEST (TEST_cgmesh_mesh_faceref, reads_return_what_was_written)
{
    Mesh m;
    m.Init (4, 2);
    m.SetFace (0, 0, 1, 2);
    m.SetFace (1, 0, 2, 3);
    m.FaceAt (1)->SetMaterialId (7);

    ASSERT_TRUE (m.FaceAt (0).IsValid ());
    ASSERT_TRUE (m.FaceAt (1).IsValid ());

    EXPECT_EQ (m.FaceAt (0)->GetNVertices (), 3);
    EXPECT_EQ (m.FaceAt (0)->GetVertex (0), 0);
    EXPECT_EQ (m.FaceAt (0)->GetVertex (1), 1);
    EXPECT_EQ (m.FaceAt (0)->GetVertex (2), 2);
    EXPECT_EQ (m.FaceAt (0)->GetMaterialId (), (int)MATERIAL_NONE);

    EXPECT_EQ (m.FaceAt (1)->GetVertex (2), 3);
    EXPECT_EQ (m.FaceAt (1)->GetMaterialId (), 7);
    EXPECT_FALSE (m.FaceAt (1)->UsesTextureCoordinates ());

    // Deux references sur la meme face voient le meme etat : ecrire par l'une se
    // lit par l'autre.
    auto a = m.FaceAt (0);
    auto b = m.FaceAt (0);
    a->SetMaterialId (11);
    EXPECT_EQ (b->GetMaterialId (), 11);
}

TEST (TEST_cgmesh_mesh_faceref, writes_reach_the_stored_face)
{
    Mesh m;
    m.Init (4, 1);
    m.SetFace (0, 0, 1, 2);

    auto r = m.FaceAt (0);
    r->SetVertex (1, 3u);
    r->SetMaterialId (5);
    r->SetUsesTextureCoordinates (true);

    // Relecture par une reference NEUVE : la valeur est bien dans le maillage,
    // pas dans la reference.
    EXPECT_EQ (m.FaceAt (0)->GetVertex (1), 3);
    EXPECT_EQ (m.FaceAt (0)->GetMaterialId (), 5);
    EXPECT_TRUE (m.FaceAt (0)->UsesTextureCoordinates ());

    r->Flip ();
    EXPECT_EQ (m.FaceAt (0)->GetVertex (0), 2);
    EXPECT_EQ (m.FaceAt (0)->GetVertex (2), 0);

    r->SetTriangle (3u, 2u, 1u);
    EXPECT_EQ (m.FaceAt (0)->GetVertex (0), 3);
    EXPECT_EQ (m.FaceAt (0)->GetVertex (2), 1);
}

// Le TROU : Mesh::RemoveFace supprime une face sans changer le compte. La
// reference doit devenir invalide, et ses accesseurs rester inoffensifs -- c'est
// ce que plusieurs appelants testaient en indexant m_pFaces en direct.
TEST (TEST_cgmesh_mesh_faceref, a_removed_face_leaves_an_invalid_reference)
{
    Mesh m;
    m.Init (4, 2);
    m.SetFace (0, 0, 1, 2);
    m.SetFace (1, 0, 2, 3);

    m.RemoveFace (0);

    EXPECT_EQ (m.GetNFaces (), 2u) << "un trou ne change pas le compte";
    EXPECT_FALSE (m.FaceAt (0).IsValid ());
    EXPECT_TRUE  (m.FaceAt (1).IsValid ());

    // Aucun de ces appels ne doit dereferencer le trou.
    EXPECT_EQ (m.FaceAt (0)->GetNVertices (), 0);
    EXPECT_EQ (m.FaceAt (0)->GetVertex (0), -1);
    EXPECT_EQ (m.FaceAt (0)->GetMaterialId (), (int)MATERIAL_NONE);
    m.FaceAt (0)->SetMaterialId (3);
    m.FaceAt (0)->Flip ();

    // Hors bornes reste distinct de rien : les deux rendent une reference invalide.
    EXPECT_FALSE (m.FaceAt (2).IsValid ());
}

TEST (TEST_cgmesh_mesh_faceref, texture_coordinates_round_trip)
{
    Mesh m;
    m.Init (3, 1);
    m.SetFace (0, 0, 1, 2);

    auto r = m.FaceAt (0);
    // Les UV sont OPTIONNELS : un maillage neuf n'en porte aucun. Le detail est
    // couvert par uv_arrays_are_optional_and_appear_on_first_write ; ici, juste
    // l'aller-retour apres activation.
    EXPECT_FALSE (r->HasTexCoordIndices ());
    EXPECT_FALSE (r->HasCornerTexCoords ());
    EXPECT_FALSE (r->UsesTextureCoordinates ());

    r->ActivateTextureCoordinatesIndices ();
    r->ActivateTextureCoordinates ();
    EXPECT_TRUE (r->HasTexCoordIndices ());
    EXPECT_TRUE (r->HasCornerTexCoords ());

    r->SetTexCoord (2u, 42u);
    EXPECT_EQ (r->GetTexCoordIndex (2), 42);

    r->SetTexCoord (1u, 0.25f, 0.75f);
    float uv[2] = { -1.f, -1.f };
    ASSERT_EQ (r->GetTexCoord (1, uv), 0);
    EXPECT_FLOAT_EQ (uv[0], 0.25f);
    EXPECT_FLOAT_EQ (uv[1], 0.75f);
}

TEST (TEST_cgmesh_mesh_faceref, out_of_range_yields_an_invalid_reference)
{
    Mesh m;
    m.Init (3, 1);
    m.SetFace (0, 0, 1, 2);

    auto bad = m.FaceAt (1);
    EXPECT_FALSE (bad.IsValid ());
    EXPECT_FALSE ((bool)bad);
    // Aucun de ces appels ne doit dereferencer quoi que ce soit.
    EXPECT_EQ (bad->GetNVertices (), 0);
    EXPECT_EQ (bad->GetVertex (0), -1);
    EXPECT_EQ (bad->GetTexCoordIndex (0), -1);
    EXPECT_FALSE (bad->UsesTextureCoordinates ());
    EXPECT_EQ (bad->SetVertex (0, 0u), -1);
    bad->SetMaterialId (3);   // sans effet, sans plantage
    bad->Flip ();

    auto ok = m.FaceAt (0);
    EXPECT_TRUE (ok.IsValid ());
    EXPECT_EQ (ok.GetIndex (), 0u);

    // Une reference construite par defaut est invalide.
    Mesh::FaceRef empty;
    EXPECT_FALSE (empty.IsValid ());
}

TEST (TEST_cgmesh_mesh_faceref, a_const_mesh_yields_a_read_only_reference)
{
    Mesh m;
    m.Init (3, 1);
    m.SetFace (0, 0, 1, 2);
    m.FaceAt (0)->SetMaterialId (4);

    const Mesh &cm = m;
    auto r = cm.FaceAt (0);
    static_assert (std::is_same<decltype(r), Mesh::ConstFaceRef>::value,
                   "un Mesh const doit rendre une ConstFaceRef");
    EXPECT_EQ (r->GetNVertices (), 3);
    EXPECT_EQ (r->GetVertex (0), 0);
    EXPECT_EQ (r->GetMaterialId (), 4);

    // Une FaceRef se degrade en ConstFaceRef (heritage public).
    Mesh::ConstFaceRef c = m.FaceAt (0);
    EXPECT_EQ (c->GetMaterialId (), 4);
}

// Le contrat des UV : un maillage neuf n'en porte AUCUN, ils apparaissent a la
// premiere ecriture, et ils valent alors pour tout le maillage. Le drapeau
// UsesTextureCoordinates, lui, reste par face.
TEST (TEST_cgmesh_mesh_faceref, uv_arrays_are_optional_and_appear_on_first_write)
{
    Mesh m;
    m.Init (4, 2);
    m.SetFace (0, 0, 1, 2);
    m.SetFace (1, 0, 2, 3);

    // Rien n'a ete ecrit : aucun tableau d'UV.
    EXPECT_FALSE (m.FaceAt (0)->HasTexCoordIndices ());
    EXPECT_FALSE (m.FaceAt (0)->HasCornerTexCoords ());
    EXPECT_FALSE (m.FaceAt (0)->UsesTextureCoordinates ());
    EXPECT_EQ (m.FaceAt (0)->GetTexCoordIndex (0), -1);
    float uv[2] = { -1.f, -1.f };
    EXPECT_EQ (m.FaceAt (0)->GetTexCoord (0, uv), -1);

    // Une ecriture d'indice alloue les indices, et EUX SEULS.
    m.FaceAt (0)->SetTexCoord (1u, 42u);
    EXPECT_TRUE  (m.FaceAt (0)->HasTexCoordIndices ());
    EXPECT_FALSE (m.FaceAt (0)->HasCornerTexCoords ());
    EXPECT_EQ (m.FaceAt (0)->GetTexCoordIndex (1), 42);

    // La presence est une propriete du MAILLAGE : la face 1 en beneficie aussi,
    // avec une valeur par defaut de zero.
    EXPECT_TRUE (m.FaceAt (1)->HasTexCoordIndices ());
    EXPECT_EQ (m.FaceAt (1)->GetTexCoordIndex (0), 0);

    // Une ecriture de valeur alloue les valeurs.
    m.FaceAt (1)->SetTexCoord (2u, 0.25f, 0.75f);
    EXPECT_TRUE (m.FaceAt (1)->HasCornerTexCoords ());
    ASSERT_EQ (m.FaceAt (1)->GetTexCoord (2, uv), 0);
    EXPECT_FLOAT_EQ (uv[0], 0.25f);
    EXPECT_FLOAT_EQ (uv[1], 0.75f);

    // Le drapeau, lui, reste PAR FACE.
    m.FaceAt (0)->SetUsesTextureCoordinates (true);
    EXPECT_TRUE  (m.FaceAt (0)->UsesTextureCoordinates ());
    EXPECT_FALSE (m.FaceAt (1)->UsesTextureCoordinates ());
}

// Append concatene les deux pools. Ce qui doit voyager : les indices decales du
// nombre de sommets, l'arite, le materiau, le drapeau et les UV.
TEST (TEST_cgmesh_mesh_faceref, append_carries_everything_and_shifts_the_indices)
{
    Mesh a;
    a.Init (3, 1);
    float va[9] = { 0,0,0,  1,0,0,  0,1,0 };
    a.SetVertices (3, va);
    a.SetFace (0, 0, 1, 2);
    a.FaceAt (0)->SetMaterialId (0);

    Mesh b;
    b.Init (4, 1);
    float vb[12] = { 5,0,0,  6,0,0,  6,1,0,  5,1,0 };
    b.SetVertices (4, vb);
    b.SetFace (0, 0, 1, 2, 3);          // un QUAD : le resultat devient mixte
    b.FaceAt (0)->SetUsesTextureCoordinates (true);
    b.FaceAt (0)->SetTexCoord (0u, 7u);
    b.FaceAt (0)->SetTexCoord (3u, 0.5f, 0.5f);

    ASSERT_EQ (a.Append (&b), 1);

    EXPECT_EQ (a.GetNVertices (), 7u);
    EXPECT_EQ (a.GetNFaces (),    2u);
    EXPECT_FALSE (a.IsTriangleMesh ()) << "un triangle et un quad";

    // Face 0 : inchangee.
    EXPECT_EQ (a.FaceAt (0)->GetNVertices (), 3);
    EXPECT_EQ (a.FaceAt (0)->GetVertex (0), 0);
    EXPECT_FALSE (a.FaceAt (0)->UsesTextureCoordinates ());

    // Face 1 : indices DECALES de 3, arite 4, drapeau et UV transportes.
    EXPECT_EQ (a.FaceAt (1)->GetNVertices (), 4);
    EXPECT_EQ (a.FaceAt (1)->GetVertex (0), 3);
    EXPECT_EQ (a.FaceAt (1)->GetVertex (3), 6);
    EXPECT_TRUE (a.FaceAt (1)->UsesTextureCoordinates ());
    EXPECT_EQ (a.FaceAt (1)->GetTexCoordIndex (0), 7);
    float uv[2] = { -1.f, -1.f };
    ASSERT_EQ (a.FaceAt (1)->GetTexCoord (3, uv), 0);
    EXPECT_FLOAT_EQ (uv[0], 0.5f);
    EXPECT_FLOAT_EQ (uv[1], 0.5f);

    // Le materiau de la face 0 ne doit PAS avoir ete decale, et MATERIAL_NONE
    // doit rester MATERIAL_NONE plutot que de deborder vers un indice valide.
    EXPECT_EQ (a.FaceAt (0)->GetMaterialId (), 0);
    EXPECT_EQ (a.FaceAt (1)->GetMaterialId (), (int)MATERIAL_NONE);
}

// L'arite uniforme est un DESCRIPTEUR derivable, pas un second stockage : elle
// doit suivre les operations qui la changent, et seulement elles.
TEST (TEST_cgmesh_mesh_faceref, the_arity_descriptor_follows_the_edits)
{
    Mesh m;
    m.Init (6, 3);
    float v[18] = { 0,0,0, 1,0,0, 1,1,0, 0,1,0, 2,0,0, 2,1,0 };
    m.SetVertices (6, v);
    m.SetFace (0, 0, 1, 2);
    m.SetFace (1, 0, 2, 3);
    m.SetFace (2, 1, 4, 5);
    EXPECT_TRUE (m.IsTriangleMesh ()) << "arite uniforme a 3";

    // Une face devient un quad : le maillage devient mixte.
    m.SetFace (1, 0, 1, 2, 3);
    EXPECT_FALSE (m.IsTriangleMesh ());
    EXPECT_EQ (m.GetFaceNVertices (0), 3);
    EXPECT_EQ (m.GetFaceNVertices (1), 4);
    EXPECT_EQ (m.GetFaceNVertices (2), 3);
    EXPECT_EQ (m.FaceAt (1)->GetVertex (3), 3) << "la tranche agrandie garde ses indices";

    // Les faces voisines ne doivent PAS avoir bouge : c'est ce que garantit
    // l'ajout d'une tranche en fin de pool plutot qu'un decalage.
    EXPECT_EQ (m.FaceAt (0)->GetVertex (2), 2);
    EXPECT_EQ (m.FaceAt (2)->GetVertex (1), 4);

    // Triangulate ramene l'arite a 3, donc IsTriangleMesh redevient O(1) et vrai.
    m.Triangulate ();
    EXPECT_TRUE (m.IsTriangleMesh ());
    EXPECT_EQ (m.GetNFaces (), 4u) << "le quad donne deux triangles";
}

// Retrecir puis regrandir une face ne doit pas corrompre ses voisines.
TEST (TEST_cgmesh_mesh_faceref, shrinking_then_growing_a_face_leaves_neighbours_intact)
{
    Mesh m;
    m.Init (5, 2);
    m.SetFace (0, 0, 1, 2);
    m.SetFace (1, 2, 3, 4);

    m.FaceAt (0)->SetNVertices (2);
    EXPECT_EQ (m.FaceAt (0)->GetNVertices (), 2);
    EXPECT_EQ (m.FaceAt (0)->GetVertex (0), 0);
    EXPECT_EQ (m.FaceAt (0)->GetVertex (2), -1) << "hors arite";
    EXPECT_EQ (m.FaceAt (1)->GetVertex (0), 2) << "la voisine est intacte";

    m.FaceAt (0)->SetNVertices (5);
    EXPECT_EQ (m.FaceAt (0)->GetNVertices (), 5);
    EXPECT_EQ (m.FaceAt (0)->GetVertex (0), 0) << "les coins conserves sont recopies";
    EXPECT_EQ (m.FaceAt (0)->GetVertex (1), 1);
    EXPECT_EQ (m.FaceAt (1)->GetVertex (0), 2) << "la voisine est toujours intacte";
    EXPECT_EQ (m.FaceAt (1)->GetVertex (2), 4);
}

// ============================================================================
//  Copie profonde
// ============================================================================
//
// Modifier la copie ne doit RIEN toucher dans l'original, et reciproquement.
// Chaque membre est verifie : c'est le seul filet contre l'oubli d'un membre, la
// regle de zero ne valant que si les types eux-memes sont corrects.

TEST (TEST_cgmesh_mesh_copy, the_copy_is_independent_of_the_original)
{
    Mesh a;
    a.Init (4, 2);
    float v[12] = { 0,0,0,  1,0,0,  1,1,0,  0,1,0 };
    a.SetVertices (4, v);
    a.SetFace (0, 0, 1, 2);
    a.SetFace (1, 0, 2, 3);
    a.SetName ("original");

    // Un membre de chaque famille : geometrie, derivations, donnees d'auteur.
    a.SetVertexNormal (0, 1.f, 0.f, 0.f);
    a.InitVertexColors (0.25f, 0.5f, 0.75f);
    a.SetTextureCoordinates (std::vector<float>{ 0.f, 0.f,  1.f, 0.f,  1.f, 1.f,  0.f, 1.f }, 4);
    a.AddLine (0, 1);
    a.AddPoint (2);
    a.FaceAt (0)->SetMaterialId (0);
    a.FaceAt (0)->SetUsesTextureCoordinates (true);
    a.FaceAt (0)->SetTexCoord (0u, 3u);
    a.InitTensors ();
    Tensor *t = new Tensor ();
    t->SetKappaMax (7.f);
    a.SetTensor (2, t);
    a.Material_Add (new MaterialColor (10, 20, 30));

    Mesh b (a);

    // --- tout est bien arrive
    EXPECT_EQ (b.GetName (), "original");
    EXPECT_EQ (b.GetNVertices (), 4u);
    EXPECT_EQ (b.GetNFaces (),    2u);
    EXPECT_EQ (b.FaceAt (1)->GetVertex (2), 3);
    EXPECT_FLOAT_EQ (b.GetVertexNormals ()[0], 1.f);
    EXPECT_FLOAT_EQ (b.GetVertexColors ()[1], 0.5f);
    EXPECT_EQ (b.GetNTextureCoordinates (), 4u);
    EXPECT_EQ (b.GetLines ().size (), 2u);
    EXPECT_EQ (b.GetPoints ().size (), 1u);
    EXPECT_EQ (b.FaceAt (0)->GetMaterialId (), 0);
    EXPECT_TRUE (b.FaceAt (0)->UsesTextureCoordinates ());
    EXPECT_EQ (b.FaceAt (0)->GetTexCoordIndex (0), 3);
    EXPECT_EQ (b.GetRevision (), a.GetRevision ()) << "meme geometrie, meme revision";
    ASSERT_NE (b.GetTensor (2), nullptr);
    EXPECT_FLOAT_EQ (b.GetTensor (2)->GetKappaMax (), 7.f);
    ASSERT_EQ (b.GetNMaterials (), 1u);
    ASSERT_NE (b.GetMaterial (0u), nullptr);

    // --- les tenseurs et les materiaux ne sont PAS partages
    EXPECT_NE (b.GetTensor (2), a.GetTensor (2)) << "adresses distinctes";
    EXPECT_NE (b.GetMaterial (0u), a.GetMaterial (0u)) << "le materiau est clone";

    // --- modifier la copie ne touche pas l'original
    b.SetVertex (0, 9.f, 9.f, 9.f);
    b.FaceAt (1)->SetVertex (2, 1u);
    b.SetVertexNormal (0, 0.f, 0.f, 1.f);
    b.SetVertexColor (0, 1.f, 1.f, 1.f);
    b.GetTensor (2)->SetKappaMax (99.f);
    b.GetMaterial (0u)->SetName ("copie");
    b.SetName ("copie");

    EXPECT_FLOAT_EQ (a.GetVertices ()[0], 0.f);
    EXPECT_EQ (a.FaceAt (1)->GetVertex (2), 3);
    EXPECT_FLOAT_EQ (a.GetVertexNormals ()[0], 1.f);
    EXPECT_FLOAT_EQ (a.GetVertexColors ()[0], 0.25f);
    EXPECT_FLOAT_EQ (a.GetTensor (2)->GetKappaMax (), 7.f);
    EXPECT_EQ (a.GetMaterial (0u)->GetName (), std::string ());
    EXPECT_EQ (a.GetName (), "original");

    // --- et reciproquement
    a.SetVertex (1, 5.f, 5.f, 5.f);
    EXPECT_FLOAT_EQ (b.GetVertices ()[3], 1.f);
}

TEST (TEST_cgmesh_mesh_copy, assignment_is_deep_too)
{
    Mesh a;
    a.Init (3, 1);
    a.SetFace (0, 0, 1, 2);
    a.Material_Add (new MaterialColor (1, 2, 3));

    Mesh b;
    b.Init (10, 5);
    b = a;

    EXPECT_EQ (b.GetNVertices (), 3u);
    EXPECT_EQ (b.GetNFaces (),    1u);
    ASSERT_EQ (b.GetNMaterials (), 1u);
    EXPECT_NE (b.GetMaterial (0u), a.GetMaterial (0u)) << "clone, pas partage";

    // L'auto-affectation ne doit pas detruire le maillage.
    Mesh *pa = &a;
    a = *pa;
    EXPECT_EQ (a.GetNFaces (), 1u);
    ASSERT_EQ (a.GetNMaterials (), 1u);
    EXPECT_NE (a.GetMaterial (0u), nullptr);
}

// clone () doit transporter le nom et le TYPE DYNAMIQUE, sans trancher.
TEST (TEST_cgmesh_mesh_copy, cloning_a_material_keeps_its_name)
{
    Mesh a;
    a.Init (3, 1);
    a.SetFace (0, 0, 1, 2);

    MaterialColor *mc = new MaterialColor (7, 8, 9);
    mc->SetName ("acier");
    a.Material_Add (mc);

    MaterialColorExt *me = new MaterialColorExt ();
    me->SetName ("jade");
    a.Material_Add (me);

    Mesh b (a);
    ASSERT_EQ (b.GetNMaterials (), 2u);
    EXPECT_EQ (b.GetMaterial (0u)->GetName (), "acier");
    EXPECT_EQ (b.GetMaterial (1u)->GetName (), "jade");
    EXPECT_EQ (b.GetMaterial (0u)->GetType (), MATERIAL_COLOR);
    EXPECT_EQ (b.GetMaterial (1u)->GetType (), MATERIAL_COLOR_ADV)
        << "clone () doit preserver le TYPE dynamique, sans trancher";

    // Les composantes suivent aussi.
    MaterialColor *cc = dynamic_cast<MaterialColor *>(b.GetMaterial (0u));
    ASSERT_NE (cc, nullptr);
    EXPECT_FLOAT_EQ (cc->GetFloatRed (), 7.f / 255.f);
}

// ============================================================================
//  GetMaterial () -- l'accesseur sans argument de Geometry
// ============================================================================
//
// Ce test est le filet qui interdit la reapparition d'une creation paresseuse
// dans cet accesseur : un maillage sans materiau doit sortir de l'appel
// rigoureusement inchange, revision comprise -- la revision sert de cle aux
// caches de rendu.
TEST (TEST_cgmesh_mesh_materials, reading_the_geometry_material_never_mutates)
{
    Mesh m;
    m.Init (3, 1);
    m.SetFace (0, 0, 1, 2);

    ASSERT_EQ (m.GetNMaterials (), 0u);
    const uint64_t revision = m.GetRevision ();
    const int      faceId   = m.FaceAt (0)->GetMaterialId ();
    ASSERT_EQ (faceId, (int)MATERIAL_NONE);

    // L'appel passe par une reference const : le type interdit desormais toute
    // mutation depuis ce chemin de lecture.
    const Mesh &readOnly = m;
    EXPECT_EQ (readOnly.GetMaterial (), nullptr) << "aucun materiau : aucun a rendre";

    EXPECT_EQ (m.GetNMaterials (), 0u)            << "l'appel n'a rien alloue";
    EXPECT_EQ (m.GetRevision (), revision)        << "l'appel n'a pas invalide les caches";
    EXPECT_EQ (m.FaceAt (0)->GetMaterialId (), faceId) << "l'appel n'a pas reetiquete les faces";
}
