#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "../src/cggraph/core/evaluator.h"
#include "../src/cggraph/nodes/catalog.h"
#include "../src/cggraph/nodes/mesh/ambient_occlusion.h"
#include "../src/cggraph/nodes/mesh/colormap.h"
#include "../src/cggraph/nodes/mesh/convex_hull.h"
#include "../src/cggraph/nodes/mesh/curvature.h"
#include "../src/cggraph/nodes/mesh/icp_align.h"
#include "../src/cggraph/nodes/mesh/thickness.h"
#include "../src/cggraph/nodes/value_types.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/surface_parametric.h"

// ===========================================================================
//  Les six noeuds d'ANALYSE -- natifs seulement
// ===========================================================================
// Chaque cas verifie un RESULTAT, pas seulement que l'appel rend true : un
// noeud dont on ne sait dire que « il s'est execute » ne prouve rien de ce
// qu'il emballe.
//
// Deux d'entre eux sont verifies contre une valeur ANALYTIQUE -- l'epaisseur
// d'une boite fermee, la courbure gaussienne d'un tore a sa selle interieure --
// et non contre un condensat releve sur le code lui-meme.

using namespace cggraph;
using namespace cggraph_nodes;

namespace {

std::shared_ptr<Mesh> MakeMesh (const std::vector<float> &verts,
                                const std::vector<unsigned int> &faces)
{
	std::shared_ptr<Mesh> mesh = std::make_shared<Mesh> ();
	mesh->Init ();
	mesh->SetVertices (static_cast<unsigned int> (verts.size () / 3),
	                   const_cast<float *> (verts.data ()));
	if (!faces.empty ())
		mesh->SetFaces (static_cast<unsigned int> (faces.size () / 3), 3,
		                const_cast<unsigned int *> (faces.data ()));
	return mesh;
}

// Boite fermee, orientee vers l'exterieur, [0,4]x[0,4]x[0,1]. La face du bas
// est en eventail autour d'un sommet DECENTRE (#8), dont le rayon interieur
// (+z) tombe au milieu de la face du haut : l'epaisseur attendue y vaut
// exactement 1.0, le jeu en z. Fixture reprise de tu_cgmesh_thickness.cpp, qui
// l'a calibree pour cette raison.
const unsigned int kBottomCentre = 8u;

std::shared_ptr<Mesh> MakeClosedBox (float dx = 0.f, float dy = 0.f, float dz = 0.f)
{
	std::vector<float> verts = {
		0,0,0,   4,0,0,   4,4,0,   0,4,0,
		0,0,1,   4,0,1,   4,4,1,   0,4,1,
		1.5f,2,0
	};
	for (std::size_t i = 0; i < verts.size (); i += 3)
	{
		verts[i] += dx; verts[i + 1] += dy; verts[i + 2] += dz;
	}
	const std::vector<unsigned int> faces = {
		8,1,0,  8,2,1,  8,3,2,  8,0,3,
		4,5,6,  4,6,7,
		0,1,5,  0,5,4,
		1,2,6,  1,6,5,
		2,3,7,  2,7,6,
		3,0,4,  3,4,7
	};
	return MakeMesh (verts, faces);
}

// Cube unite ferme, plus UN point strictement interieur. L'enveloppe convexe
// doit rendre les huit coins et jeter le neuvieme.
std::shared_ptr<Mesh> MakeCubeWithInteriorPoint ()
{
	const std::vector<float> verts = {
		0,0,0,  1,0,0,  1,1,0,  0,1,0,
		0,0,1,  1,0,1,  1,1,1,  0,1,1,
		0.5f,0.5f,0.5f
	};
	const std::vector<unsigned int> faces = {
		0,2,1, 0,3,2,
		4,5,6, 4,6,7,
		0,1,5, 0,5,4,
		1,2,6, 1,6,5,
		2,3,7, 2,7,6,
		3,0,4, 3,4,7
	};
	return MakeMesh (verts, faces);
}

// Tore ferme (R=3, r=1) : la selle interieure (u=0, v=pi) porte une courbure
// gaussienne ANALYTIQUE de -1/(r(R-r)) = -0.5. Le sommet correspondant est
// d'indice (nv/2)*nu, convention de ParametricTorus::Generate que le cas
// verifie par sa position avant de s'en servir.
std::shared_ptr<Mesh> MakeTorus (unsigned int nu, unsigned int nv, int *saddleIndex)
{
	ParametricTorus torus (nu, nv, 3.0f, 1.0f);
	if (!torus.Generate ())
		return nullptr;

	const unsigned int nVerts = torus.GetNVertices ();
	std::vector<float> verts (3 * static_cast<std::size_t> (nVerts));
	for (unsigned int i = 0; i < nVerts; i++)
		torus.GetVertex (i, &verts[3 * static_cast<std::size_t> (i)]);

	std::vector<unsigned int> faces;
	for (unsigned int f = 0; f < torus.GetNFaces (); f++)
	{
		if (torus.GetFaceNVertices (f) != 3)
			continue;
		for (int k = 0; k < 3; k++)
			faces.push_back (static_cast<unsigned int> (torus.GetFaceVertex (f, k)));
	}

	if (saddleIndex) *saddleIndex = static_cast<int> ((nv / 2) * nu);
	return MakeMesh (verts, faces);
}

Value MeshValue (const std::shared_ptr<Mesh> &mesh)
{
	return Value::Make (Types ().mesh, mesh);
}

Value FieldValue (std::vector<float> values, std::vector<char> defined)
{
	std::shared_ptr<ScalarField> field = std::make_shared<ScalarField> ();
	field->values = std::move (values);
	field->defined = std::move (defined);
	return Value::Make (Types ().scalarField, std::move (field));
}

// Un plan carre [0,4]^2 en `side` x `side` sommets, a la hauteur z, oriente
// vers +z (`up` vrai) ou vers -z.
void AppendPlane (std::vector<float> &verts, std::vector<unsigned int> &faces,
                  unsigned int side, float z, bool up)
{
	const unsigned int base = static_cast<unsigned int> (verts.size () / 3);
	for (unsigned int j = 0; j < side; ++j)
		for (unsigned int i = 0; i < side; ++i)
		{
			verts.push_back (4.0f * static_cast<float> (i) / static_cast<float> (side - 1));
			verts.push_back (4.0f * static_cast<float> (j) / static_cast<float> (side - 1));
			verts.push_back (z);
		}
	for (unsigned int j = 0; j + 1 < side; ++j)
		for (unsigned int i = 0; i + 1 < side; ++i)
		{
			const unsigned int a = base + j * side + i;
			if (up)
			{
				faces.push_back (a); faces.push_back (a + 1); faces.push_back (a + side);
				faces.push_back (a + 1); faces.push_back (a + side + 1); faces.push_back (a + side);
			}
			else
			{
				faces.push_back (a); faces.push_back (a + side); faces.push_back (a + 1);
				faces.push_back (a + 1); faces.push_back (a + side); faces.push_back (a + side + 1);
			}
		}
}

} // namespace

