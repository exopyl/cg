#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <utility>
#include <memory>
#include <vector>

#include "../src/cggraph/core/evaluator.h"
#include "../src/cggraph/nodes/catalog.h"
#include "../src/cggraph/nodes/mesh/color.h"
#include "../src/cggraph/nodes/mesh/mounts.h"
#include "../src/cggraph/nodes/shapes/contour_ops.h"
#include "../src/cggraph/nodes/text/load_font.h"
#include "../src/cggraph/nodes/value_types.h"
#include "../src/cgmesh/contour_ops.h"
#include "../src/cgmesh/countersink.h"
#include "../src/cgmesh/mesh_slab.h"
#include "../src/cgmesh/extrude_contours.h"
#include "../src/cgmesh/mesh.h"

// ===========================================================================
//  L'EMPILEMENT 2,5D : socle d'epaisseur propre, plaque silhouette, fusion
// ===========================================================================
//
// Ce que ces cas gardent n'est pas « Clipper2 sait decaler » -- c'est l'affaire
// de tu_cgmesh_contour_ops.cpp -- mais la CHAINE de noeuds : que
// `shape.contours.plate`, `shape.contours.offset`, `zBottom` et `mesh.merge`
// composent la piece qu'un socle demande, et que chacun REFUSE plutot que de
// rendre faux.
//
// Pourquoi un fichier a part de tu_cggraph_nodes_shapes.cpp : celui-la garde les
// adaptateurs de FORMES (le catalogue parametrique, la baie gothique, les
// profils). Ce chantier-ci compose des contours et des maillages ; il n'exerce
// aucun des memes corps, et melanger les deux rendrait chaque fichier moins
// lisible qu'il ne l'est.
//
// ===========================================================================

using namespace cggraph;
using namespace cggraph_nodes;

namespace {

std::vector<unsigned char> ReadAllBytes (const std::string &path)
{
	std::ifstream f (path, std::ios::binary);
	if (!f) return {};
	return std::vector<unsigned char> (std::istreambuf_iterator<char> (f),
	                                   std::istreambuf_iterator<char> ());
}

// Chaine police -> contours de texte : le seul producteur de contours du
// catalogue qui n'ait pas besoin d'un fichier SVG. Rend kInvalidNodeId quand la
// police de test est absente, pour que l'appelant puisse sauter le cas.
NodeId AddTextContours (Graph &graph, const char *text, float size)
{
	std::vector<unsigned char> bytes = ReadAllBytes ("./test/data/fonts/DejaVuSans.ttf");
	if (bytes.empty ()) return kInvalidNodeId;

	const NodeId fontId = graph.AddNode (MakeNode ("text.font.load"));
	static_cast<LoadFontNode *> (graph.FindNode (fontId))->SetBytes (std::move (bytes));

	const NodeId contours = graph.AddNode (MakeNode ("text.contours"));
	if (graph.Connect (fontId, 0, contours, 0) != ConnectStatus::Ok) return kInvalidNodeId;

	Node *node = graph.FindNode (contours);
	node->GetParams ().SetString ("text", text);
	node->GetParams ().SetFloat ("size", size);
	return contours;
}

void zRange (const Mesh &mesh, float &lo, float &hi)
{
	lo = 1e30f; hi = -1e30f;
	Mesh &m = const_cast<Mesh &> (mesh);
	for (unsigned int v = 0; v < m.GetNVertices (); v++)
	{
		float p[3];
		m.GetVertex (v, p);
		lo = std::min (lo, p[2]);
		hi = std::max (hi, p[2]);
	}
}

typedef std::vector<ExtrudeContour> Contours;

float totalAbsArea (const Contours &in)
{
	float a = 0.f;
	for (const ExtrudeContour &c : in) a += std::fabs (contourSignedArea (c.pts));
	return a;
}

}  // namespace

// --- pose en Z --------------------------------------------------------------

TEST (TEST_cggraph_nodes_stack, the_extruder_poses_its_solid_where_z_bottom_says)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "H", 10.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId extrude = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, extrude, 0), ConnectStatus::Ok);
	Node *node = graph.FindNode (extrude);
	node->GetParams ().SetFloat ("depth", 3.0f);
	node->GetParams ().SetFloat ("zBottom", 7.0f);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (extrude, outputs, ctx).IsOk ());
	std::shared_ptr<const Mesh> mesh = outputs[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (mesh, nullptr);

	// `depth` est une EPAISSEUR, pas une cote absolue : le solide occupe
	// [zBottom, zBottom + depth]. C'est ce qui rend l'empilement independant --
	// deplacer le socle ne change pas l'epaisseur des lettres.
	float lo = 0.f, hi = 0.f;
	zRange (*mesh, lo, hi);
	EXPECT_NEAR (lo, 7.0f, 1e-4f);
	EXPECT_NEAR (hi, 10.0f, 1e-4f);
}

// --- socle + lettres --------------------------------------------------------

