#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "../src/cggraph/core/evaluator.h"
#include "../src/cggraph/core/serialize.h"
#include "../src/cggraph/nodes/catalog.h"
#include "../src/cggraph/nodes/catalog_factory.h"
#include "../src/cggraph/nodes/io/file_ref.h"
#include "../src/cggraph/nodes/mesh/color.h"
#include "../src/cggraph/nodes/value_types.h"
#include "../src/cgmesh/import_svg.h"
#include "../src/cgmesh/material.h"
#include "../src/cgmesh/mesh.h"

// ===========================================================================
//  K5 -- le noeud svg.extrude.colored, sa bascule, et ce qu'elle gouverne
// ===========================================================================
//
// Ce fichier tient les quatre criteres du jalon, et il les tient sur la MEME
// chaine que la page : file.ref -> svg.extrude.colored -> mesh.color.
//
//   1. un document ANTERIEUR (svg.contours -> shape.extrude -> mesh.color)
//      s'ouvre et s'evalue encore ;
//   2. bascule a `false`, mesh.color peint TOUTES les faces ;
//   3. bascule a `true`, la palette depasse trente couleurs et mesh.color n'en
//      peint AUCUNE ;
//   4. le grisage du selecteur suit la mesure `paintedFaces`, et rien d'autre.
//
// Le quatrieme critere se joue dans le navigateur ; ce qui se verifie ICI est
// ce dont il depend -- la mesure publiee. Un test C++ ne peut pas constater un
// widget gris ; il peut constater que le chiffre qui le grise vaut ce qu'il doit
// valoir, et c'est la seule moitie du critere qui soit falsifiable sans ecran.
//
using namespace cggraph;
using namespace cggraph_nodes;

namespace {

const char *kTiger  = "./test/data/svg/Ghostscript_Tiger.svg";
const char *kRose   = "./test/data/svg/rose.svg";
const char *kLegacy = "./test/data/graphs/svg_before_k5.json";

std::string ReadTextFile (const char *path)
{
	std::string text;
	std::FILE *f = std::fopen (path, "rb");
	if (!f)
		return text;
	std::fseek (f, 0, SEEK_END);
	const long size = std::ftell (f);
	std::fseek (f, 0, SEEK_SET);
	if (size > 0)
	{
		text.resize ((std::size_t)size);
		if (std::fread (&text[0], 1, text.size (), f) != text.size ())
			text.clear ();
	}
	std::fclose (f);
	return text;
}

// La chaine de la page, montee a la main : la source, l'extrudeur colore, la
// couleur. Les deux noeuds sont rendus a l'appelant, qui regle la bascule puis
// evalue -- c'est exactement ce que fait le panneau.
struct Chain
{
	Graph  graph;
	NodeId file = 0;
	NodeId svg = 0;
	NodeId color = 0;

	explicit Chain (const char *path)
	{
		file = graph.AddNode (MakeNode ("file.ref"));
		svg = graph.AddNode (MakeNode ("svg.extrude.colored"));
		color = graph.AddNode (MakeNode ("mesh.color"));
		static_cast<FileRefNode *> (graph.FindNode (file))->SetPath (path);
		graph.Connect (file, 0, svg, 0);
		graph.Connect (svg, 0, color, 0);
	}

	ParamSet &SvgParams () { return graph.FindNode (svg)->GetParams (); }
};

// Mesures publiees par un noeud, par nom. C'est le chemin qu'emprunte la page :
// graphNodeInfo les serialise depuis PublishStats, et le gabarit les lit.
double StatOf (const Graph &graph, NodeId id, const char *name)
{
	const Node *node = graph.FindNode (id);
	if (node == nullptr)
		return -1.0;
	std::vector<NodeStat> stats;
	node->PublishStats (stats);
	for (const NodeStat &s : stats)
		if (s.name == name)
			return s.value;
	return -1.0;
}

unsigned int FacesWithoutMaterial (const Mesh &mesh)
{
	unsigned int n = 0;
	for (unsigned int f = 0; f < mesh.GetNFaces (); ++f)
		if (mesh.GetFaceMaterialId (f) < 0)
			++n;
	return n;
}

void Extent (const Mesh &mesh, float out[3])
{
	const std::vector<float> &v = mesh.GetVertices ();
	out[0] = out[1] = out[2] = 0.0f;
	if (v.size () < 3)
		return;
	float lo[3] = { v[0], v[1], v[2] }, hi[3] = { v[0], v[1], v[2] };
	for (std::size_t i = 0; i + 2 < v.size (); i += 3)
		for (int d = 0; d < 3; ++d)
		{
			lo[d] = std::min (lo[d], v[i + d]);
			hi[d] = std::max (hi[d], v[i + d]);
		}
	for (int d = 0; d < 3; ++d)
		out[d] = hi[d] - lo[d];
}

} // namespace