// ---------------------------------------------------------------------------
//  Colormap
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_analysis, colormap_paints_the_two_ends_of_the_jet_ramp)
{
	std::shared_ptr<Mesh> mesh = MakeClosedBox ();
	const unsigned int nv = mesh->GetNVertices ();

	// Champ = coordonnee x. Le minimum et le maximum sont donc connus, et avec
	// eux les deux couleurs extremes de la rampe jet.
	std::vector<float> values (nv);
	for (unsigned int i = 0; i < nv; ++i)
		values[i] = mesh->GetVertices ()[3u * i];

	unsigned int lo = 0, hi = 0;
	for (unsigned int i = 0; i < nv; ++i)
	{
		if (values[i] < values[lo]) lo = i;
		if (values[i] > values[hi]) hi = i;
	}

	ColormapNode node;
	EvalContext ctx;
	ValueList in = { MeshValue (mesh), FieldValue (values, {}) };
	ValueList out (1);
	ASSERT_TRUE (node.Compute (ctx, in, out));

	const Mesh *painted = out[0].Get<Mesh> (Types ().mesh);
	ASSERT_NE (painted, nullptr);
	const std::vector<float> &colors = painted->GetVertexColors ();
	ASSERT_EQ (colors.size (), 3u * nv);

	// Bas de la rampe jet : bleu sombre. Haut : rouge sombre.
	EXPECT_FLOAT_EQ (colors[3u * lo], 0.0f);
	EXPECT_FLOAT_EQ (colors[3u * lo + 1u], 0.0f);
	EXPECT_FLOAT_EQ (colors[3u * lo + 2u], 0.5f);
	EXPECT_FLOAT_EQ (colors[3u * hi], 0.5f);
	EXPECT_FLOAT_EQ (colors[3u * hi + 1u], 0.0f);
	EXPECT_FLOAT_EQ (colors[3u * hi + 2u], 0.0f);

	// L'entree n'a pas ete touchee : elle n'a toujours aucune couleur.
	EXPECT_TRUE (mesh->GetVertexColors ().empty ());
}