TEST (TEST_cggraph_nodes_stack, a_plate_and_its_letters_stack_into_one_mesh)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "HI", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId plate = graph.AddNode (MakeNode ("shape.contours.plate"));
	ASSERT_EQ (graph.Connect (contours, 0, plate, 0), ConnectStatus::Ok);
	graph.FindNode (plate)->GetParams ().SetFloat ("margin", 4.0f);

	const NodeId plateSolid = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (plate, 0, plateSolid, 0), ConnectStatus::Ok);
	graph.FindNode (plateSolid)->GetParams ().SetFloat ("depth", 2.0f);
	graph.FindNode (plateSolid)->GetParams ().SetFloat ("zBottom", 0.0f);

	const NodeId letters = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, letters, 0), ConnectStatus::Ok);
	graph.FindNode (letters)->GetParams ().SetFloat ("depth", 5.0f);
	graph.FindNode (letters)->GetParams ().SetFloat ("zBottom", 2.0f);

	const NodeId merge = graph.AddNode (MakeNode ("mesh.merge"));
	ASSERT_EQ (graph.Connect (plateSolid, 0, merge, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (letters, 0, merge, 1), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList merged, onlyPlate, onlyLetters;
	ASSERT_TRUE (evaluator.Evaluate (merge, merged, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (plateSolid, onlyPlate, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (letters, onlyLetters, ctx).IsOk ());

	std::shared_ptr<const Mesh> all = merged[0].Share<Mesh> (Types ().mesh);
	std::shared_ptr<const Mesh> p = onlyPlate[0].Share<Mesh> (Types ().mesh);
	std::shared_ptr<const Mesh> l = onlyLetters[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (all, nullptr);
	ASSERT_NE (p, nullptr);
	ASSERT_NE (l, nullptr);

	// CONCATENATION et non union booleenne : la piece porte exactement les deux
	// coques. Si ce compte cessait d'etre exact, `mesh.merge` aurait commence a
	// faire autre chose que ce que son en-tete promet.
	EXPECT_EQ (all->GetNVertices (), p->GetNVertices () + l->GetNVertices ());
	EXPECT_EQ (all->GetNFaces (), p->GetNFaces () + l->GetNFaces ());

	// Et la piece occupe bien les deux etages : socle en bas, lettres dessus.
	float lo = 0.f, hi = 0.f;
	zRange (*all, lo, hi);
	EXPECT_NEAR (lo, 0.0f, 1e-4f);
	EXPECT_NEAR (hi, 7.0f, 1e-4f);

	// L'entree n'est pas modifiee par la fusion : le socle seul reste le socle.
	float plo = 0.f, phi = 0.f;
	zRange (*p, plo, phi);
	EXPECT_NEAR (phi, 2.0f, 1e-4f);
}

TEST (TEST_cggraph_nodes_stack, merging_without_a_second_mesh_passes_the_first_through)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "H", 10.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId extrude = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, extrude, 0), ConnectStatus::Ok);
	const NodeId merge = graph.AddNode (MakeNode ("mesh.merge"));
	ASSERT_EQ (graph.Connect (extrude, 0, merge, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList out, direct;
	// C'est ce qui rend le noeud utilisable dans un graphe FIXE : une page qui
	// propose « socle : aucun » n'a rien a brancher sur la seconde entree, et ne
	// peut pas debrancher un noeud.
	ASSERT_TRUE (evaluator.Evaluate (merge, out, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (extrude, direct, ctx).IsOk ());

	std::shared_ptr<const Mesh> a = out[0].Share<Mesh> (Types ().mesh);
	std::shared_ptr<const Mesh> b = direct[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (a, nullptr);
	ASSERT_NE (b, nullptr);
	// Le MEME maillage, repartage : sans socle, la fusion ne paie aucune copie.
	EXPECT_EQ (a.get (), b.get ());
}

// --- les deux formes de plaque ---------------------------------------------

TEST (TEST_cggraph_nodes_stack, the_plate_bounds_the_text_and_the_silhouette_follows_it)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "HI", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId plate = graph.AddNode (MakeNode ("shape.contours.plate"));
	ASSERT_EQ (graph.Connect (contours, 0, plate, 0), ConnectStatus::Ok);
	graph.FindNode (plate)->GetParams ().SetFloat ("margin", 3.0f);

	const NodeId halo = graph.AddNode (MakeNode ("shape.contours.offset"));
	ASSERT_EQ (graph.Connect (contours, 0, halo, 0), ConnectStatus::Ok);
	graph.FindNode (halo)->GetParams ().SetFloat ("delta", 3.0f);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outText, outPlate, outHalo;
	ASSERT_TRUE (evaluator.Evaluate (contours, outText, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (plate, outPlate, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (halo, outHalo, ctx).IsOk ());

	std::shared_ptr<const Contours> text = outText[0].Share<Contours> (Types ().extrudeContours);
	std::shared_ptr<const Contours> rect = outPlate[0].Share<Contours> (Types ().extrudeContours);
	std::shared_ptr<const Contours> silhouette =
		outHalo[0].Share<Contours> (Types ().extrudeContours);
	ASSERT_NE (text, nullptr);
	ASSERT_NE (rect, nullptr);
	ASSERT_NE (silhouette, nullptr);

	// La plaque est UN rectangle : l'emprise plus la marge, et rien de la forme.
	ASSERT_EQ (rect->size (), 1u);
	float tx0 = 0.f, ty0 = 0.f, tx1 = 0.f, ty1 = 0.f;
	float px0 = 0.f, py0 = 0.f, px1 = 0.f, py1 = 0.f;
	ASSERT_TRUE (contoursBBox (*text, tx0, ty0, tx1, ty1));
	ASSERT_TRUE (contoursBBox (*rect, px0, py0, px1, py1));
	EXPECT_NEAR (px0, tx0 - 3.0f, 1e-3f);
	EXPECT_NEAR (py1, ty1 + 3.0f, 1e-3f);

	// La silhouette SUIT les lettres : elle porte aussi loin que la marge, donc
	// meme emprise a l'arrondi des coins pres, mais elle epouse les contre-formes
	// -- donc une aire strictement plus PETITE que le rectangle.
	float hx0 = 0.f, hy0 = 0.f, hx1 = 0.f, hy1 = 0.f;
	ASSERT_TRUE (contoursBBox (*silhouette, hx0, hy0, hx1, hy1));
	EXPECT_NEAR (hx1 - hx0, px1 - px0, 0.4f);
	EXPECT_LT (totalAbsArea (*silhouette), totalAbsArea (*rect))
		<< "une silhouette qui remplit autant qu'un rectangle n'en est pas une";

	// Le sens de trace de la plaque est aligne sur celui des lettres : a
	// l'envers, elle les SOUSTRAIRAIT au lieu de les porter, et le meme graphe
	// rendrait un socle avec une police et un pochoir avec une autre.
	float widest = 0.f;
	for (const ExtrudeContour &c : *text)
	{
		const float a = contourSignedArea (c.pts);
		if (std::fabs (a) > std::fabs (widest)) widest = a;
	}
	EXPECT_GT (contourSignedArea ((*rect)[0].pts) * widest, 0.f);
}

// --- le garde-fou de la silhouette -----------------------------------------

TEST (TEST_cggraph_nodes_stack, the_offset_node_reports_a_silhouette_that_came_apart)
{
	Graph graph;
	// Deux barres largement espacees : a halo etroit, la plaque silhouette sort
	// en DEUX morceaux -- donc la piece imprimee en deux pieces.
	const NodeId contours = AddTextContours (graph, "I I", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId halo = graph.AddNode (MakeNode ("shape.contours.offset"));
	ASSERT_EQ (graph.Connect (contours, 0, halo, 0), ConnectStatus::Ok);
	ContourOffsetNode *node = static_cast<ContourOffsetNode *> (graph.FindNode (halo));

	Evaluator evaluator (graph);
	EvalContext ctx;

	node->GetParams ().SetFloat ("delta", 0.5f);
	ValueList apart;
	ASSERT_TRUE (evaluator.Evaluate (halo, apart, ctx).IsOk ());
	EXPECT_GE (node->GetPieceCount (), 2u)
		<< "le compte de morceaux ne signale pas la rupture -- c'est le seul "
		   "garde-fou avant l'impression";

	// Assez large pour relier : un seul morceau. Sans ce second point, le cas ne
	// prouverait pas que le compte MESURE quelque chose.
	node->GetParams ().SetFloat ("delta", 8.0f);
	ValueList joined;
	ASSERT_TRUE (evaluator.Evaluate (halo, joined, ctx).IsOk ());
	EXPECT_EQ (node->GetPieceCount (), 1u);
}

TEST (TEST_cggraph_nodes_stack, an_offset_that_consumes_the_matter_refuses)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "I", 10.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId halo = graph.AddNode (MakeNode ("shape.contours.offset"));
	ASSERT_EQ (graph.Connect (contours, 0, halo, 0), ConnectStatus::Ok);
	// Retrecir de 50 mm une lettre qui en mesure 10 : il ne reste rien. Le noeud
	// REFUSE, plutot que de publier une region vide que l'extrudeur rejetterait
	// plus loin sans pouvoir dire d'ou elle vient.
	graph.FindNode (halo)->GetParams ().SetFloat ("delta", -50.0f);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList out;
	EXPECT_FALSE (evaluator.Evaluate (halo, out, ctx).IsOk ());
}

// ---------------------------------------------------------------------------
//  LE SELECTEUR : ce qu'une page a graphe fixe ne pouvait pas dire
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_stack, the_selector_hands_back_the_branch_its_index_names)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "H", 10.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	// Trois variantes discernables par leur seule plage de Z.
	NodeId branches[3];
	const float depths[3] = { 1.0f, 2.0f, 3.0f };
	for (int k = 0; k < 3; ++k)
	{
		branches[k] = graph.AddNode (MakeNode ("shape.extrude"));
		ASSERT_EQ (graph.Connect (contours, 0, branches[k], 0), ConnectStatus::Ok);
		graph.FindNode (branches[k])->GetParams ().SetFloat ("depth", depths[k]);
	}

	const NodeId select = graph.AddNode (MakeNode ("flow.select.mesh"));
	for (int k = 0; k < 3; ++k)
		ASSERT_EQ (graph.Connect (branches[k], 0, select, k), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	for (int k = 0; k < 3; ++k)
	{
		graph.FindNode (select)->GetParams ().SetInt ("index", k);
		ValueList out, direct;
		ASSERT_TRUE (evaluator.Evaluate (select, out, ctx).IsOk ()) << "index " << k;
		ASSERT_TRUE (evaluator.Evaluate (branches[k], direct, ctx).IsOk ());

		std::shared_ptr<const Mesh> chosen = out[0].Share<Mesh> (Types ().mesh);
		std::shared_ptr<const Mesh> expected = direct[0].Share<Mesh> (Types ().mesh);
		ASSERT_NE (chosen, nullptr);
		ASSERT_NE (expected, nullptr);

		// REPARTAGE et non copie : le selecteur ne fabrique rien.
		EXPECT_EQ (chosen.get (), expected.get ()) << "index " << k;

		float lo = 0.f, hi = 0.f;
		zRange (*chosen, lo, hi);
		EXPECT_NEAR (hi, depths[k], 1e-4f) << "index " << k;
	}
}

TEST (TEST_cggraph_nodes_stack, an_index_pointing_at_an_unfed_port_is_refused)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "H", 10.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId only = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, only, 0), ConnectStatus::Ok);
	const NodeId select = graph.AddNode (MakeNode ("flow.select.mesh"));
	ASSERT_EQ (graph.Connect (only, 0, select, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;

	ValueList ok;
	graph.FindNode (select)->GetParams ().SetInt ("index", 0);
	ASSERT_TRUE (evaluator.Evaluate (select, ok, ctx).IsOk ());

	// Un index qui designe un port VIDE est une erreur de graphe, pas un defaut
	// a combler en silence par la premiere entree.
	ValueList bad;
	graph.FindNode (select)->GetParams ().SetInt ("index", 1);
	EXPECT_FALSE (evaluator.Evaluate (select, bad, ctx).IsOk ());
}

TEST (TEST_cggraph_nodes_stack, an_out_of_range_index_is_clamped_to_the_last_branch)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "H", 10.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	NodeId branches[3];
	for (int k = 0; k < 3; ++k)
	{
		branches[k] = graph.AddNode (MakeNode ("shape.extrude"));
		ASSERT_EQ (graph.Connect (contours, 0, branches[k], 0), ConnectStatus::Ok);
		graph.FindNode (branches[k])->GetParams ().SetFloat ("depth", 1.0f + (float)k);
	}
	const NodeId select = graph.AddNode (MakeNode ("flow.select.mesh"));
	for (int k = 0; k < 3; ++k)
		ASSERT_EQ (graph.Connect (branches[k], 0, select, k), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList high, low, last;
	graph.FindNode (select)->GetParams ().SetInt ("index", 99);
	ASSERT_TRUE (evaluator.Evaluate (select, high, ctx).IsOk ());
	graph.FindNode (select)->GetParams ().SetInt ("index", -7);
	ASSERT_TRUE (evaluator.Evaluate (select, low, ctx).IsOk ());
	graph.FindNode (select)->GetParams ().SetInt ("index", 2);
	ASSERT_TRUE (evaluator.Evaluate (select, last, ctx).IsOk ());

	float h = 0.f, l = 0.f, e = 0.f, dummy = 0.f;
	zRange (*high[0].Share<Mesh> (Types ().mesh), dummy, h);
	zRange (*low[0].Share<Mesh> (Types ().mesh), dummy, l);
	zRange (*last[0].Share<Mesh> (Types ().mesh), dummy, e);
	EXPECT_NEAR (h, e, 1e-4f);      // 99 -> la derniere
	EXPECT_NEAR (l, 1.0f, 1e-4f);   // -7 -> la premiere
}

TEST (TEST_cggraph_nodes_stack, the_selector_does_not_shield_a_failing_unselected_branch)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "H", 10.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId good = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, good, 0), ConnectStatus::Ok);

	// Une branche qui echoue : un offset qui consomme toute la matiere.
	const NodeId doomedOffset = graph.AddNode (MakeNode ("shape.contours.offset"));
	ASSERT_EQ (graph.Connect (contours, 0, doomedOffset, 0), ConnectStatus::Ok);
	graph.FindNode (doomedOffset)->GetParams ().SetFloat ("delta", -50.0f);
	const NodeId doomed = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (doomedOffset, 0, doomed, 0), ConnectStatus::Ok);

	const NodeId select = graph.AddNode (MakeNode ("flow.select.mesh"));
	ASSERT_EQ (graph.Connect (good, 0, select, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (doomed, 0, select, 1), ConnectStatus::Ok);
	graph.FindNode (select)->GetParams ().SetInt ("index", 0);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList out;
	// LA RESERVE, TENUE PAR UN CAS : l'evaluateur tire ses entrees AVANT
	// Compute, donc la branche 1 est calculee meme si l'index designe la 0. Le
	// selecteur choisit un resultat, il ne rattrape pas une panne -- et un
	// gabarit doit borner ses branches en consequence.
	EXPECT_FALSE (evaluator.Evaluate (select, out, ctx).IsOk ())
		<< "si ce cas passe, la reserve inscrite dans flow/select.h est fausse "
		   "et l'evaluation est devenue paresseuse";
}

TEST (TEST_cggraph_nodes_stack, the_contours_selector_switches_the_plate_form)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "HI", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId rect = graph.AddNode (MakeNode ("shape.contours.plate"));
	ASSERT_EQ (graph.Connect (contours, 0, rect, 0), ConnectStatus::Ok);
	graph.FindNode (rect)->GetParams ().SetFloat ("margin", 3.0f);

	const NodeId halo = graph.AddNode (MakeNode ("shape.contours.offset"));
	ASSERT_EQ (graph.Connect (contours, 0, halo, 0), ConnectStatus::Ok);
	graph.FindNode (halo)->GetParams ().SetFloat ("delta", 3.0f);

	const NodeId select = graph.AddNode (MakeNode ("flow.select.contours"));
	ASSERT_EQ (graph.Connect (rect, 0, select, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (halo, 0, select, 1), ConnectStatus::Ok);

	// Et un maillage ne se branche PAS sur un selecteur de contours : les deux
	// variantes de la famille ne sont pas interchangeables.
	const NodeId solid = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (select, 0, solid, 0), ConnectStatus::Ok);
	EXPECT_EQ (graph.Connect (solid, 0, select, 2), ConnectStatus::TypeMismatch);

	Evaluator evaluator (graph);
	EvalContext ctx;

	graph.FindNode (select)->GetParams ().SetInt ("index", 0);
	ValueList asRect;
	ASSERT_TRUE (evaluator.Evaluate (select, asRect, ctx).IsOk ());
	graph.FindNode (select)->GetParams ().SetInt ("index", 1);
	ValueList asHalo;
	ASSERT_TRUE (evaluator.Evaluate (select, asHalo, ctx).IsOk ());

	std::shared_ptr<const Contours> r = asRect[0].Share<Contours> (Types ().extrudeContours);
	std::shared_ptr<const Contours> h = asHalo[0].Share<Contours> (Types ().extrudeContours);
	ASSERT_NE (r, nullptr);
	ASSERT_NE (h, nullptr);
	// Le rectangle est UN contour ; la silhouette en porte plus, et remplit moins.
	EXPECT_EQ (r->size (), 1u);
	EXPECT_LT (totalAbsArea (*h), totalAbsArea (*r));
}