// ---------------------------------------------------------------------------
//  Critere 1 -- le document anterieur
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_svg_k5, the_pre_k5_document_still_loads_and_evaluates)
{
	// LE CRITERE QUI COUTE LE PLUS CHER A RATER : un document enregistre depuis
	// la page d'avant nomme `svg.contours` et `shape.extrude`. Retirer l'un des
	// deux du catalogue le rendrait illisible, et l'utilisateur n'aurait aucun
	// moyen de le reparer.
	//
	// Le document est un FICHIER et non une chaine ecrite ici : c'est le graphe
	// que le gabarit portait avant K5, extrait tel quel.
	const std::string document = ReadTextFile (kLegacy);
	ASSERT_FALSE (document.empty ()) << kLegacy << " introuvable";

	Graph graph;
	const CatalogFactory factory;
	const LoadResult loaded = LoadGraph (document, factory, graph);
	ASSERT_TRUE (loaded.IsOk ()) << ToString (loaded.status) << " : " << loaded.detail;
	EXPECT_EQ (graph.GetNodeCount (), 4u);

	FileRefNode *source = nullptr;
	for (NodeId id = 1; id <= 8 && source == nullptr; ++id)
		source = dynamic_cast<FileRefNode *> (graph.FindNode (id));
	ASSERT_NE (source, nullptr);
	source->SetPath (kRose);

	// Il s'OUVRE, et il se CALCULE : la premiere moitie seule laisserait passer
	// un noeud dont la version de descripteur aurait ete incrementee.
	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (3, outputs, ctx).IsOk ());
	ASSERT_FALSE (outputs.empty ());
	const Mesh *mesh = outputs[0].Get<Mesh> (Types ().mesh);
	ASSERT_NE (mesh, nullptr);
	EXPECT_GT (mesh->GetNFaces (), 0u);

	// Et la chaine anterieure produit toujours une piece d'UN SEUL materiau :
	// c'est ce que « le rendu d'aujourd'hui » veut dire.
	EXPECT_EQ (mesh->GetNMaterials (), 1u);
}