TEST (TEST_cggraph_nodes_analysis, colormap_neutralises_undefined_values_the_body_ignores)
{
	// InitVertexColorsFromArray ecrit du noir pour un sommet non defini PUIS
	// l'ecrase par color_jet, faute d'un else (mesh.cpp:142-148). Sans la
	// neutralisation faite par l'adaptateur, une valeur aberrante portee par un
	// sommet non defini ecraserait toute l'echelle et changerait TOUTES les
	// couleurs. Le cas compare donc deux champs qui ne different que par cette
	// valeur-la.
	std::shared_ptr<Mesh> mesh = MakeClosedBox ();
	const unsigned int nv = mesh->GetNVertices ();

	std::vector<float> plain (nv);
	for (unsigned int i = 0; i < nv; ++i)
		plain[i] = static_cast<float> (i);

	std::vector<float> poisoned = plain;
	std::vector<char> defined (nv, (char)1);
	poisoned[nv - 1u] = 1.0e6f;
	defined[nv - 1u] = 0;

	// Reference : le meme champ, dont le sommet non defini porte deja la valeur
	// que la neutralisation va lui donner.
	std::vector<float> reference = plain;
	reference[nv - 1u] = plain[0];
	std::vector<char> referenceDefined (nv, (char)1);
	referenceDefined[nv - 1u] = 0;

	ColormapNode node;
	EvalContext ctx;

	ValueList inA = { MeshValue (mesh), FieldValue (poisoned, defined) };
	ValueList outA (1);
	ASSERT_TRUE (node.Compute (ctx, inA, outA));

	ValueList inB = { MeshValue (mesh), FieldValue (reference, referenceDefined) };
	ValueList outB (1);
	ASSERT_TRUE (node.Compute (ctx, inB, outB));

	const Mesh *a = outA[0].Get<Mesh> (Types ().mesh);
	const Mesh *b = outB[0].Get<Mesh> (Types ().mesh);
	ASSERT_NE (a, nullptr);
	ASSERT_NE (b, nullptr);
	EXPECT_EQ (a->GetVertexColors (), b->GetVertexColors ())
		<< "la valeur d'un sommet NON DEFINI a change les couleurs";
}

// ---------------------------------------------------------------------------
//  ConvexHull
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_analysis, convex_hull_drops_the_interior_point_and_keeps_the_corners)
{
	std::shared_ptr<Mesh> mesh = MakeCubeWithInteriorPoint ();
	ASSERT_EQ (mesh->GetNVertices (), 9u);

	ConvexHullNode node;
	EvalContext ctx;
	ValueList in = { MeshValue (mesh) };
	ValueList out (1);
	ASSERT_TRUE (node.Compute (ctx, in, out));

	const Mesh *hull = out[0].Get<Mesh> (Types ().mesh);
	ASSERT_NE (hull, nullptr);

	// Huit coins, douze triangles : l'enveloppe d'un cube, le point interieur
	// jete. C'est le resultat, pas le fait que l'appel ait rendu true.
	EXPECT_EQ (hull->GetNVertices (), 8u);
	EXPECT_EQ (hull->GetNFaces (), 12u);

	// Aucun sommet rendu n'est le point interieur, et chacun est un coin.
	const std::vector<float> &v = hull->GetVertices ();
	for (unsigned int i = 0; i < hull->GetNVertices (); ++i)
	{
		const bool isCentre = std::fabs (v[3u * i] - 0.5f) < 1e-5f
		                   && std::fabs (v[3u * i + 1u] - 0.5f) < 1e-5f
		                   && std::fabs (v[3u * i + 2u] - 0.5f) < 1e-5f;
		EXPECT_FALSE (isCentre) << "le point interieur est dans l'enveloppe";
		for (int k = 0; k < 3; ++k)
			EXPECT_TRUE (std::fabs (v[3u * i + k]) < 1e-5f
			             || std::fabs (v[3u * i + k] - 1.0f) < 1e-5f);
	}

	EXPECT_EQ (mesh->GetNVertices (), 9u) << "l'entree a ete modifiee";
}

TEST (TEST_cggraph_nodes_analysis, convex_hull_refuses_an_entry_it_cannot_seed)
{
	// double_triangle () ne sait pas amorcer sur moins de quatre points, et ne
	// le signale que par un printf que personne ne lit. L'adaptateur refuse.
	const std::vector<float> verts = { 0,0,0,  1,0,0,  0,1,0 };
	std::shared_ptr<Mesh> mesh = MakeMesh (verts, {});

	ConvexHullNode node;
	EvalContext ctx;
	ValueList in = { MeshValue (mesh) };
	ValueList out (1);
	EXPECT_FALSE (node.Compute (ctx, in, out));
}