// ---------------------------------------------------------------------------
//  L'ARETE PROFILEE, et le selecteur qui en choisit la forme
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_stack, a_bar_profile_cannot_feed_the_profiled_extruder)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "H", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId profiled = graph.AddNode (MakeNode ("shape.extrude.profiled"));
	ASSERT_EQ (graph.Connect (contours, 0, profiled, 0), ConnectStatus::Ok);

	// EBRASEMENT sur le port de profil, et rien d'autre : une section de BARRE
	// est une boucle fermee, elle se lirait de travers et rendrait une piece
	// repliee que rien ne signalerait. Le refus est a la CONNEXION.
	const NodeId bar = graph.AddNode (MakeNode ("profile.bar.roll"));
	EXPECT_EQ (graph.Connect (bar, 0, profiled, 1), ConnectStatus::TypeMismatch);

	const NodeId chamfer = graph.AddNode (MakeNode ("profile.chamfer"));
	EXPECT_EQ (graph.Connect (chamfer, 0, profiled, 1), ConnectStatus::Ok);
}

TEST (TEST_cggraph_nodes_stack, the_edge_form_is_chosen_by_a_profile_selector)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "H", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	// Les deux producteurs que la page offrira : chanfrein (ou biseau, c'est le
	// meme a largeur != profondeur) et conge.
	const NodeId chamfer = graph.AddNode (MakeNode ("profile.chamfer"));
	graph.FindNode (chamfer)->GetParams ().SetFloat ("width", 1.0f);
	graph.FindNode (chamfer)->GetParams ().SetFloat ("depth", 1.0f);

	const NodeId cavetto = graph.AddNode (MakeNode ("profile.cavetto"));
	graph.FindNode (cavetto)->GetParams ().SetFloat ("width", 1.0f);
	graph.FindNode (cavetto)->GetParams ().SetFloat ("depth", 1.0f);

	const NodeId pick = graph.AddNode (MakeNode ("flow.select.profile"));
	ASSERT_EQ (graph.Connect (chamfer, 0, pick, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (cavetto, 0, pick, 1), ConnectStatus::Ok);

	const NodeId solid = graph.AddNode (MakeNode ("shape.extrude.profiled"));
	ASSERT_EQ (graph.Connect (contours, 0, solid, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (pick, 0, solid, 1), ConnectStatus::Ok);
	graph.FindNode (solid)->GetParams ().SetFloat ("depth", 4.0f);
	graph.FindNode (solid)->GetParams ().SetFloat ("zBottom", 2.0f);

	Evaluator evaluator (graph);
	EvalContext ctx;

	graph.FindNode (pick)->GetParams ().SetInt ("index", 0);
	ValueList asChamfer;
	ASSERT_TRUE (evaluator.Evaluate (solid, asChamfer, ctx).IsOk ());
	graph.FindNode (pick)->GetParams ().SetInt ("index", 1);
	ValueList asCavetto;
	ASSERT_TRUE (evaluator.Evaluate (solid, asCavetto, ctx).IsOk ());

	std::shared_ptr<const Mesh> a = asChamfer[0].Share<Mesh> (Types ().mesh);
	std::shared_ptr<const Mesh> b = asCavetto[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (a, nullptr);
	ASSERT_NE (b, nullptr);

	// Le conge courbe l'arete : il ne peut pas rendre le meme maillage que la
	// coupe droite. Sans cette verification, le selecteur pourrait rester bloque
	// sur une branche sans que rien ne le dise.
	EXPECT_NE (a->GetNVertices (), b->GetNVertices ());

	// Et la piece est bien posee ou `zBottom` le dit : le profil ne deplace pas
	// le solide, il n'en taille que l'arete du dessus.
	float lo = 0.f, hi = 0.f;
	zRange (*a, lo, hi);
	EXPECT_NEAR (lo, 2.0f, 1e-4f);
	EXPECT_NEAR (hi, 6.0f, 1e-4f);
}

TEST (TEST_cggraph_nodes_stack, a_zero_width_profile_gives_the_plain_extrusion_back)
{
	// C'est ainsi qu'une page a graphe FIXE offre « arete vive » : elle met la
	// largeur a zero, au lieu de debrancher un noeud -- ce qu'elle ne sait pas
	// faire. L'emprise doit alors etre exactement celle de l'extrusion droite.
	Graph graph;
	const NodeId contours = AddTextContours (graph, "HI", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId flat = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, flat, 0), ConnectStatus::Ok);
	graph.FindNode (flat)->GetParams ().SetFloat ("depth", 3.0f);

	const NodeId sharp = graph.AddNode (MakeNode ("profile.chamfer"));
	graph.FindNode (sharp)->GetParams ().SetFloat ("width", 0.0f);
	graph.FindNode (sharp)->GetParams ().SetFloat ("depth", 1.0f);

	const NodeId profiled = graph.AddNode (MakeNode ("shape.extrude.profiled"));
	ASSERT_EQ (graph.Connect (contours, 0, profiled, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (sharp, 0, profiled, 1), ConnectStatus::Ok);
	graph.FindNode (profiled)->GetParams ().SetFloat ("depth", 3.0f);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList straight, edged;
	ASSERT_TRUE (evaluator.Evaluate (flat, straight, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (profiled, edged, ctx).IsOk ());

	std::shared_ptr<const Mesh> s = straight[0].Share<Mesh> (Types ().mesh);
	std::shared_ptr<const Mesh> e = edged[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (s, nullptr);
	ASSERT_NE (e, nullptr);

	float slo = 0.f, shi = 0.f, elo = 0.f, ehi = 0.f;
	zRange (*s, slo, shi);
	zRange (*e, elo, ehi);
	EXPECT_NEAR (slo, elo, 1e-4f);
	EXPECT_NEAR (shi, ehi, 1e-4f);
}

// ---------------------------------------------------------------------------
//  LE BOOLEEN 2D : composer la region avant de l'extruder
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_stack, the_boolean_node_cuts_the_text_out_of_its_plate)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "HI", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId plate = graph.AddNode (MakeNode ("shape.contours.plate"));
	ASSERT_EQ (graph.Connect (contours, 0, plate, 0), ConnectStatus::Ok);
	graph.FindNode (plate)->GetParams ().SetFloat ("margin", 5.0f);

	// « plaque MOINS texte » : le capot d'un socle grave, ou celui d'un socle a
	// coque unique. C'est l'usage qui justifie ce noeud.
	const NodeId cut = graph.AddNode (MakeNode ("shape.boolean2d"));
	ASSERT_EQ (graph.Connect (plate, 0, cut, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (contours, 0, cut, 1), ConnectStatus::Ok);
	graph.FindNode (cut)->GetParams ().SetInt ("op", 1);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList plain, holed, text;
	ASSERT_TRUE (evaluator.Evaluate (plate, plain, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (cut, holed, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (contours, text, ctx).IsOk ());

	std::shared_ptr<const Contours> p = plain[0].Share<Contours> (Types ().extrudeContours);
	std::shared_ptr<const Contours> h = holed[0].Share<Contours> (Types ().extrudeContours);
	std::shared_ptr<const Contours> t = text[0].Share<Contours> (Types ().extrudeContours);
	ASSERT_NE (p, nullptr);
	ASSERT_NE (h, nullptr);
	ASSERT_NE (t, nullptr);

	// La plaque est UN rectangle ; percee, elle porte en plus les contours des
	// lettres, devenus des trous.
	ASSERT_EQ (p->size (), 1u);
	EXPECT_GT (h->size (), p->size ());

	// L'emprise ne bouge pas -- on a retire de la matiere, pas deplace le bord.
	float px0 = 0.f, py0 = 0.f, px1 = 0.f, py1 = 0.f;
	float hx0 = 0.f, hy0 = 0.f, hx1 = 0.f, hy1 = 0.f;
	ASSERT_TRUE (contoursBBox (*p, px0, py0, px1, py1));
	ASSERT_TRUE (contoursBBox (*h, hx0, hy0, hx1, hy1));
	EXPECT_NEAR (px1 - px0, hx1 - hx0, 1e-3f);
	EXPECT_NEAR (py1 - py0, hy1 - hy0, 1e-3f);

	// Et un contour au moins tourne A L'ENVERS du rectangle : c'est ce qui fait
	// d'un contour un TROU au sens NonZero, donc ce qui distingue un socle grave
	// d'un socle sur lequel on aurait pose des lettres.
	const float plateSign = contourSignedArea ((*p)[0].pts) >= 0.f ? 1.f : -1.f;
	bool anyHole = false;
	for (const ExtrudeContour &c : *h)
		if (contourSignedArea (c.pts) * plateSign < 0.f) anyHole = true;
	EXPECT_TRUE (anyHole) << "aucun contour ne tourne a l'envers : rien n'est perce";
}

TEST (TEST_cggraph_nodes_stack, the_boolean_node_reads_its_ports_in_order)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "HI", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId plate = graph.AddNode (MakeNode ("shape.contours.plate"));
	ASSERT_EQ (graph.Connect (contours, 0, plate, 0), ConnectStatus::Ok);
	graph.FindNode (plate)->GetParams ().SetFloat ("margin", 5.0f);

	// A = plaque, B = texte : il reste la plaque percee.
	const NodeId cut = graph.AddNode (MakeNode ("shape.boolean2d"));
	ASSERT_EQ (graph.Connect (plate, 0, cut, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (contours, 0, cut, 1), ConnectStatus::Ok);
	graph.FindNode (cut)->GetParams ().SetInt ("op", 1);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList out;
	ASSERT_TRUE (evaluator.Evaluate (cut, out, ctx).IsOk ());

	// A = texte, B = plaque : le texte est ENTIEREMENT dans la plaque, il ne
	// reste rien. Le noeud refuse -- et c'est ainsi qu'on sait que l'ordre des
	// ports est lu, et non deux entrees interchangeables.
	ASSERT_EQ (graph.Disconnect (cut, 0), true);
	ASSERT_EQ (graph.Disconnect (cut, 1), true);
	ASSERT_EQ (graph.Connect (contours, 0, cut, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (plate, 0, cut, 1), ConnectStatus::Ok);

	ValueList reversed;
	EXPECT_FALSE (evaluator.Evaluate (cut, reversed, ctx).IsOk ())
		<< "le texte moins la plaque qui le contient devrait ne rien laisser";
}

TEST (TEST_cggraph_nodes_stack, the_boolean_node_unions_and_intersects_too)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "HI", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId plate = graph.AddNode (MakeNode ("shape.contours.plate"));
	ASSERT_EQ (graph.Connect (contours, 0, plate, 0), ConnectStatus::Ok);
	graph.FindNode (plate)->GetParams ().SetFloat ("margin", 5.0f);

	const NodeId op = graph.AddNode (MakeNode ("shape.boolean2d"));
	ASSERT_EQ (graph.Connect (plate, 0, op, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (contours, 0, op, 1), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;

	// Reunion : le texte etant dans la plaque, la reunion EST la plaque.
	graph.FindNode (op)->GetParams ().SetInt ("op", 0);
	ValueList merged;
	ASSERT_TRUE (evaluator.Evaluate (op, merged, ctx).IsOk ());
	std::shared_ptr<const Contours> u = merged[0].Share<Contours> (Types ().extrudeContours);
	ASSERT_NE (u, nullptr);
	EXPECT_EQ (u->size (), 1u);

	// Intersection : la part commune, donc le TEXTE lui-meme.
	graph.FindNode (op)->GetParams ().SetInt ("op", 2);
	ValueList common;
	ASSERT_TRUE (evaluator.Evaluate (op, common, ctx).IsOk ());
	std::shared_ptr<const Contours> i = common[0].Share<Contours> (Types ().extrudeContours);
	ASSERT_NE (i, nullptr);
	EXPECT_LT (totalAbsArea (*i), totalAbsArea (*u))
		<< "l'intersection devrait etre strictement plus petite que la reunion";

	// Un op hors intervalle est BORNE, pas rejete en silence sur le premier.
	graph.FindNode (op)->GetParams ().SetInt ("op", 99);
	ValueList clamped;
	ASSERT_TRUE (evaluator.Evaluate (op, clamped, ctx).IsOk ());
	std::shared_ptr<const Contours> c = clamped[0].Share<Contours> (Types ().extrudeContours);
	ASSERT_NE (c, nullptr);
	EXPECT_NEAR (totalAbsArea (*c), totalAbsArea (*i), 1e-2f) << "99 doit valoir 2";
}

// ---------------------------------------------------------------------------
//  LA COULEUR, donnee du document
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_stack, the_color_node_paints_the_faces_nobody_painted)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "H", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId solid = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, solid, 0), ConnectStatus::Ok);

	const NodeId paint = graph.AddNode (MakeNode ("mesh.color"));
	ASSERT_EQ (graph.Connect (solid, 0, paint, 0), ConnectStatus::Ok);
	graph.FindNode (paint)->GetParams ().SetString ("color", "#20c040");

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList bare, painted;
	ASSERT_TRUE (evaluator.Evaluate (solid, bare, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (paint, painted, ctx).IsOk ());

	std::shared_ptr<const Mesh> before = bare[0].Share<Mesh> (Types ().mesh);
	std::shared_ptr<const Mesh> after = painted[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (before, nullptr);
	ASSERT_NE (after, nullptr);

	// La GEOMETRIE ne bouge pas : peindre n'est pas deformer.
	EXPECT_EQ (after->GetNVertices (), before->GetNVertices ());
	EXPECT_EQ (after->GetNFaces (), before->GetNFaces ());

	// Avant, aucune face ne porte de materiau ; apres, toutes.
	unsigned int bareNone = 0, afterNone = 0;
	for (unsigned int f = 0; f < before->GetNFaces (); ++f)
		if (before->GetFaceMaterialId (f) < 0) bareNone++;
	for (unsigned int f = 0; f < after->GetNFaces (); ++f)
		if (after->GetFaceMaterialId (f) < 0) afterNone++;
	EXPECT_EQ (bareNone, before->GetNFaces ());
	EXPECT_EQ (afterNone, 0u);

	// Et le noeud le DIT, au lieu de le laisser supposer.
	ColorMeshNode *node = static_cast<ColorMeshNode *> (graph.FindNode (paint));
	EXPECT_EQ (node->GetPaintedFaces (), after->GetNFaces ());
	EXPECT_EQ (node->GetKeptFaces (), 0u);
}

TEST (TEST_cggraph_nodes_stack, the_color_node_leaves_painted_faces_alone)
{
	// Deux couleurs en cascade : la seconde ne doit RIEN repeindre. C'est la
	// regle qui garde sa palette a un relief colore branche plus loin -- et elle
	// se verifie sans relief, en chainant le noeud sur lui-meme.
	Graph graph;
	const NodeId contours = AddTextContours (graph, "H", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId solid = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, solid, 0), ConnectStatus::Ok);
	const NodeId first = graph.AddNode (MakeNode ("mesh.color"));
	ASSERT_EQ (graph.Connect (solid, 0, first, 0), ConnectStatus::Ok);
	graph.FindNode (first)->GetParams ().SetString ("color", "#20c040");
	const NodeId second = graph.AddNode (MakeNode ("mesh.color"));
	ASSERT_EQ (graph.Connect (first, 0, second, 0), ConnectStatus::Ok);
	graph.FindNode (second)->GetParams ().SetString ("color", "#c04020");

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList out;
	ASSERT_TRUE (evaluator.Evaluate (second, out, ctx).IsOk ());

	ColorMeshNode *node = static_cast<ColorMeshNode *> (graph.FindNode (second));
	EXPECT_EQ (node->GetPaintedFaces (), 0u)
		<< "la seconde couleur a repeint des faces deja peintes";
	EXPECT_GT (node->GetKeptFaces (), 0u);
}

TEST (TEST_cggraph_nodes_stack, a_colour_that_is_not_a_colour_is_refused)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "H", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId solid = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, solid, 0), ConnectStatus::Ok);
	const NodeId paint = graph.AddNode (MakeNode ("mesh.color"));
	ASSERT_EQ (graph.Connect (solid, 0, paint, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;

	// Un texte qui n'est pas « #rrggbb » est REFUSE, pas remplace par un gris :
	// une couleur de secours laisserait croire que le reglage a ete pris.
	for (const char *bad : { "bleu", "#fff", "b4bec8", "#gggggg", "" })
	{
		graph.FindNode (paint)->GetParams ().SetString ("color", bad);
		ValueList out;
		EXPECT_FALSE (evaluator.Evaluate (paint, out, ctx).IsOk ())
			<< "« " << bad << " » a ete accepte comme couleur";
	}

	// Et la forme valide passe, sans quoi le refus ci-dessus ne prouverait rien.
	graph.FindNode (paint)->GetParams ().SetString ("color", "#B4BEC8");
	ValueList ok;
	EXPECT_TRUE (evaluator.Evaluate (paint, ok, ctx).IsOk ())
		<< "les majuscules hexadecimales devraient etre acceptees";
}

// ===========================================================================
//  LA FIXATION MURALE : un bandeau, deux oreilles percees, a la toute fin
// ===========================================================================
//
// Ce que ces cas gardent tient en trois phrases. La ligne de fixation est
// IMPOSEE, jamais deduite de la forme -- sans quoi un modele bas a gauche et
// haut a droite pendrait de travers. Les trous sont REELLEMENT perces, ce qui
// se mesure au volume et pas a l'oeil. Et le noeud REFUSE les proportions qui
// ne donneraient pas une fixation, plutot que de rendre une piece qu'on ne
// decouvrirait qu'apres impression.

namespace {

// Volume signe par le theoreme de la divergence. ADDITIF sur des coques fermees
// meme quand elles s'interpenetrent -- c'est ce qui permet de peser la fixation
// seule, par difference, sans la separer du reste.
double meshSignedVolume (const Mesh &mesh)
{
	Mesh &m = const_cast<Mesh &> (mesh);
	double vol = 0.0;
	for (unsigned int f = 0; f < m.GetNFaces (); ++f)
	{
		if (m.GetFaceNVertices (f) != 3) continue;
		float p[3][3];
		for (int k = 0; k < 3; ++k)
			m.GetVertex ((unsigned int)m.GetFaceVertex (f, k), p[k]);
		vol += ((double)p[0][0] * ((double)p[1][1] * p[2][2] - (double)p[2][1] * p[1][2])
		      - (double)p[1][0] * ((double)p[0][1] * p[2][2] - (double)p[2][1] * p[0][2])
		      + (double)p[2][0] * ((double)p[0][1] * p[1][2] - (double)p[1][1] * p[0][2])) / 6.0;
	}
	return vol;
}

// Aire du POLYGONE a n cotes inscrit dans le cercle -- pas celle du cercle :
// circleContour echantillonne, et comparer a pi*R^2 ferait echouer un test juste
// de 0,6 % a 32 segments.
double inscribedPolygonArea (double radius, int segments)
{
	const double PI = 3.14159265358979323846;
	return 0.5 * segments * radius * radius * std::sin (2.0 * PI / segments);
}

void xyRange (const Mesh &mesh, float &x0, float &x1, float &y0, float &y1)
{
	Mesh &m = const_cast<Mesh &> (mesh);
	x0 = y0 = 1e30f; x1 = y1 = -1e30f;
	for (unsigned int v = 0; v < m.GetNVertices (); ++v)
	{
		float p[3];
		m.GetVertex (v, p);
		x0 = std::min (x0, p[0]); x1 = std::max (x1, p[0]);
		y0 = std::min (y0, p[1]); y1 = std::max (y1, p[1]);
	}
}

}  // namespace

TEST (TEST_cggraph_nodes_stack, the_mounts_widen_the_piece_and_announce_their_hole_spacing)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "Hip", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId solid = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, solid, 0), ConnectStatus::Ok);
	const NodeId mounts = graph.AddNode (MakeNode ("mesh.mounts"));
	ASSERT_EQ (graph.Connect (solid, 0, mounts, 0), ConnectStatus::Ok);

	Node *node = graph.FindNode (mounts);
	node->GetParams ().SetFloat ("earDiameter", 10.0f);
	node->GetParams ().SetFloat ("overhang", 8.0f);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList bare, fixed;
	ASSERT_TRUE (evaluator.Evaluate (solid, bare, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (mounts, fixed, ctx).IsOk ());

	std::shared_ptr<const Mesh> before = bare[0].Share<Mesh> (Types ().mesh);
	std::shared_ptr<const Mesh> after = fixed[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (before, nullptr);
	ASSERT_NE (after, nullptr);

	float bx0, bx1, by0, by1, ax0, ax1, ay0, ay1;
	xyRange (*before, bx0, bx1, by0, by1);
	xyRange (*after, ax0, ax1, ay0, ay1);

	// Le DEBORD est ce qu'il dit : la piece s'elargit d'exactement autant de
	// chaque cote, ni du diametre de l'oreille ni d'autre chose.
	EXPECT_NEAR (ax0, bx0 - 8.0f, 1e-3f);
	EXPECT_NEAR (ax1, bx1 + 8.0f, 1e-3f);

	// L'ENTRAXE tombe de l'emprise et du debord ; il ne se regle pas, donc le
	// noeud le publie -- c'est le seul chiffre dont on ait besoin devant le mur.
	MountsNode *mountsNode = static_cast<MountsNode *> (node);
	EXPECT_NEAR (mountsNode->GetHoleSpacing (), (bx1 - bx0) + 2.f * 8.0f - 10.0f, 1e-3f);

	std::vector<cggraph::NodeStat> stats;
	node->PublishStats (stats);
	ASSERT_EQ (stats.size (), 1u);
	EXPECT_EQ (stats[0].name, "holeSpacing");
	EXPECT_NEAR (stats[0].value, (double)mountsNode->GetHoleSpacing (), 1e-3);
}

TEST (TEST_cggraph_nodes_stack, the_mounting_line_is_imposed_and_not_read_off_the_shape)
{
	Graph graph;
	// « pd » : une lettre qui descend sous la ligne de base et une qui monte, donc
	// une forme dont les deux bouts ne sont PAS a la meme hauteur. C'est le cas
	// qui condamne les deux pastilles posees « aux extremites ».
	const NodeId contours = AddTextContours (graph, "pd", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId solid = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, solid, 0), ConnectStatus::Ok);
	const NodeId mounts = graph.AddNode (MakeNode ("mesh.mounts"));
	ASSERT_EQ (graph.Connect (solid, 0, mounts, 0), ConnectStatus::Ok);

	Node *node = graph.FindNode (mounts);
	node->GetParams ().SetFloat ("earDiameter", 10.0f);
	node->GetParams ().SetFloat ("bandWidth", 4.0f);
	node->GetParams ().SetFloat ("overhang", 8.0f);
	node->GetParams ().SetFloat ("thickness", 1.5f);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList bare;
	ASSERT_TRUE (evaluator.Evaluate (solid, bare, ctx).IsOk ());
	std::shared_ptr<const Mesh> before = bare[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (before, nullptr);
	float bx0, bx1, by0, by1;
	xyRange (*before, bx0, bx1, by0, by1);
	float bzLo, bzHi;
	zRange (*before, bzLo, bzHi);

	// La fixation est CONCATENEE au maillage d'entree : ses sommets sont donc
	// exactement ceux dont l'indice depasse le compte d'origine. C'est le seul
	// critere exact -- un critere geometrique (« a gauche de la piece ») ne tient
	// plus des lors que l'etendue est mesuree dans la tranche : a la fraction 1
	// du « pd », la matiere du haut est le seul montant du d, et l'oreille gauche
	// se retrouve loin a DROITE du bord gauche de la piece.
	(void)bx0; (void)bx1;
	const unsigned int inputVertices = const_cast<Mesh &> (*before).GetNVertices ();

	for (const float fraction : { 0.0f, 0.5f, 1.0f })
	{
		node->GetParams ().SetFloat ("lineHeight", fraction);
		ValueList fixed;
		ASSERT_TRUE (evaluator.Evaluate (mounts, fixed, ctx).IsOk ());
		std::shared_ptr<const Mesh> after = fixed[0].Share<Mesh> (Types ().mesh);
		ASSERT_NE (after, nullptr);

		Mesh &m = const_cast<Mesh &> (*after);
		ASSERT_GT (m.GetNVertices (), inputVertices);
		float earLo = 1e30f, earHi = -1e30f, zLo = 1e30f, zHi = -1e30f;
		for (unsigned int v = inputVertices; v < m.GetNVertices (); ++v)
		{
			float p[3];
			m.GetVertex (v, p);
			earLo = std::min (earLo, p[1]); earHi = std::max (earHi, p[1]);
			zLo = std::min (zLo, p[2]); zHi = std::max (zHi, p[2]);
		}

		// La ligne est a la FRACTION demandee de l'emprise, quoi que fasse la
		// forme entre les deux bouts. Les oreilles debordent de leur rayon de part
		// et d'autre, symetriquement : leur milieu EST la ligne.
		const float expected = by0 + fraction * (by1 - by0);
		EXPECT_NEAR (0.5f * (earLo + earHi), expected, 1e-2f)
			<< "fraction " << fraction;
		EXPECT_NEAR (earHi - earLo, 10.0f, 1e-2f) << "fraction " << fraction;

		// Et en Z, elle part du BAS de la piece et monte de son epaisseur : c'est
		// ce qui la fait mordre dans le socle quand il y en a un.
		EXPECT_NEAR (zLo, bzLo, 1e-3f);
		EXPECT_NEAR (zHi, bzLo + 1.5f, 1e-3f);
	}
}

TEST (TEST_cggraph_nodes_stack, the_holes_are_really_pierced_and_the_volume_says_so)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "Hip", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId solid = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, solid, 0), ConnectStatus::Ok);
	const NodeId mounts = graph.AddNode (MakeNode ("mesh.mounts"));
	ASSERT_EQ (graph.Connect (solid, 0, mounts, 0), ConnectStatus::Ok);

	// PROPORTIONS DE MESURE, pas d'usage : le bandeau est plus HAUT que l'oreille
	// n'est large (12 > 2 x 5), donc chaque disque est entierement dans la bande
	// en y et le recouvrement vaut exactement une moitie de disque. L'aire
	// attendue devient calculable a la main, ce qui n'est pas le cas autrement.
	const float earR = 5.0f, holeR = 2.0f, band = 12.0f, overhang = 8.0f, thickness = 2.0f;
	Node *node = graph.FindNode (mounts);
	node->GetParams ().SetFloat ("earDiameter", 2.f * earR);
	node->GetParams ().SetFloat ("holeDiameter", 2.f * holeR);
	node->GetParams ().SetFloat ("bandWidth", band);
	node->GetParams ().SetFloat ("overhang", overhang);
	node->GetParams ().SetFloat ("thickness", thickness);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList bare, fixed;
	ASSERT_TRUE (evaluator.Evaluate (solid, bare, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (mounts, fixed, ctx).IsOk ());
	std::shared_ptr<const Mesh> before = bare[0].Share<Mesh> (Types ().mesh);
	std::shared_ptr<const Mesh> after = fixed[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (before, nullptr);
	ASSERT_NE (after, nullptr);

	float bx0, bx1, by0, by1;
	xyRange (*before, bx0, bx1, by0, by1);
	const double span = (double)(bx1 - bx0) + 2.0 * overhang - 2.0 * earR;

	// Aire = le bandeau, plus les deux moities d'oreille qui en depassent (soit
	// un disque entier), moins les deux trous.
	const double area = span * band
	                  + inscribedPolygonArea (earR, 32)
	                  - 2.0 * inscribedPolygonArea (holeR, 32);

	// Le volume signe est ADDITIF sur des coques fermees, meme imbriquees : la
	// difference pese la fixation seule.
	const double added = meshSignedVolume (*after) - meshSignedVolume (*before);
	EXPECT_NEAR (added, area * thickness, 0.005 * area * thickness);

	// Et sans les trous, le volume serait plus grand de leur matiere : la mesure
	// ci-dessus les DISTINGUE d'un simple cercle grave.
	const double unpierced = (area + 2.0 * inscribedPolygonArea (holeR, 32)) * thickness;
	EXPECT_LT (added, unpierced - 0.5 * inscribedPolygonArea (holeR, 32) * thickness);
}

TEST (TEST_cggraph_nodes_stack, proportions_that_would_not_give_a_fixture_are_refused)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "Hip", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId solid = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, solid, 0), ConnectStatus::Ok);
	const NodeId mounts = graph.AddNode (MakeNode ("mesh.mounts"));
	ASSERT_EQ (graph.Connect (solid, 0, mounts, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	Node *node = graph.FindNode (mounts);

	// Les valeurs par defaut passent, sans quoi les refus ci-dessous ne
	// prouveraient rien.
	{
		ValueList out;
		ASSERT_TRUE (evaluator.Evaluate (mounts, out, ctx).IsOk ());
	}

	struct Case { const char *param; float value; const char *why; };
	const Case cases[] = {
		{ "holeDiameter", 12.0f, "un trou aussi large que l'oreille n'est plus une oreille" },
		{ "thickness", 0.0f, "une fixation sans epaisseur n'existe pas" },
		{ "bandWidth", 0.0f, "un bandeau sans hauteur ne relie rien" },
		// Debord si negatif que les deux centres se croisent : la piece sortirait
		// avec ses deux trous du mauvais cote l'un de l'autre.
		{ "overhang", -500.0f, "les deux oreilles se croisent" }
	};
	for (const Case &c : cases)
	{
		const cggraph::ParamValue *kept = node->GetParams ().Find (c.param);
		ASSERT_NE (kept, nullptr) << c.param;
		const float restore = kept->floatValue;

		node->GetParams ().SetFloat (c.param, c.value);
		ValueList out;
		EXPECT_FALSE (evaluator.Evaluate (mounts, out, ctx).IsOk ()) << c.why;
		node->GetParams ().SetFloat (c.param, restore);
	}
}

// ===========================================================================
//  LA FRAISURE : une bague qui REMET la matiere, faute de booleen 3D
// ===========================================================================
//
// La piece fraisee se construit a l'envers : la plaque est percee au diametre
// de la BOUCHE, de part en part, et la bague remet ce qu'il ne fallait pas
// retirer. Ce que ces cas gardent est donc, avant tout, que la bague est bien
// un AJOUT -- une bague retournee se soustrairait de la piece, et rien a
// l'ecran ne le dirait -- et que le creux final est un CONE et non un lamage.

namespace {

// Aire du polygone a n cotes inscrit dans le cercle de rayon r. La bague est
// echantillonnee, donc ce n'est pas pi r^2 qu'il faut : a 32 segments l'ecart
// est de 0,6 %, largement de quoi faire echouer une comparaison juste.
double inscribedArea (double r, int n)
{
	const double PI = 3.14159265358979323846;
	return 0.5 * n * std::sin (2.0 * PI / n) * r * r;
}

double cylinderVolume (double radius, double h, int n)
{
	return inscribedArea (radius, n) * h;
}

// Tronc de cone dont le rayon va lineairement de rLow a rHigh sur la hauteur h :
// l'integrale de r(z)^2 vaut h (a^2 + ab + b^2) / 3.
double frustumVolume (double rLow, double rHigh, double h, int n)
{
	const double PI = 3.14159265358979323846;
	const double k = 0.5 * n * std::sin (2.0 * PI / n);
	return k * h * (rLow * rLow + rLow * rHigh + rHigh * rHigh) / 3.0;
}

}  // namespace

TEST (TEST_cggraph_nodes_stack, the_countersink_collar_is_a_closed_solid_of_the_volume_expected)
{
	const int n = 32;
	CountersinkCollarOptions opt;
	opt.holeRadius = 2.25f;
	opt.mouthRadius = 4.0f;
	opt.zBottom = 0.0f;
	opt.zTop = 3.0f;
	opt.coneDepth = 1.75f;
	opt.segments = n;
	opt.bite = 0.05f;

	std::unique_ptr<Mesh> collar (countersinkCollar (opt));
	ASSERT_NE (collar, nullptr);

	const double outer = opt.mouthRadius + opt.bite;
	const double straight = opt.zTop - opt.coneDepth - opt.zBottom;
	const double expected =
		// la portee cylindrique : l'anneau plein entre le trou et le bord exterieur
		cylinderVolume (outer, straight, n) - cylinderVolume (opt.holeRadius, straight, n)
		// puis la zone conique : le meme cylindre, moins le tronc qui s'evase
		+ cylinderVolume (outer, opt.coneDepth, n)
		- frustumVolume (opt.holeRadius, opt.mouthRadius, opt.coneDepth, n);

	// Le volume signe MESURE l'orientation en meme temps que le volume : negatif,
	// la bague creuserait la piece au lieu de la remplir.
	const double measured = meshSignedVolume (*collar);
	EXPECT_GT (measured, 0.0) << "la bague est retournee : elle se soustrairait";
	EXPECT_NEAR (measured, expected, 1e-3 * expected);

	// Peau FERMEE : chaque arete exactement deux fois, sans quoi le STL n'est pas
	// imprimable. Cinq parois de deux triangles par segment.
	EXPECT_EQ (collar->GetNFaces (), (unsigned int)(10 * n));
	std::map<std::pair<unsigned int, unsigned int>, int> edges;
	Mesh &m = *collar;
	for (unsigned int f = 0; f < m.GetNFaces (); ++f)
		for (int k = 0; k < 3; ++k)
		{
			unsigned int a = (unsigned int)m.GetFaceVertex (f, k);
			unsigned int b = (unsigned int)m.GetFaceVertex (f, (k + 1) % 3);
			if (a > b) std::swap (a, b);
			edges[std::make_pair (a, b)]++;
		}
	std::size_t odd = 0;
	for (const auto &e : edges)
		if (e.second != 2) ++odd;
	EXPECT_EQ (odd, 0u) << odd << " aretes non partagees : la peau est ouverte";
}

TEST (TEST_cggraph_nodes_stack, a_collar_that_could_not_guide_a_screw_is_refused)
{
	CountersinkCollarOptions opt;
	opt.holeRadius = 2.0f;
	opt.mouthRadius = 4.0f;
	opt.zBottom = 0.0f;
	opt.zTop = 2.0f;

	// Traversante : il ne resterait aucune portee cylindrique sous le cone.
	opt.coneDepth = 2.0f;
	EXPECT_EQ (countersinkCollar (opt), nullptr);
	opt.coneDepth = 3.0f;
	EXPECT_EQ (countersinkCollar (opt), nullptr);

	// Bouche plus etroite ou egale au trou : ce n'est pas une fraisure.
	opt.coneDepth = 1.0f;
	opt.mouthRadius = 2.0f;
	EXPECT_EQ (countersinkCollar (opt), nullptr);

	// Et la forme licite passe, sans quoi les refus ne prouveraient rien.
	opt.mouthRadius = 4.0f;
	std::unique_ptr<Mesh> ok (countersinkCollar (opt));
	EXPECT_NE (ok, nullptr);
}

TEST (TEST_cggraph_nodes_stack, the_countersink_removes_a_cone_and_not_a_counterbore)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "Hip", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId solid = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, solid, 0), ConnectStatus::Ok);
	const NodeId mounts = graph.AddNode (MakeNode ("mesh.mounts"));
	ASSERT_EQ (graph.Connect (solid, 0, mounts, 0), ConnectStatus::Ok);

	const float holeD = 4.5f, mouthD = 9.0f, thickness = 3.0f;
	Node *node = graph.FindNode (mounts);
	node->GetParams ().SetFloat ("holeDiameter", holeD);
	node->GetParams ().SetFloat ("earDiameter", 14.0f);
	node->GetParams ().SetFloat ("thickness", thickness);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList plain, sunk;
	ASSERT_TRUE (evaluator.Evaluate (mounts, plain, ctx).IsOk ());
	std::shared_ptr<const Mesh> flat = plain[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (flat, nullptr);
	float flatLo, flatHi;
	zRange (*flat, flatLo, flatHi);

	node->GetParams ().SetFloat ("countersinkDiameter", mouthD);
	node->GetParams ().SetFloat ("countersinkAngle", 90.0f);
	ASSERT_TRUE (evaluator.Evaluate (mounts, sunk, ctx).IsOk ());
	std::shared_ptr<const Mesh> cs = sunk[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (cs, nullptr);

	// La fraisure ENLEVE de la matiere, quelle que soit la facon dont elle est
	// construite -- et elle est construite par AJOUTS seulement. C'est donc la
	// verification qui compte : le volume total doit baisser.
	const double before = meshSignedVolume (*flat);
	const double after = meshSignedVolume (*cs);
	EXPECT_LT (after, before) << "la fraisure a ajoute de la matiere au lieu d'en retirer";

	// Et ce qu'elle enleve est un CONE : le vide entre la portee cylindrique et la
	// paroi qui s'evase, soit l'integrale de la section du cone moins celle du
	// trou. Un lamage -- un creux a fond plat de meme bouche et meme profondeur --
	// en enleverait plus du double ; c'est l'ecart entre les deux qui distingue
	// une tete fraisee affleurante d'une tete cylindrique noyee.
	const double r = 0.5 * holeD, R = 0.5 * mouthD;
	const double depth = R - r;                   // tan(45 degres) = 1
	const double cone = 2.0 * (frustumVolume (r, R, depth, 32)
	                         - cylinderVolume (r, depth, 32));
	const double counterbore = 2.0 * (cylinderVolume (R, depth, 32)
	                                - cylinderVolume (r, depth, 32));
	EXPECT_LT (cone, 0.6 * counterbore)
		<< "cone et lamage se valent : la mesure ci-dessous ne distinguerait rien";

	// ⚠ LE RECOUVREMENT SE PAIE DANS CE CHIFFRE, et il faut le dire plutot que
	// d'elargir la tolerance jusqu'a ce que ca passe. La bague mord de `bite` dans
	// la plaque ; un volume signe est ADDITIF, donc ce chevauchement est compte
	// DEUX fois dans la somme des deux coques. La piece imprimee, elle, ne le
	// contient qu'une fois -- le trancheur unifie. Le terme ci-dessous est
	// exactement ce double compte, et il est lu sur le defaut de la bague plutot
	// que recopie, pour qu'un changement de ce defaut fasse rougir ce cas.
	const double bite = CountersinkCollarOptions ().bite;
	const double overlap = 2.0 * (inscribedArea (R + bite, 32) - inscribedArea (R, 32))
	                     * thickness;
	EXPECT_NEAR (before - after, cone - overlap, 0.02 * cone);

	// L'EPAISSEUR ne change pas : la fraisure se creuse DANS la plaque, elle ne
	// la surepaissit pas, et la face arriere reste plane contre le mur.
	float csLo, csHi;
	zRange (*cs, csLo, csHi);
	EXPECT_NEAR (csLo, flatLo, 1e-3f);
	EXPECT_NEAR (csHi, flatHi, 1e-3f);
}

TEST (TEST_cggraph_nodes_stack, a_countersink_that_would_eat_the_ear_or_the_plate_is_refused)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "Hip", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId solid = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, solid, 0), ConnectStatus::Ok);
	const NodeId mounts = graph.AddNode (MakeNode ("mesh.mounts"));
	ASSERT_EQ (graph.Connect (solid, 0, mounts, 0), ConnectStatus::Ok);

	Node *node = graph.FindNode (mounts);
	node->GetParams ().SetFloat ("holeDiameter", 4.5f);
	node->GetParams ().SetFloat ("earDiameter", 12.0f);
	node->GetParams ().SetFloat ("thickness", 2.0f);

	Evaluator evaluator (graph);
	EvalContext ctx;

	struct Case { float diameter; float angle; const char *why; };
	const Case refused[] = {
		{ 4.0f, 90.0f, "une bouche plus etroite que le trou n'est pas une fraisure" },
		{ 12.0f, 90.0f, "une bouche aussi large que l'oreille mange l'oreille" },
		// (9 - 4,5) / 2 = 2,25 mm de cone dans 2 mm de plaque : traversante.
		{ 9.0f, 90.0f, "la fraisure traverserait la plaque" },
		{ 6.0f, 10.0f, "un angle de 10 degres n'est pas une tete de vis" }
	};
	for (const Case &c : refused)
	{
		node->GetParams ().SetFloat ("countersinkDiameter", c.diameter);
		node->GetParams ().SetFloat ("countersinkAngle", c.angle);
		ValueList out;
		EXPECT_FALSE (evaluator.Evaluate (mounts, out, ctx).IsOk ()) << c.why;
	}

	// Zero vaut « aucune fraisure », et c'est le defaut : la piece est alors
	// celle d'avant, pas une piece refusee.
	node->GetParams ().SetFloat ("countersinkDiameter", 0.0f);
	node->GetParams ().SetFloat ("countersinkAngle", 90.0f);
	ValueList none;
	EXPECT_TRUE (evaluator.Evaluate (mounts, none, ctx).IsOk ());

	// Et une fraisure qui TIENT dans la plaque passe : 6 mm de bouche pour 4,5 de
	// trou font 0,75 mm de cone, largement dans les 2 mm.
	node->GetParams ().SetFloat ("countersinkDiameter", 6.0f);
	ValueList ok;
	EXPECT_TRUE (evaluator.Evaluate (mounts, ok, ctx).IsOk ());
}