// ---------------------------------------------------------------------------
//  Criteres 2 et 3 -- ce que la bascule fait des deux cotes
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_svg_k5, the_toggle_off_leaves_every_face_to_the_color_node)
{
	Chain chain (kTiger);
	chain.SvgParams ().SetBool ("useSvgColors", false);

	Evaluator evaluator (chain.graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (chain.color, outputs, ctx).IsOk ());
	std::shared_ptr<const Mesh> painted = outputs[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (painted, nullptr);

	// UNE seule couleur dans la piece, celle de mesh.color.
	EXPECT_EQ (painted->GetNMaterials (), 1u);
	EXPECT_EQ (FacesWithoutMaterial (*painted), 0u);

	// `paintedFaces == nombre de faces` : le critere 2 mot pour mot.
	const double faces = (double)painted->GetNFaces ();
	EXPECT_EQ (StatOf (chain.graph, chain.color, "paintedFaces"), faces);
	EXPECT_EQ (StatOf (chain.graph, chain.color, "keptFaces"), 0.0);

	// Et la marqueterie n'a PAS tourne : la bascule gouverne les deux options,
	// donc `false` doit laisser la geometrie ou elle etait.
	EXPECT_EQ (StatOf (chain.graph, chain.svg, "hiddenShapes"), 0.0);
	EXPECT_EQ (StatOf (chain.graph, chain.svg, "subtractedShapes"), 0.0);
}

TEST (TEST_cggraph_svg_k5, the_toggle_on_paints_the_document_and_leaves_nothing_to_paint)
{
	Chain chain (kTiger);
	chain.SvgParams ().SetBool ("useSvgColors", true);

	Evaluator evaluator (chain.graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (chain.color, outputs, ctx).IsOk ());
	std::shared_ptr<const Mesh> painted = outputs[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (painted, nullptr);

	// Le seuil du critere 3 -- « au moins trente couleurs » -- porte sur les
	// materiaux du maillage RENDU. mesh.color ajoute le sien a la table meme
	// s'il ne peint rien : c'est celui du noeud amont qu'on compte.
	const double materials = StatOf (chain.graph, chain.svg, "materials");
	EXPECT_GE (materials, 30.0);
	EXPECT_EQ (painted->GetNMaterials (), (unsigned int)materials + 1u);

	// AUCUNE face sans materiau, donc rien a peindre : c'est l'indicateur qui
	// grise le selecteur cote page.
	EXPECT_EQ (FacesWithoutMaterial (*painted), 0u);
	EXPECT_EQ (StatOf (chain.graph, chain.color, "paintedFaces"), 0.0);
	EXPECT_EQ (StatOf (chain.graph, chain.color, "keptFaces"),
	           (double)painted->GetNFaces ());
}

// ---------------------------------------------------------------------------
//  Les mesures publiees, et leurs valeurs sur le fichier de reference
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_svg_k5, the_node_publishes_what_the_document_lost)
{
	Chain chain (kTiger);
	chain.SvgParams ().SetBool ("useSvgColors", true);

	Evaluator evaluator (chain.graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (chain.svg, outputs, ctx).IsOk ());

	const double gradients = StatOf (chain.graph, chain.svg, "gradientShapes");
	const double hidden = StatOf (chain.graph, chain.svg, "hiddenShapes");
	const double subtracted = StatOf (chain.graph, chain.svg, "subtractedShapes");
	const double materials = StatOf (chain.graph, chain.svg, "materials");

	std::cout << "[K5] tigre : gradientShapes " << gradients
	          << " · hiddenShapes " << hidden
	          << " · subtractedShapes " << subtracted
	          << " · materials " << materials << std::endl;
	RecordProperty ("gradientShapes", (int)gradients);
	RecordProperty ("hiddenShapes", (int)hidden);
	RecordProperty ("subtractedShapes", (int)subtracted);

	// AUCUN degrade dans ce fichier : le compte est a zero, et c'est ce qui rend
	// la mesure utile ailleurs -- un fichier qui en porte le dirait.
	EXPECT_EQ (gradients, 0.0);

	// DEUX groupes masques, et pas « beaucoup » : le chiffre est celui que K3 a
	// figé (fullyCoveredGroups == 2). Les « 197 formes coupant une forme
	// superieure » de K0 comptaient des BOITES ENGLOBANTES, qui majorent
	// massivement -- l'ecrire ici evite qu'on rebatisse une interface sur ce
	// majorant.
	EXPECT_EQ (hidden, 2.0);

	// Amputees mais vivantes : strictement entre zero et le nombre de groupes.
	// Bornes plutot qu'egalite, parce que la valeur exacte depend de
	// `flattenTol` -- ce que le seuil des deux precedentes ne fait pas.
	EXPECT_GT (subtracted, 0.0);
	EXPECT_LT (subtracted, 304.0);
}

// ---------------------------------------------------------------------------
//  « Le rendu d'aujourd'hui », pris au mot
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_svg_k5, without_colors_and_without_rings_it_is_the_previous_chain_vertex_for_vertex)
{
	// LE CRITERE 2 PRIS AU PIED DE LA LETTRE. « Le rendu d'aujourd'hui » ne se
	// verifie pas par un compte de materiaux : il se verifie SOMMET PAR SOMMET
	// contre la chaine qu'on remplace.
	//
	// Deux reglages, et non un, l'obtiennent : `useSvgColors` a faux ecarte la
	// palette ET la marqueterie, et `strokeOnFilledShapes` a faux ecarte les
	// anneaux -- que le NOEUD arme par defaut, la ou svg.contours ne les
	// connaissait pas. C'est la seule difference entre les deux defauts, et ce
	// cas la nomme au lieu de la laisser se decouvrir a l'ecran.
	Graph graph;
	const NodeId fileId = graph.AddNode (MakeNode ("file.ref"));
	static_cast<FileRefNode *> (graph.FindNode (fileId))->SetPath (kTiger);

	const NodeId before = graph.AddNode (MakeNode ("svg.contours"));
	const NodeId extrude = graph.AddNode (MakeNode ("shape.extrude"));
	const NodeId after = graph.AddNode (MakeNode ("svg.extrude.colored"));
	ASSERT_EQ (graph.Connect (fileId, 0, before, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (before, 0, extrude, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (fileId, 0, after, 0), ConnectStatus::Ok);

	graph.FindNode (before)->GetParams ().SetFloat ("fitSize", 0.0f);
	graph.FindNode (extrude)->GetParams ().SetFloat ("depth", 3.0f);
	ParamSet &params = graph.FindNode (after)->GetParams ();
	params.SetFloat ("fitSize", 0.0f);
	params.SetFloat ("depth", 3.0f);
	params.SetBool ("useSvgColors", false);
	params.SetBool ("strokeOnFilledShapes", false);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList oldOut, newOut;
	ASSERT_TRUE (evaluator.Evaluate (extrude, oldOut, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (after, newOut, ctx).IsOk ());
	std::shared_ptr<const Mesh> oldMesh = oldOut[0].Share<Mesh> (Types ().mesh);
	std::shared_ptr<const Mesh> newMesh = newOut[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (oldMesh, nullptr);
	ASSERT_NE (newMesh, nullptr);

	ASSERT_EQ (newMesh->GetNVertices (), oldMesh->GetNVertices ());
	ASSERT_EQ (newMesh->GetNFaces (), oldMesh->GetNFaces ());
	EXPECT_EQ (newMesh->GetNMaterials (), 0u);
	EXPECT_EQ (oldMesh->GetNMaterials (), 0u);

	// SOMMET PAR SOMMET, et a l'egalite exacte : les deux chemins font les memes
	// operations flottantes dans le meme ordre. Une tolerance masquerait
	// justement ce qu'on cherche a voir.
	const std::vector<float> &a = oldMesh->GetVertices ();
	const std::vector<float> &b = newMesh->GetVertices ();
	ASSERT_EQ (a.size (), b.size ());
	std::size_t differing = 0;
	for (std::size_t i = 0; i < a.size (); ++i)
		if (a[i] != b[i])
			++differing;
	EXPECT_EQ (differing, 0u);

	// Et le contre-exemple, sans lequel le cas ci-dessus pourrait passer sur deux
	// maillages vides : armer les anneaux DOIT changer la geometrie.
	params.SetBool ("strokeOnFilledShapes", true);
	ValueList ringed;
	ASSERT_TRUE (evaluator.Evaluate (after, ringed, ctx).IsOk ());
	std::shared_ptr<const Mesh> withRings = ringed[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (withRings, nullptr);
	EXPECT_GT (withRings->GetNVertices (), oldMesh->GetNVertices ());
}

TEST (TEST_cggraph_svg_k5, under_a_size_the_two_chains_tessellate_at_two_scales)
{
	// LA LIMITE DU CAS PRECEDENT, mesuree plutot que passee sous silence.
	//
	// L'egalite au bit n'a lieu QUE sans mise a l'echelle. Les deux chaines ne
	// tessellent pas a la meme echelle : svg.contours met les CONTOURS a la taille
	// demandee avant de les donner a glutess, tandis que le noeud unique tesselle
	// le dessin normalise puis met le MAILLAGE a l'echelle. glutess decide de ses
	// fusions de sommets (COMBINE) sur les coordonnees qu'il recoit, donc les deux
	// ordres ne rendent pas le meme nombre de sommets.
	//
	// L'ecart est petit et il est BORNE ici, pour qu'une derive le fasse rougir.
	// La piece, elle, a exactement les memes cotes : c'est ce que l'utilisateur
	// regarde.
	Graph graph;
	const NodeId fileId = graph.AddNode (MakeNode ("file.ref"));
	static_cast<FileRefNode *> (graph.FindNode (fileId))->SetPath (kTiger);
	const NodeId before = graph.AddNode (MakeNode ("svg.contours"));
	const NodeId extrude = graph.AddNode (MakeNode ("shape.extrude"));
	const NodeId after = graph.AddNode (MakeNode ("svg.extrude.colored"));
	ASSERT_EQ (graph.Connect (fileId, 0, before, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (before, 0, extrude, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (fileId, 0, after, 0), ConnectStatus::Ok);

	graph.FindNode (before)->GetParams ().SetFloat ("fitSize", 100.0f);
	graph.FindNode (extrude)->GetParams ().SetFloat ("depth", 3.0f);
	ParamSet &params = graph.FindNode (after)->GetParams ();
	params.SetFloat ("fitSize", 100.0f);
	params.SetFloat ("depth", 3.0f);
	params.SetBool ("useSvgColors", false);
	params.SetBool ("strokeOnFilledShapes", false);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList oldOut, newOut;
	ASSERT_TRUE (evaluator.Evaluate (extrude, oldOut, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (after, newOut, ctx).IsOk ());
	std::shared_ptr<const Mesh> oldMesh = oldOut[0].Share<Mesh> (Types ().mesh);
	std::shared_ptr<const Mesh> newMesh = newOut[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (oldMesh, nullptr);
	ASSERT_NE (newMesh, nullptr);

	const double delta = std::abs ((double)newMesh->GetNVertices ()
	                               - (double)oldMesh->GetNVertices ());
	std::cout << "[K5] tigre a 100 mm : " << oldMesh->GetNVertices ()
	          << " sommets par svg.contours, " << newMesh->GetNVertices ()
	          << " par svg.extrude.colored" << std::endl;
	EXPECT_LT (delta / (double)oldMesh->GetNVertices (), 0.01);

	float a[3], b[3];
	Extent (*oldMesh, a);
	Extent (*newMesh, b);
	for (int d = 0; d < 3; ++d)
		EXPECT_NEAR (a[d], b[d], 1e-2f) << "axe " << d;
}

// ---------------------------------------------------------------------------
//  Ce que la migration des parametres doit preserver
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_svg_k5, size_scales_the_plan_and_leaves_the_depth_alone)
{
	// LE CONTRAT DE LA CHAINE PRECEDENTE, et il ne doit pas changer en passant
	// dans un noeud unique : `fitSize` porte le PLAN, `depth` l'epaisseur, et les
	// deux sont independantes. Un noeud monolithique qui les mettrait a l'echelle
	// ensemble ferait dependre l'epaisseur de la taille sans rien dire.
	Chain chain (kRose);
	chain.SvgParams ().SetFloat ("fitSize", 100.0f);
	chain.SvgParams ().SetFloat ("depth", 3.0f);

	Evaluator evaluator (chain.graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (chain.svg, outputs, ctx).IsOk ());
	std::shared_ptr<const Mesh> mesh = outputs[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (mesh, nullptr);

	float extent[3];
	Extent (*mesh, extent);
	EXPECT_NEAR (std::max (extent[0], extent[1]), 100.0f, 1e-3f);
	EXPECT_NEAR (extent[2], 3.0f, 1e-4f);

	// Doubler la taille ne touche pas l'epaisseur.
	chain.SvgParams ().SetFloat ("fitSize", 200.0f);
	ValueList larger;
	ASSERT_TRUE (evaluator.Evaluate (chain.svg, larger, ctx).IsOk ());
	std::shared_ptr<const Mesh> second = larger[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (second, nullptr);
	Extent (*second, extent);
	EXPECT_NEAR (std::max (extent[0], extent[1]), 200.0f, 1e-3f);
	EXPECT_NEAR (extent[2], 3.0f, 1e-4f);
}

TEST (TEST_cggraph_svg_k5, the_node_refuses_a_path_that_leads_nowhere)
{
	// Un chemin mort est un ECHEC, pas un maillage vide : sans cela, la page
	// afficherait « 0 sommets » au lieu de nommer le noeud fautif.
	Chain chain ("./test/data/svg/aucun_fichier.svg");

	Evaluator evaluator (chain.graph);
	EvalContext ctx;
	ValueList outputs;
	EXPECT_FALSE (evaluator.Evaluate (chain.svg, outputs, ctx).IsOk ());
}

TEST (TEST_cggraph_svg_k5, the_ring_of_a_filled_shape_is_on_by_default_here_and_off_in_cpp)
{
	// LE DEDOUBLEMENT DES DEFAUTS, et il est volontaire : le defaut C++ preserve
	// le comportement anterieur bit pour bit (les oracles de K0 a K3 restent
	// valides), le defaut du NOEUD sert la regle du produit -- ne rien perdre du
	// document. Les deux repondent a deux questions differentes, et ce cas les
	// tient toutes les deux.
	SvgExtrudeOptions defaults;
	EXPECT_FALSE (defaults.strokeOnFilledShapes);

	const std::unique_ptr<Node> node = MakeNode ("svg.extrude.colored");
	ASSERT_NE (node, nullptr);
	const ParamValue *value = node->GetParams ().Find ("strokeOnFilledShapes");
	ASSERT_NE (value, nullptr);
	EXPECT_EQ (value->type, ParamType::Bool);
	EXPECT_TRUE (value->boolValue);

	// Et la bascule, elle aussi, est armee par defaut : le noeud s'appelle
	// « colored ».
	const ParamValue *toggle = node->GetParams ().Find ("useSvgColors");
	ASSERT_NE (toggle, nullptr);
	EXPECT_TRUE (toggle->boolValue);
}