// ---------------------------------------------------------------------------
//  ICP
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_analysis, icp_recovers_a_known_translation)
{
	std::shared_ptr<Mesh> target = MakeClosedBox ();
	// Source = la meme boite, deplacee. Le recalage doit la ramener sur la
	// cible : c'est une verite de terrain, pas un condensat.
	const float dx = 0.30f, dy = -0.20f, dz = 0.15f;
	std::shared_ptr<Mesh> source = MakeClosedBox (dx, dy, dz);

	IcpAlignNode node;
	EvalContext ctx;
	ValueList in = { MeshValue (source), MeshValue (target) };
	ValueList out (1);
	ASSERT_TRUE (node.Compute (ctx, in, out));

	const Mesh *aligned = out[0].Get<Mesh> (Types ().mesh);
	ASSERT_NE (aligned, nullptr);
	ASSERT_EQ (aligned->GetNVertices (), target->GetNVertices ());

	// Ecart avant recalage : sqrt(dx^2+dy^2+dz^2) ~ 0.39 sur CHAQUE sommet.
	const std::vector<float> &a = aligned->GetVertices ();
	const std::vector<float> &t = target->GetVertices ();
	float worst = 0.0f;
	for (std::size_t i = 0; i < t.size (); i += 3)
	{
		const float d = std::sqrt ((a[i]-t[i])*(a[i]-t[i])
		                         + (a[i+1]-t[i+1])*(a[i+1]-t[i+1])
		                         + (a[i+2]-t[i+2])*(a[i+2]-t[i+2]));
		worst = std::max (worst, d);
	}
	EXPECT_LT (worst, 0.05f) << "ecart maximal apres recalage : " << worst;

	// La source n'a pas bouge : c'est une copie qui a ete transformee.
	EXPECT_NEAR (source->GetVertices ()[0], dx, 1e-6f);
}

// ---------------------------------------------------------------------------
//  AmbientOcclusion
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_analysis, ambient_occlusion_grows_when_the_occluder_comes_closer)
{
	// Deux plans qui se font face. En rapprochant le second, l'occlusion doit
	// AUGMENTER : c'est la propriete que l'algorithme est cense avoir, et elle
	// ne depend d'aucune valeur relevee sur lui.
	auto occlusionSum = [] (float gap) {
		std::vector<float> verts;
		std::vector<unsigned int> faces;
		AppendPlane (verts, faces, 9u, 0.0f, true);    // normales vers +z
		AppendPlane (verts, faces, 9u, gap, false);    // normales vers -z
		std::shared_ptr<Mesh> mesh = MakeMesh (verts, faces);

		AmbientOcclusionNode node;
		EvalContext ctx;
		ValueList in = { MeshValue (mesh) };
		ValueList out (1);
		EXPECT_TRUE (node.Compute (ctx, in, out));
		const ScalarField *field = out[0].Get<ScalarField> (Types ().scalarField);
		EXPECT_NE (field, nullptr);
		if (field == nullptr) return -1.0f;
		EXPECT_EQ (field->values.size (), mesh->GetNVertices ());

		float total = 0.0f;
		for (std::size_t i = 0; i < field->values.size (); ++i)
		{
			// L'occlusion est bornee par clampOcclusion.
			EXPECT_GE (field->values[i], 0.0f);
			EXPECT_LE (field->values[i], 1.0f);
			total += field->values[i];
		}
		return total;
	};

	const float faraway = occlusionSum (1.5f);
	const float nearby = occlusionSum (0.25f);
	ASSERT_GE (faraway, 0.0f);
	ASSERT_GE (nearby, 0.0f);
	EXPECT_GT (nearby, faraway) << "occlusion proche = " << nearby
	                            << ", lointaine = " << faraway;
	EXPECT_GT (nearby, 0.0f) << "aucun sommet n'est occulte";
}

TEST (TEST_cggraph_nodes_analysis, ambient_occlusion_marks_an_area_less_vertex_undefined)
{
	// Un sommet qu'aucune face ne reference a une aire nulle. Le corps le laisse
	// a une occlusion de 0, indistinguable d'un sommet parfaitement degage :
	// c'est l'adaptateur qui le marque NON DEFINI, et ce cas est le seul qui
	// atteigne cette branche -- sur un maillage ordinaire, tout sommet porte
	// une aire.
	std::vector<float> verts;
	std::vector<unsigned int> faces;
	AppendPlane (verts, faces, 5u, 0.0f, true);
	const unsigned int isolated = static_cast<unsigned int> (verts.size () / 3);
	verts.push_back (2.0f); verts.push_back (2.0f); verts.push_back (3.0f);
	std::shared_ptr<Mesh> mesh = MakeMesh (verts, faces);

	AmbientOcclusionNode node;
	EvalContext ctx;
	ValueList in = { MeshValue (mesh) };
	ValueList out (1);
	ASSERT_TRUE (node.Compute (ctx, in, out));

	const ScalarField *field = out[0].Get<ScalarField> (Types ().scalarField);
	ASSERT_NE (field, nullptr);
	ASSERT_EQ (field->defined.size (), mesh->GetNVertices ());
	EXPECT_EQ (field->defined[isolated], 0) << "le sommet isole est declare defini";
	// Et les sommets portes par des faces, eux, sont definis.
	EXPECT_EQ (field->defined[0], 1);
}