// ===========================================================================
//  L'ETENDUE A LA HAUTEUR DE LA FIXATION, et non sur l'emprise totale
// ===========================================================================
//
// « Jusqu'ou va ce modele ? » n'a pas de reponse unique. Un L va jusqu'a la
// pointe de son pied en bas, et pas plus loin que son montant a mi-hauteur. La
// boite englobante ne rend que la premiere : une fixation placee a mi-hauteur
// d'apres elle deborderait dans le vide, oreille comprise.

namespace {

// Le L, pose a la main : aucune police n'entre ici, donc rien ne depend de
// metriques qu'un remplacement de fichier changerait. Pied de 10 de large sur 2
// de haut, montant de 2 de large sur 12.
std::vector<ExtrudeContour> elShape ()
{
	ExtrudeContour c;
	c.pts = { Vector2f (0.f, 0.f),  Vector2f (10.f, 0.f), Vector2f (10.f, 2.f),
	          Vector2f (2.f, 2.f),  Vector2f (2.f, 12.f), Vector2f (0.f, 12.f) };
	return { c };
}

}  // namespace

TEST (TEST_cggraph_nodes_stack, the_slab_extent_follows_the_shape_and_not_its_bounding_box)
{
	ExtrudeAppendOptions opt;
	opt.zBottom = 0.f;
	opt.zTop = 3.f;
	opt.winding = ExtrudeWinding::NonZero;
	ExtrudedMeshBuilder builder;
	ASSERT_TRUE (builder.Append (elShape (), opt));
	std::unique_ptr<Mesh> mesh (builder.Build ());
	ASSERT_NE (mesh, nullptr);

	float lo = 0.f, hi = 0.f;

	// EN BAS, dans le pied : toute la largeur.
	ASSERT_TRUE (meshSlabExtentX (*mesh, 0.5f, 1.5f, lo, hi));
	EXPECT_NEAR (lo, 0.f, 1e-4f);
	EXPECT_NEAR (hi, 10.f, 1e-4f);

	// A MI-HAUTEUR, dans le montant : le pied n'y est plus, et c'est tout
	// l'interet -- la boite englobante, elle, aurait rendu 10 aux deux hauteurs.
	ASSERT_TRUE (meshSlabExtentX (*mesh, 5.f, 7.f, lo, hi));
	EXPECT_NEAR (lo, 0.f, 1e-4f);
	EXPECT_NEAR (hi, 2.f, 1e-4f);

	// A CHEVAL sur la marche : la tranche prend le plus large des deux.
	ASSERT_TRUE (meshSlabExtentX (*mesh, 1.f, 3.f, lo, hi));
	EXPECT_NEAR (hi, 10.f, 1e-4f);

	// Une tranche d'epaisseur NULLE est licite : c'est la section a une cote.
	ASSERT_TRUE (meshSlabExtentX (*mesh, 6.f, 6.f, lo, hi));
	EXPECT_NEAR (hi, 2.f, 1e-4f);

	// HORS de la piece : faux, et les sorties intactes. Ce n'est pas une panne,
	// c'est la reponse -- celle qui doit arreter l'appelant.
	float keptLo = -12345.f, keptHi = -12345.f;
	EXPECT_FALSE (meshSlabExtentX (*mesh, 20.f, 30.f, keptLo, keptHi));
	EXPECT_FLOAT_EQ (keptLo, -12345.f);
	EXPECT_FLOAT_EQ (keptHi, -12345.f);
	EXPECT_FALSE (meshSlabExtentX (*mesh, -5.f, -1.f, keptLo, keptHi));

	// Les bornes remises dans l'ordre donnent la meme chose.
	float swappedLo = 0.f, swappedHi = 0.f;
	ASSERT_TRUE (meshSlabExtentX (*mesh, 7.f, 5.f, swappedLo, swappedHi));
	EXPECT_NEAR (swappedHi, 2.f, 1e-4f);
}