TEST (TEST_cggraph_nodes_analysis, ambient_occlusion_does_not_depend_on_a_stale_bounding_box)
{
	// Le rayon d'influence est derive de la diagonale de la boite englobante,
	// que Mesh MET EN CACHE sans detecter sa peremption -- et dont le lecteur ne
	// teste pas la vacuite. Sur un maillage jamais passe par computebbox (), le
	// corps lisait donc de la memoire non initialisee, et rendait un resultat
	// different d'une execution a l'autre. Le cas fige l'invariant qui le
	// ferme : la sortie ne doit pas dependre de l'etat du cache de boite dans
	// l'entree.
	//
	// ⚠ Filet PROBABILISTE, et c'est dit : ce contre quoi il protege est une
	// lecture non initialisee, c'est-a-dire un comportement indefini, dont
	// aucun observable n'est garanti.
	std::vector<float> verts;
	std::vector<unsigned int> faces;
	AppendPlane (verts, faces, 9u, 0.0f, true);
	AppendPlane (verts, faces, 9u, 0.25f, false);

	std::shared_ptr<Mesh> withBox = MakeMesh (verts, faces);
	withBox->computebbox ();
	std::shared_ptr<Mesh> withoutBox = MakeMesh (verts, faces);

	AmbientOcclusionNode node;
	EvalContext ctx;

	ValueList inA = { MeshValue (withBox) };
	ValueList outA (1);
	ASSERT_TRUE (node.Compute (ctx, inA, outA));

	ValueList inB = { MeshValue (withoutBox) };
	ValueList outB (1);
	ASSERT_TRUE (node.Compute (ctx, inB, outB));

	const ScalarField *a = outA[0].Get<ScalarField> (Types ().scalarField);
	const ScalarField *b = outB[0].Get<ScalarField> (Types ().scalarField);
	ASSERT_NE (a, nullptr);
	ASSERT_NE (b, nullptr);
	EXPECT_EQ (a->values, b->values)
		<< "l'occlusion depend d'une boite englobante que l'entree portait ou non";
}

// ---------------------------------------------------------------------------
//  Thickness
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_analysis, thickness_measures_the_analytic_gap_of_a_closed_box)
{
	// Boite [0,4]x[0,4]x[0,1] : au sommet decentre du bas, le rayon interieur
	// (+z) traverse exactement 1.0. Verite ANALYTIQUE.
	std::shared_ptr<Mesh> mesh = MakeClosedBox ();

	// Rayon unique, cone nul, aucun lissage : le corps se reduit alors
	// exactement a ComputeWallThickness.
	ThicknessNode node (1, 0.0f, 0);
	EvalContext ctx;
	ValueList in = { MeshValue (mesh) };
	ValueList out (1);
	ASSERT_TRUE (node.Compute (ctx, in, out));

	const ScalarField *field = out[0].Get<ScalarField> (Types ().scalarField);
	ASSERT_NE (field, nullptr);
	ASSERT_EQ (field->values.size (), mesh->GetNVertices ());
	ASSERT_EQ (field->defined.size (), mesh->GetNVertices ());

	EXPECT_EQ (field->defined[kBottomCentre], 1);
	EXPECT_NEAR (field->values[kBottomCentre], 1.0f, 1e-3f);

	// L'entree n'a pas ete touchee : le corps recalcule les normales du
	// maillage qu'on lui donne, et ce maillage-la est une copie.
	EXPECT_TRUE (mesh->GetVertexNormals ().empty ());
}

TEST (TEST_cggraph_nodes_analysis, thickness_leaves_an_open_mesh_undefined_rather_than_wrong)
{
	// Un plan seul n'a pas de paroi opposee : le champ doit dire « non defini »,
	// et surtout pas « epaisseur nulle ». Confondre les deux donne une carte
	// fausse partout ou le maillage est ouvert.
	std::vector<float> verts;
	std::vector<unsigned int> faces;
	AppendPlane (verts, faces, 5u, 0.0f, true);
	std::shared_ptr<Mesh> mesh = MakeMesh (verts, faces);

	ThicknessNode node (1, 0.0f, 0);
	EvalContext ctx;
	ValueList in = { MeshValue (mesh) };
	ValueList out (1);
	ASSERT_TRUE (node.Compute (ctx, in, out));

	const ScalarField *field = out[0].Get<ScalarField> (Types ().scalarField);
	ASSERT_NE (field, nullptr);
	ASSERT_EQ (field->defined.size (), mesh->GetNVertices ());
	for (std::size_t i = 0; i < field->defined.size (); ++i)
		EXPECT_EQ (field->defined[i], 0) << "sommet " << i;
}

// ---------------------------------------------------------------------------
//  Curvature
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_analysis, curvature_matches_the_analytic_gaussian_at_the_torus_saddle)
{
	int saddle = -1;
	std::shared_ptr<Mesh> mesh = MakeTorus (120u, 60u, &saddle);
	ASSERT_NE (mesh, nullptr);
	ASSERT_GE (saddle, 0);

	// L'indice du sommet de selle vient d'une convention d'indexation interne a
	// Generate () : on la verifie plutot que de la supposer.
	const std::vector<float> &v = mesh->GetVertices ();
	ASSERT_NEAR (v[3 * (std::size_t)saddle], 3.0f - 1.0f, 1e-4f);
	ASSERT_NEAR (v[3 * (std::size_t)saddle + 1], 0.0f, 1e-4f);
	ASSERT_NEAR (v[3 * (std::size_t)saddle + 2], 0.0f, 1e-4f);

	// method 2 = Desbrun (voir kMethods dans curvature.cpp), curvature 3 =
	// Gaussienne.
	CurvatureNode node (2, 3);
	EvalContext ctx;
	ValueList in = { MeshValue (mesh) };
	ValueList out (2);
	ASSERT_TRUE (node.Compute (ctx, in, out));

	const ScalarField *field = out[1].Get<ScalarField> (Types ().scalarField);
	ASSERT_NE (field, nullptr);
	ASSERT_EQ (field->values.size (), mesh->GetNVertices ());
	ASSERT_EQ (field->defined[saddle], 1);

	// K = k_meridien * k_parallele = (1/r) * (-1/(R-r)) = -0.5 a la selle
	// interieure. Le signe est celui d'une selle ; la magnitude est analytique.
	EXPECT_LT (field->values[saddle], 0.0f) << "la selle interieure n'est pas une selle";
	EXPECT_NEAR (std::fabs (field->values[saddle]), 0.5f, 0.06f);

	// Le maillage rendu PORTE ses tenseurs : c'est la seconde sortie, et elle
	// n'est pas une copie decorative de l'entree.
	const Mesh *withTensors = out[0].Get<Mesh> (Types ().mesh);
	ASSERT_NE (withTensors, nullptr);
	EXPECT_EQ (withTensors->GetNTensors (), mesh->GetNVertices ());
	EXPECT_EQ (mesh->GetNTensors (), 0u) << "l'entree a recu des tenseurs";
}

TEST (TEST_cggraph_nodes_analysis, curvature_leaves_border_vertices_undefined_rather_than_flat)
{
	// Les estimateurs n'ecrivent AUCUN tenseur sur un sommet de bord ou non
	// manifold : ils y posent nullptr. Rendre 0 pour ces sommets ferait d'une
	// absence de mesure une courbure nulle -- une carte fausse sur tout le bord.
	// Un plan ouvert est le seul maillage de ce fichier qui ait un bord ; le
	// tore n'en a pas, et ce cas est donc le seul a atteindre cette branche.
	std::vector<float> verts;
	std::vector<unsigned int> faces;
	AppendPlane (verts, faces, 7u, 0.0f, true);
	std::shared_ptr<Mesh> mesh = MakeMesh (verts, faces);

	CurvatureNode node (2, 3);
	EvalContext ctx;
	ValueList in = { MeshValue (mesh) };
	ValueList out (2);
	ASSERT_TRUE (node.Compute (ctx, in, out));

	const ScalarField *field = out[1].Get<ScalarField> (Types ().scalarField);
	ASSERT_NE (field, nullptr);
	ASSERT_EQ (field->defined.size (), mesh->GetNVertices ());

	// Coin du plan : sommet de bord, sans tenseur.
	EXPECT_EQ (field->defined[0], 0) << "un sommet de bord est declare defini";
	// Centre du plan : sommet interieur, avec tenseur.
	const unsigned int centre = 7u * 3u + 3u;
	EXPECT_EQ (field->defined[centre], 1) << "un sommet interieur est declare indefini";

	std::size_t undefined = 0;
	for (char d : field->defined)
		if (d == 0) ++undefined;
	// Les 24 sommets du pourtour d'une grille 7x7.
	EXPECT_EQ (undefined, 24u);
}