TEST (TEST_cggraph_nodes_stack, the_mounts_hug_the_matter_at_their_own_height)
{
	Graph graph;
	// Un L : large en bas, etroit a mi-hauteur. Le cas qui separe les deux
	// mesures, et le seul ou l'ancienne se voyait.
	const NodeId contours = AddTextContours (graph, "L", 40.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId solid = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, solid, 0), ConnectStatus::Ok);
	const NodeId mounts = graph.AddNode (MakeNode ("mesh.mounts"));
	ASSERT_EQ (graph.Connect (solid, 0, mounts, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	Node *node = graph.FindNode (mounts);
	MountsNode *mountsNode = static_cast<MountsNode *> (node);

	node->GetParams ().SetFloat ("lineHeight", 0.0f);
	ValueList low;
	ASSERT_TRUE (evaluator.Evaluate (mounts, low, ctx).IsOk ());
	const float atFoot = mountsNode->GetHoleSpacing ();

	node->GetParams ().SetFloat ("lineHeight", 0.5f);
	ValueList mid;
	ASSERT_TRUE (evaluator.Evaluate (mounts, mid, ctx).IsOk ());
	const float atStem = mountsNode->GetHoleSpacing ();

	// Le montant est plus etroit que le pied, donc l'entraxe RETRECIT. Avec
	// l'emprise totale les deux seraient egaux -- c'est exactement ce que ce cas
	// garde, et il aurait ete vert avant le changement.
	EXPECT_LT (atStem, atFoot - 1.0f)
		<< "l'entraxe ne suit pas la matiere : pied " << atFoot << ", montant " << atStem;

	// Et le bandeau tombe bien SUR la matiere : son extremite gauche est prise
	// sur elle, donc l'emprise de la piece fixee ne peut pas s'etendre au-dela
	// de ce que le debord ajoute.
	std::shared_ptr<const Mesh> fixed = mid[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (fixed, nullptr);
	std::shared_ptr<const Mesh> bare =
		[&] { ValueList v; EXPECT_TRUE (evaluator.Evaluate (solid, v, ctx).IsOk ());
		      return v[0].Share<Mesh> (Types ().mesh); }();
	ASSERT_NE (bare, nullptr);
	float bx0, bx1, by0, by1, fx0, fx1, fy0, fy1;
	xyRange (*bare, bx0, bx1, by0, by1);
	xyRange (*fixed, fx0, fx1, fy0, fy1);
	// A mi-hauteur, la matiere s'arrete AVANT le bord droit de la piece : la
	// fixation ne peut donc pas atteindre ce bord, debord compris ou non.
	EXPECT_LT (fx1, bx1 + 6.0f + 1e-3f);
}

TEST (TEST_cggraph_nodes_stack, a_straight_sided_piece_keeps_the_same_span_at_every_height)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "L", 40.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	// Avec un SOCLE rectangulaire, la matiere occupe toute la largeur a toute
	// hauteur : la mesure dans la tranche rend alors exactement ce que rendait
	// l'emprise totale. C'est le controle qui interdit de croire que le
	// changement deplace la fixation dans tous les cas.
	const NodeId plate = graph.AddNode (MakeNode ("shape.contours.plate"));
	ASSERT_EQ (graph.Connect (contours, 0, plate, 0), ConnectStatus::Ok);
	const NodeId solid = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (plate, 0, solid, 0), ConnectStatus::Ok);
	graph.FindNode (plate)->GetParams ().SetFloat ("cornerRadius", 0.0f);

	const NodeId mounts = graph.AddNode (MakeNode ("mesh.mounts"));
	ASSERT_EQ (graph.Connect (solid, 0, mounts, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	Node *node = graph.FindNode (mounts);
	MountsNode *mountsNode = static_cast<MountsNode *> (node);

	float first = 0.f;
	for (const float fraction : { 0.2f, 0.5f, 0.8f })
	{
		node->GetParams ().SetFloat ("lineHeight", fraction);
		ValueList out;
		ASSERT_TRUE (evaluator.Evaluate (mounts, out, ctx).IsOk ());
		const float span = mountsNode->GetHoleSpacing ();
		if (first == 0.f) first = span;
		EXPECT_NEAR (span, first, 1e-3f) << "fraction " << fraction;
	}
}

// ===========================================================================
//  LES MESURES SURVIVENT AU CACHE
// ===========================================================================
//
// Un noeud publie les mesures de son DERNIER calcul. Un succes de cache n'en
// fait aucun : le noeud garde alors celles d'une autre signature, et qui les lui
// demande obtient un chiffre exact qui ne decrit pas ce qu'il regarde.
//
// Le cas est ecrit ici et pas dans les tests du moteur parce que c'est ici qu'il
// s'est manifeste, et sous sa forme la plus couteuse : l'entraxe d'une fixation
// restait fige des qu'on revenait sur une hauteur deja visitee. Un chiffre faux,
// et de ceux d'apres lesquels on perce un mur.

TEST (TEST_cggraph_nodes_stack, revisiting_a_setting_does_not_serve_a_stale_measurement)
{
	Graph graph;
	const NodeId contours = AddTextContours (graph, "L", 40.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId solid = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, solid, 0), ConnectStatus::Ok);
	const NodeId mounts = graph.AddNode (MakeNode ("mesh.mounts"));
	ASSERT_EQ (graph.Connect (solid, 0, mounts, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	Node *node = graph.FindNode (mounts);

	// Ce que l'evaluateur rend POUR CE PARCOURS, par opposition a ce que le noeud
	// garde de son dernier calcul. C'est le chemin que suit la page.
	const auto runSpacing = [&] () -> double {
		const std::map<NodeId, std::vector<cggraph::NodeStat>> run = evaluator.TakeRunStats ();
		std::map<NodeId, std::vector<cggraph::NodeStat>>::const_iterator it = run.find (mounts);
		if (it == run.end ()) return -1.0;
		for (const cggraph::NodeStat &s : it->second)
			if (s.name == "holeSpacing") return s.value;
		return -1.0;
	};

	const auto spacingAt = [&] (float fraction) -> double {
		node->GetParams ().SetFloat ("lineHeight", fraction);
		ValueList out;
		EXPECT_TRUE (evaluator.Evaluate (mounts, out, ctx).IsOk ());
		return runSpacing ();
	};

	// Premier passage : chaque hauteur est calculee, donc mesuree.
	const double first0 = spacingAt (0.0f);
	const double first5 = spacingAt (0.5f);
	ASSERT_GT (first0, 0.0);
	ASSERT_GT (first5, 0.0);
	ASSERT_GT (first0, first5 + 1.0) << "le L ne separe pas les deux hauteurs";

	// Second passage : les deux signatures sont EN CACHE, donc aucun Compute ne
	// tourne. C'est exactement la ou le chiffre mentait.
	EXPECT_TRUE (evaluator.IsCached (mounts));
	const double again5 = spacingAt (0.5f);
	const double again0 = spacingAt (0.0f);
	EXPECT_DOUBLE_EQ (again5, first5);
	EXPECT_DOUBLE_EQ (again0, first0);

	// Et le succes de cache a bien eu lieu, sans quoi le cas ne prouverait rien :
	// il aurait pu tout recalculer et passer pour de mauvaises raisons.
	EXPECT_GT (evaluator.GetStats ().hits, 0u);

	// LA DEMONSTRATION, et elle vaut mieux qu'un commentaire : a cet instant
	// precis, le NOEUD porte encore la mesure de son dernier calcul reel -- celui
	// de la hauteur 0,5 -- alors qu'on vient de lui redemander la hauteur 0. Le
	// chiffre qu'il rend est exact et decrit autre chose. C'est pour cela que la
	// lecture passe par le parcours et non par lui.
	MountsNode *mountsNode = static_cast<MountsNode *> (node);
	EXPECT_NEAR ((double)mountsNode->GetHoleSpacing (), first5, 1e-3);
	EXPECT_GT (std::fabs ((double)mountsNode->GetHoleSpacing () - again0), 1.0);
}

TEST (TEST_cggraph_nodes_stack, a_measurement_survives_a_cache_hit_on_the_node_above_it)
{
	// LE CAS REEL, et il est plus dur que le precedent : la page n'evalue pas la
	// fixation, elle evalue la SORTIE. Quand la signature de la sortie est deja
	// en cache, l'evaluateur retourne tot et ne descend meme pas jusqu'a la
	// fixation -- qui ne passe alors par aucun des deux sites d'enregistrement.
	// Mesure sur la page avant correction : l'entraxe restait fige sur le dernier
	// reglage NEUF, un chiffre d'apres lequel on perce un mur.
	Graph graph;
	const NodeId contours = AddTextContours (graph, "L", 40.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";

	const NodeId solid = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, solid, 0), ConnectStatus::Ok);
	const NodeId mounts = graph.AddNode (MakeNode ("mesh.mounts"));
	ASSERT_EQ (graph.Connect (solid, 0, mounts, 0), ConnectStatus::Ok);
	// LE NOEUD DU DESSUS, comme sur la page ou la couleur termine la chaine.
	const NodeId paint = graph.AddNode (MakeNode ("mesh.color"));
	ASSERT_EQ (graph.Connect (mounts, 0, paint, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	Node *node = graph.FindNode (mounts);

	const auto spacingAt = [&] (float fraction) -> double {
		node->GetParams ().SetFloat ("lineHeight", fraction);
		ValueList out;
		// On evalue la SORTIE, pas la fixation.
		EXPECT_TRUE (evaluator.Evaluate (paint, out, ctx).IsOk ());
		const std::map<NodeId, std::vector<cggraph::NodeStat>> run = evaluator.TakeRunStats ();
		std::map<NodeId, std::vector<cggraph::NodeStat>>::const_iterator it = run.find (mounts);
		if (it == run.end ()) return -1.0;
		for (const cggraph::NodeStat &s : it->second)
			if (s.name == "holeSpacing") return s.value;
		return -1.0;
	};

	const double first0 = spacingAt (0.0f);
	const double first5 = spacingAt (0.5f);
	ASSERT_GT (first0, 0.0);
	ASSERT_GT (first5, 0.0);
	ASSERT_GT (first0, first5 + 1.0) << "le L ne separe pas les deux hauteurs";

	const unsigned int before = evaluator.GetStats ().misses;
	// Retour sur des reglages deja vus : la sortie sort du cache telle quelle,
	// donc RIEN ne se recalcule -- et la mesure doit tout de meme etre juste.
	const double again0 = spacingAt (0.0f);
	const double again5 = spacingAt (0.5f);
	EXPECT_EQ (evaluator.GetStats ().misses, before)
		<< "un calcul a eu lieu : le cas ne prouve plus rien";
	EXPECT_DOUBLE_EQ (again0, first0);
	EXPECT_DOUBLE_EQ (again5, first5);
}

TEST (TEST_cggraph_nodes_stack, a_band_falling_between_two_lines_of_text_is_refused)
{
	// LE CAS QUI S'EST PRESENTE COMME « la fixation ne se met plus a jour ». Sur
	// un texte de deux lignes, le bandeau a mi-hauteur tombe dans le BLANC entre
	// elles : il ne se souderait a rien et sortirait en barre libre. Le noeud
	// refuse -- et un refus GELE la vue de la page sur le modele precedent, ce
	// qui se lit exactement comme « rien ne s'est mis a jour ».
	//
	// Le conseil de panne du gabarit (failureHints, noeud 15) est ce qui rend ce
	// cul-de-sac lisible ; ce cas-ci garde la geometrie qui le provoque.
	Graph graph;
	const NodeId contours = AddTextContours (graph, "A\nA", 20.0f);
	if (contours == kInvalidNodeId) GTEST_SKIP () << "police de test absente";
	// Interligne TRIPLE : le blanc est alors plus haut que le bandeau quelle que
	// soit la police, donc le cas ne depend d'aucune metrique particuliere.
	graph.FindNode (contours)->GetParams ().SetFloat ("lineSpacing", 3.0f);

	const NodeId solid = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_EQ (graph.Connect (contours, 0, solid, 0), ConnectStatus::Ok);
	const NodeId mounts = graph.AddNode (MakeNode ("mesh.mounts"));
	ASSERT_EQ (graph.Connect (solid, 0, mounts, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	Node *node = graph.FindNode (mounts);
	node->GetParams ().SetFloat ("bandWidth", 6.0f);

	node->GetParams ().SetFloat ("lineHeight", 0.5f);
	ValueList mid;
	EXPECT_FALSE (evaluator.Evaluate (mounts, mid, ctx).IsOk ())
		<< "un bandeau pose dans le blanc entre deux lignes ne tient a rien";

	// Et sur les lignes elles-memes, il passe : le refus vise le VIDE, pas le
	// texte multiligne.
	for (const float fraction : { 0.0f, 1.0f })
	{
		node->GetParams ().SetFloat ("lineHeight", fraction);
		ValueList out;
		EXPECT_TRUE (evaluator.Evaluate (mounts, out, ctx).IsOk ()) << "fraction " << fraction;
	}

	// L'issue que le conseil de panne recommande en second : ELARGIR le bandeau
	// jusqu'a ce qu'il atteigne les lettres. Elle doit marcher, sans quoi le
	// conseil enverrait dans le mur.
	node->GetParams ().SetFloat ("lineHeight", 0.5f);
	node->GetParams ().SetFloat ("bandWidth", 200.0f);
	ValueList wide;
	EXPECT_TRUE (evaluator.Evaluate (mounts, wide, ctx).IsOk ())
		<< "un bandeau assez large pour rejoindre les deux lignes devrait passer";
}