TEST (TEST_cggraph_nodes_analysis, curvature_refuses_the_two_settings_it_cannot_honour)
{
	int saddle = -1;
	std::shared_ptr<Mesh> mesh = MakeTorus (40u, 20u, &saddle);
	ASSERT_NE (mesh, nullptr);

	EvalContext ctx;
	ValueList in = { MeshValue (mesh) };

	// Cinq methodes exposees, pas six : TENSOR_STEINER a son corps sous #if 0.
	{
		CurvatureNode node (5, 3);
		ValueList out (2);
		EXPECT_FALSE (node.Compute (ctx, in, out));
	}
	{
		CurvatureNode node (0, 4);
		ValueList out (2);
		EXPECT_FALSE (node.Compute (ctx, in, out));
	}
	// Et les cinq positions valides produisent bien un champ.
	for (int method = 0; method < 5; ++method)
	{
		CurvatureNode node (method, 3);
		ValueList out (2);
		ASSERT_TRUE (node.Compute (ctx, in, out)) << "methode " << method;
		const ScalarField *field = out[1].Get<ScalarField> (Types ().scalarField);
		ASSERT_NE (field, nullptr) << "methode " << method;
		EXPECT_EQ (field->values.size (), mesh->GetNVertices ());
	}
}

TEST (TEST_cggraph_nodes_analysis, the_curvature_setting_changes_the_field)
{
	// Un parametre qui ne change pas la sortie est un mensonge que la UI rend
	// credible (§8.1bis). Les deux courbures demandees doivent differer.
	int saddle = -1;
	std::shared_ptr<Mesh> mesh = MakeTorus (40u, 20u, &saddle);
	ASSERT_NE (mesh, nullptr);

	EvalContext ctx;
	ValueList in = { MeshValue (mesh) };

	CurvatureNode gaussian (2, 3);
	ValueList outG (2);
	ASSERT_TRUE (gaussian.Compute (ctx, in, outG));

	CurvatureNode mean (2, 2);
	ValueList outM (2);
	ASSERT_TRUE (mean.Compute (ctx, in, outM));

	const ScalarField *g = outG[1].Get<ScalarField> (Types ().scalarField);
	const ScalarField *m = outM[1].Get<ScalarField> (Types ().scalarField);
	ASSERT_NE (g, nullptr);
	ASSERT_NE (m, nullptr);
	EXPECT_NE (g->values, m->values);
}

// ---------------------------------------------------------------------------
//  Le jeton d'annulation traverse les cinq noeuds qui ont une boucle externe
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_analysis, the_cancellation_token_reaches_the_five_loop_bearing_bodies)
{
	// Drapeau pose AVANT l'appel : chaque corps doit renoncer dans sa boucle
	// externe. Le sixieme noeud -- la carte de couleurs -- n'a pas de boucle
	// externe a interrompre : c'est une passe lineaire de coloriage, et un
	// point de test y serait indistinguable de son absence.
	std::atomic<bool> cancelled (true);
	EvalContext ctx;
	ctx.SetCancellationFlag (&cancelled);

	int saddle = -1;
	std::shared_ptr<Mesh> torus = MakeTorus (60u, 30u, &saddle);
	ASSERT_NE (torus, nullptr);
	std::shared_ptr<Mesh> box = MakeClosedBox ();
	std::shared_ptr<Mesh> cube = MakeCubeWithInteriorPoint ();

	// Les CINQ estimateurs, un par un : chacun porte sa propre garde dans sa
	// propre boucle, et n'en tester qu'un laisserait les quatre autres sans
	// filet. Hybrid delegue a Desbrun, ce qui fait de lui le cas qui verifie
	// aussi que le code de retour REMONTE.
	for (int method = 0; method < 5; ++method)
	{
		CurvatureNode node (method, 3);
		ValueList in = { MeshValue (torus) };
		ValueList out (2);
		EXPECT_FALSE (node.Compute (ctx, in, out)) << "mesh.curvature, methode " << method;
	}
	{
		ThicknessNode node (16, 60.0f, 1);
		ValueList in = { MeshValue (torus) };
		ValueList out (1);
		EXPECT_FALSE (node.Compute (ctx, in, out)) << "mesh.thickness";
	}
	{
		AmbientOcclusionNode node;
		ValueList in = { MeshValue (torus) };
		ValueList out (1);
		EXPECT_FALSE (node.Compute (ctx, in, out)) << "mesh.ambient_occlusion";
	}
	{
		ConvexHullNode node;
		ValueList in = { MeshValue (cube) };
		ValueList out (1);
		EXPECT_FALSE (node.Compute (ctx, in, out)) << "mesh.hull.convex";
	}
	{
		// ICP renonce des la premiere iteration : la source rendue est donc la
		// source telle quelle, non recalee. C'est observable, et c'est ce qui
		// distingue le jeton d'un no-op.
		IcpAlignNode node;
		std::shared_ptr<Mesh> moved = MakeClosedBox (0.30f, -0.20f, 0.15f);
		ValueList in = { MeshValue (moved), MeshValue (box) };
		ValueList out (1);
		ASSERT_TRUE (node.Compute (ctx, in, out)) << "mesh.align.icp";
		const Mesh *aligned = out[0].Get<Mesh> (Types ().mesh);
		ASSERT_NE (aligned, nullptr);
		EXPECT_NEAR (aligned->GetVertices ()[0], 0.30f, 1e-5f)
			<< "mesh.align.icp a recale malgre le jeton";
	}
}

// ---------------------------------------------------------------------------
//  Les six noeuds passent par le catalogue et par l'evaluateur
// ---------------------------------------------------------------------------

namespace {

// Source de test : verse un maillage deja construit. Le catalogue n'a pas de
// noeud qui verse une valeur en memoire.
class MeshSourceNode : public Node
{
public:
	explicit MeshSourceNode (std::shared_ptr<Mesh> mesh) : m_mesh (std::move (mesh))
	{
		m_desc.typeName = "test.mesh.source";
		m_desc.outputs.push_back ({ "maillage", Types ().mesh, false });
	}
	const NodeDesc &GetDesc () const override { return m_desc; }
	bool Compute (EvalContext &, const ValueList &, ValueList &out) override
	{
		out[0] = Value::Make (Types ().mesh, m_mesh);
		return true;
	}

private:
	NodeDesc m_desc;
	std::shared_ptr<Mesh> m_mesh;
};

} // namespace

TEST (TEST_cggraph_nodes_analysis, thickness_feeds_colormap_through_the_graph)
{
	// La chaine que le type ScalarField existe pour rendre possible : une
	// mesure alimente un coloriage, sans qu'aucun des deux ne connaisse
	// l'autre.
	Graph graph;
	const NodeId source =
		graph.AddNode (std::unique_ptr<Node> (new MeshSourceNode (MakeClosedBox ())));
	const NodeId thickness = graph.AddNode (MakeNode ("mesh.thickness"));
	const NodeId colormap = graph.AddNode (MakeNode ("mesh.color.map"));
	ASSERT_NE (thickness, kInvalidNodeId);
	ASSERT_NE (colormap, kInvalidNodeId);

	ASSERT_EQ (graph.Connect (source, 0, thickness, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (source, 0, colormap, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (thickness, 0, colormap, 1), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	const EvalResult result = evaluator.Evaluate (colormap, outputs, ctx);
	ASSERT_EQ (result.status, EvalStatus::Ok) << result.detail;
	ASSERT_EQ (outputs.size (), 1u);

	const Mesh *painted = outputs[0].Get<Mesh> (Types ().mesh);
	ASSERT_NE (painted, nullptr);
	EXPECT_EQ (painted->GetVertexColors ().size (), 3u * painted->GetNVertices ());
}

TEST (TEST_cggraph_nodes_analysis, a_scalar_field_does_not_connect_to_a_mesh_port)
{
	// Le champ scalaire est un type A PART. Le brancher sur un port de maillage
	// doit etre refuse a la connexion, et non produire un resultat faux.
	Graph graph;
	const NodeId curvature = graph.AddNode (MakeNode ("mesh.curvature"));
	const NodeId colormap = graph.AddNode (MakeNode ("mesh.color.map"));
	ASSERT_NE (curvature, kInvalidNodeId);
	ASSERT_NE (colormap, kInvalidNodeId);

	// Sortie 1 de la courbure = le champ ; entree 0 du coloriage = le maillage.
	EXPECT_EQ (graph.Connect (curvature, 1, colormap, 0), ConnectStatus::TypeMismatch);
	// Et le bon appariement passe.
	EXPECT_EQ (graph.Connect (curvature, 1, colormap, 1), ConnectStatus::Ok);
	EXPECT_EQ (graph.Connect (curvature, 0, colormap, 0), ConnectStatus::Ok);
}
