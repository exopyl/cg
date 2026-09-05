#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "../src/cggraph/core/evaluator.h"
#include "../src/cggraph/nodes/catalog.h"
#include "../src/cggraph/nodes/io/file_ref.h"
#include "../src/cggraph/nodes/shapes/gothic_window.h"
#include "../src/cggraph/nodes/shapes/parametric_shape.h"
#include "../src/cggraph/nodes/shapes/profile.h"
#include "../src/cggraph/nodes/svg/svg_contours.h"
#include "../src/cggraph/nodes/text/load_font.h"
#include "../src/cggraph/nodes/value_types.h"
#include "../src/cgmesh/extrude_contours.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/parameterized_shapes.h"
#include "../src/cgmesh/parametric_catalog.h"
#include "../src/cgmesh/profile2d.h"

// ===========================================================================
//  Les GENERATEURS -- adaptateur generique, profils promus, gothique
// ===========================================================================
// Trois choses distinctes se verifient ici, et il ne faut pas les confondre :
//
//   1. la FRONTIERE de l'adaptateur generique : il emballe les formes simples,
//      et SEULEMENT elles. Une forme dont le constructeur prend un fichier ne
//      doit pas y entrer -- elle donnerait un noeud sans port d'entree, dont la
//      ressource determinante n'entrerait dans aucune signature ;
//   2. le RESULTAT de chaque noeud produit, et pas seulement qu'il s'execute ;
//   3. la PROMOTION des sous-objets geometriques du gothique en ports : le port
//      doit rendre EXACTEMENT ce que rendait la position du menu qu'il remplace,
//      puis aller au-dela.

using namespace cggraph;
using namespace cggraph_nodes;

namespace {

std::shared_ptr<const Mesh> EvaluateShape (Node *node)
{
	Graph graph;
	const NodeId id = graph.AddNode (std::unique_ptr<Node> (node));
	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	if (!evaluator.Evaluate (id, outputs, ctx).IsOk ())
		return nullptr;
	if (outputs.empty ())
		return nullptr;
	return outputs[0].Share<Mesh> (Types ().mesh);
}

// Empreinte grossiere d'un jeu de contours : le nombre total de points. Elle
// suffit ici -- deux supports de FORMES differentes ne donnent pas le meme
// nombre de points --, et elle ne depend d'aucune tolerance flottante.
std::size_t PointCount (const std::vector<ExtrudeContour> &contours)
{
	std::size_t total = 0;
	for (const ExtrudeContour &contour : contours)
		total += contour.pts.size ();
	return total;
}

float ZExtent (const Mesh &mesh)
{
	const std::vector<float> &v = mesh.GetVertices ();
	if (v.size () < 3)
		return 0.0f;
	float lo = v[2], hi = v[2];
	for (std::size_t i = 2; i < v.size (); i += 3)
	{
		lo = std::min (lo, v[i]);
		hi = std::max (hi, v[i]);
	}
	return hi - lo;
}

std::vector<unsigned char> ReadAllBytes (const std::string &path)
{
	std::vector<unsigned char> bytes;
	FILE *file = std::fopen (path.c_str (), "rb");
	if (file == nullptr)
		return bytes;
	unsigned char chunk[8192];
	std::size_t read = 0;
	while ((read = std::fread (chunk, 1, sizeof (chunk), file)) > 0)
		bytes.insert (bytes.end (), chunk, chunk + read);
	std::fclose (file);
	return bytes;
}

std::unique_ptr<Node> MakeShapeNode (const char *typeName)
{
	return MakeNode (typeName);
}

void BBox (const Mesh &mesh, float lo[3], float hi[3])
{
	for (int k = 0; k < 3; ++k) { lo[k] = 1e30f; hi[k] = -1e30f; }
	for (unsigned int v = 0; v < mesh.GetNVertices (); ++v)
	{
		float p[3];
		const_cast<Mesh &> (mesh).GetVertex (v, p);
		for (int k = 0; k < 3; ++k)
		{
			lo[k] = std::min (lo[k], p[k]);
			hi[k] = std::max (hi[k], p[k]);
		}
	}
}

}  // namespace

// ---------------------------------------------------------------------------
//  Frontiere de l'adaptateur generique
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_shapes, the_generic_adapter_covers_the_shape_catalogue_except_the_gothic_window)
{
	// Filet de DERIVE entre deux listes qui vivent dans deux bibliotheques :
	// les fabriques dans cgmesh, les identifiants de noeud ici. Une forme
	// ajoutee la-bas sans liaison ici fait rougir ce cas -- c'est la seule
	// chose qui empeche les deux de s'ecarter en silence.
	std::set<std::string> bound;
	for (const ShapeBinding &binding : ShapeBindings ())
	{
		EXPECT_TRUE (bound.insert (binding.shapeName).second)
			<< "deux liaisons pour la meme forme : " << binding.shapeName;
		EXPECT_NE (FindParametricShape (binding.shapeName), nullptr)
			<< "liaison vers une forme inconnue de cgmesh : " << binding.shapeName;
	}

	for (const ParametricShapeEntry &shape : ParametricShapes ())
	{
		if (std::string (shape.name) == "Gothic Window")
		{
			// La seule exception, et elle est NOMMEE : sous-objets geometriques,
			// donc descripteur ecrit a la main.
			EXPECT_EQ (bound.count (shape.name), 0u);
			continue;
		}
		EXPECT_EQ (bound.count (shape.name), 1u) << "forme sans noeud : " << shape.name;
	}

	EXPECT_EQ (ParametricShapes ().size (), 27u);
	EXPECT_EQ (ShapeBindings ().size (), 26u);
}

TEST (TEST_cggraph_nodes_shapes, no_shape_node_wraps_a_form_that_reads_a_file)
{
	// Le critere, dans les deux sens. Un noeud produit par l'adaptateur
	// generique n'a AUCUN port d'entree : si sa forme consommait un fichier,
	// cette ressource n'entrerait dans aucune signature et le cache resservirait
	// un resultat perime sans jamais planter.
	//
	// Les cinq formes a ressource de constructeur, relevees a la lecture des
	// declarations de parameterized_shapes.h. Instrument :
	//   grep -n "explicit Parameterized.*(const std::string" -> quatre, plus
	//   ParameterizedText3D dont le constructeur a deux arguments.
	const char *withResource[] = { "SVG extrusion", "Image quantification",
	                               "Image pixel blocks", "Text 3D",
	                               "Implicit (point cloud)" };
	for (const char *name : withResource)
	{
		EXPECT_EQ (FindParametricShape (name), nullptr)
			<< name << " ne doit pas figurer au catalogue de formes SANS ressource";
		for (const ShapeBinding &binding : ShapeBindings ())
			EXPECT_NE (std::string (binding.shapeName), name);
	}

	// CONTROLE POSITIF : sans lui, la boucle ci-dessus passerait tout aussi bien
	// sur un catalogue VIDE.
	EXPECT_NE (FindParametricShape ("Cube"), nullptr);

	for (const ShapeBinding &binding : ShapeBindings ())
	{
		std::unique_ptr<Node> node = MakeShapeNode (binding.typeName);
		ASSERT_NE (node, nullptr) << binding.typeName;
		EXPECT_TRUE (node->GetDesc ().inputs.empty ()) << binding.typeName;
		ASSERT_EQ (node->GetDesc ().outputs.size (), 1u) << binding.typeName;
		EXPECT_EQ (node->GetDesc ().outputs[0].type, Types ().mesh) << binding.typeName;
	}

	// Et le noeud a ressource du catalogue -- le texte -- en a bien un.
	std::unique_ptr<Node> text = MakeNode ("text.contours");
	ASSERT_NE (text, nullptr);
	EXPECT_FALSE (text->GetDesc ().inputs.empty ());
}

// ---------------------------------------------------------------------------
//  Resultat de chaque noeud produit
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_shapes, every_shape_node_produces_a_finite_non_degenerate_mesh)
{
	for (const ShapeBinding &binding : ShapeBindings ())
	{
		std::unique_ptr<Node> node = MakeShapeNode (binding.typeName);
		ASSERT_NE (node, nullptr) << binding.typeName;
		std::shared_ptr<const Mesh> mesh = EvaluateShape (node.release ());
		ASSERT_NE (mesh, nullptr) << binding.typeName;
		EXPECT_GT (mesh->GetNVertices (), 0u) << binding.typeName;
		EXPECT_GT (mesh->GetNFaces (), 0u) << binding.typeName;

		float lo[3], hi[3];
		BBox (*mesh, lo, hi);
		for (int k = 0; k < 3; ++k)
		{
			EXPECT_TRUE (std::isfinite (lo[k])) << binding.typeName;
			EXPECT_TRUE (std::isfinite (hi[k])) << binding.typeName;
		}
		// Une forme reduite a un point n'est pas une forme.
		EXPECT_GT ((hi[0]-lo[0]) + (hi[1]-lo[1]) + (hi[2]-lo[2]), 0.f) << binding.typeName;
	}
}

TEST (TEST_cggraph_nodes_shapes, a_parameter_really_reaches_the_wrapped_body)
{
	// Le cube, parce que son parametre a une valeur MESURABLE : CreateCube rend
	// une arete de 2, que Regenerate met a l'echelle demandee.
	std::unique_ptr<Node> node = MakeShapeNode ("shape.cube");
	ASSERT_NE (node, nullptr);
	node->GetParams ().SetFloat ("Edge length", 3.0f);
	std::shared_ptr<const Mesh> mesh = EvaluateShape (node.release ());
	ASSERT_NE (mesh, nullptr);

	float lo[3], hi[3];
	BBox (*mesh, lo, hi);
	for (int k = 0; k < 3; ++k)
		EXPECT_NEAR (hi[k] - lo[k], 3.0f, 1e-5f);

	// Et la valeur par defaut n'est pas la meme : sans ce second point, le cas
	// passerait sur un adaptateur qui ignorerait le parametre et rendrait par
	// chance la bonne taille.
	std::unique_ptr<Node> other = MakeShapeNode ("shape.cube");
	ASSERT_NE (other, nullptr);
	std::shared_ptr<const Mesh> plain = EvaluateShape (other.release ());
	ASSERT_NE (plain, nullptr);
	float lo2[3], hi2[3];
	BBox (*plain, lo2, hi2);
	EXPECT_NEAR (hi2[0] - lo2[0], 1.0f, 1e-5f);
}

TEST (TEST_cggraph_nodes_shapes, the_adapter_applies_the_bounds_that_the_param_set_cannot_carry)
{
	// ParamSet ne porte pas de bornes ; Parameter en porte. Sans reprise par
	// l'adaptateur, « nu = 10000 » construirait 10 000 x 20 sommets.
	std::unique_ptr<Node> clamped = MakeShapeNode ("shape.sphere");
	ASSERT_NE (clamped, nullptr);
	clamped->GetParams ().SetInt ("nu", 10000);
	std::shared_ptr<const Mesh> a = EvaluateShape (clamped.release ());
	ASSERT_NE (a, nullptr);

	std::unique_ptr<Node> atMax = MakeShapeNode ("shape.sphere");
	ASSERT_NE (atMax, nullptr);
	atMax->GetParams ().SetInt ("nu", 200);      // la borne haute declaree
	std::shared_ptr<const Mesh> b = EvaluateShape (atMax.release ());
	ASSERT_NE (b, nullptr);

	EXPECT_EQ (a->GetNVertices (), b->GetNVertices ());

	// Controle positif : en dessous de la borne, la valeur agit.
	std::unique_ptr<Node> small = MakeShapeNode ("shape.sphere");
	ASSERT_NE (small, nullptr);
	small->GetParams ().SetInt ("nu", 20);
	std::shared_ptr<const Mesh> c = EvaluateShape (small.release ());
	ASSERT_NE (c, nullptr);
	EXPECT_LT (c->GetNVertices (), b->GetNVertices ());
}

// ---------------------------------------------------------------------------
//  Les profils promus
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_shapes, a_profile_node_publishes_the_curve_its_parameters_describe)
{
	Graph graph;
	const NodeId id = graph.AddNode (MakeNode ("profile.chamfer"));
	Node *node = graph.FindNode (id);
	ASSERT_NE (node, nullptr);
	node->GetParams ().SetFloat ("width", 2.0f);
	node->GetParams ().SetFloat ("depth", 4.0f);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (id, outputs, ctx).IsOk ());
	ASSERT_EQ (outputs.size (), 1u);

	const Profile2D *profile = outputs[0].Get<Profile2D> (Types ().splayProfile);
	ASSERT_NE (profile, nullptr);
	ASSERT_EQ (profile->points.size (), 2u);
	EXPECT_EQ (profile->points[0].x, 0.0);
	EXPECT_EQ (profile->points[1].x, 4.0);
	EXPECT_EQ (profile->points[1].y, 2.0);

	// Et il ne se lit PAS comme une section de barre.
	EXPECT_EQ (outputs[0].Get<Profile2D> (Types ().barProfile), nullptr);
}

TEST (TEST_cggraph_nodes_shapes, a_bar_profile_cannot_be_plugged_into_a_splay_port)
{
	Graph graph;
	const NodeId bar = graph.AddNode (MakeNode ("profile.bar.roll"));
	const NodeId splay = graph.AddNode (MakeNode ("profile.chamfer"));
	const NodeId window = graph.AddNode (MakeNode ("shape.gothic.window"));

	// Refus : une section fermee n'est pas un ebrasement.
	EXPECT_EQ (graph.Connect (bar, 0, window, 0), ConnectStatus::TypeMismatch);
	// Acceptations, sans quoi le refus ci-dessus ne prouverait rien.
	EXPECT_EQ (graph.Connect (splay, 0, window, 0), ConnectStatus::Ok);
	EXPECT_EQ (graph.Connect (bar, 0, window, 1), ConnectStatus::Ok);
}

// ---------------------------------------------------------------------------
//  Le gothique a sous-objets promus
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_shapes, the_gothic_node_replaces_the_profile_menu_by_two_ports)
{
	std::unique_ptr<Node> node = MakeNode ("shape.gothic.window");
	ASSERT_NE (node, nullptr);

	// Le parametre du menu n'est PAS declare : deux mecanismes pour une meme
	// chose, c'est un mensonge que la UI rendrait credible.
	EXPECT_EQ (node->GetParams ().Find ("Profile"), nullptr);

	// Les 21 autres, eux, sont bien la.
	EXPECT_EQ (node->GetParams ().GetCount (), 21u);
	EXPECT_NE (node->GetParams ().Find ("Width"), nullptr);
	EXPECT_NE (node->GetParams ().Find ("Rosette foil count"), nullptr);

	ASSERT_EQ (node->GetDesc ().inputs.size (), 2u);
	EXPECT_TRUE (node->GetDesc ().inputs[0].optional);
	EXPECT_TRUE (node->GetDesc ().inputs[1].optional);
	EXPECT_EQ (node->GetDesc ().inputs[0].type, Types ().splayProfile);
	EXPECT_EQ (node->GetDesc ().inputs[1].type, Types ().barProfile);
}

TEST (TEST_cggraph_nodes_shapes, an_unfed_profile_port_gives_back_the_flat_position)
{
	std::shared_ptr<const Mesh> fromNode = EvaluateShape (MakeNode ("shape.gothic.window").release ());
	ASSERT_NE (fromNode, nullptr);

	// La forme paramerie, menu sur « Flat » -- son defaut.
	ParameterizedGothicWindow shape;
	shape.Regenerate ();
	std::unique_ptr<Mesh> reference (shape.TakeMesh ());
	ASSERT_NE (reference, nullptr);

	EXPECT_EQ (fromNode->GetNVertices (), reference->GetNVertices ());
	EXPECT_EQ (fromNode->GetNFaces (), reference->GetNFaces ());
}

TEST (TEST_cggraph_nodes_shapes, the_chamfer_port_reproduces_the_menu_position_it_replaces)
{
	// C'est LE cas qui etablit que la promotion n'a rien perdu : brancher un
	// profil de chanfrein neuf sur une baie neuve rend exactement ce que rendait
	// la position « Chamfer » du menu, dont les valeurs se derivaient des
	// offsets par defaut.
	Graph graph;
	const NodeId profile = graph.AddNode (MakeNode ("profile.chamfer"));
	const NodeId window = graph.AddNode (MakeNode ("shape.gothic.window"));
	ASSERT_EQ (graph.Connect (profile, 0, window, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (window, outputs, ctx).IsOk ());
	std::shared_ptr<const Mesh> fromPort = outputs[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (fromPort, nullptr);

	ParameterizedGothicWindow shape;
	for (Parameter &p : shape.GetParameters ())
		if (p.GetName () == "Profile")
			p.SetInt (1);                       // Chamfer
	shape.Regenerate ();
	std::unique_ptr<Mesh> reference (shape.TakeMesh ());
	ASSERT_NE (reference, nullptr);

	ASSERT_EQ (fromPort->GetNVertices (), reference->GetNVertices ());
	ASSERT_EQ (fromPort->GetNFaces (), reference->GetNFaces ());
	for (unsigned int v = 0; v < reference->GetNVertices (); ++v)
	{
		float a[3], b[3];
		const_cast<Mesh &> (*fromPort).GetVertex (v, a);
		reference->GetVertex (v, b);
		for (int k = 0; k < 3; ++k)
			EXPECT_FLOAT_EQ (a[k], b[k]) << "sommet " << v;
	}
}

TEST (TEST_cggraph_nodes_shapes, a_cavetto_port_goes_where_no_menu_position_could)
{
	// Et voici le gain : une moulure que les cinq positions ne savaient pas
	// dire. Elle doit produire une geometrie DIFFERENTE du chanfrein, sur la
	// meme baie.
	Graph graph;
	const NodeId chamfer = graph.AddNode (MakeNode ("profile.chamfer"));
	const NodeId cavetto = graph.AddNode (MakeNode ("profile.cavetto"));
	const NodeId a = graph.AddNode (MakeNode ("shape.gothic.window"));
	const NodeId b = graph.AddNode (MakeNode ("shape.gothic.window"));
	ASSERT_EQ (graph.Connect (chamfer, 0, a, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (cavetto, 0, b, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outA, outB;
	ASSERT_TRUE (evaluator.Evaluate (a, outA, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (b, outB, ctx).IsOk ());
	std::shared_ptr<const Mesh> meshA = outA[0].Share<Mesh> (Types ().mesh);
	std::shared_ptr<const Mesh> meshB = outB[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (meshA, nullptr);
	ASSERT_NE (meshB, nullptr);

	// Le cavet ajoute cinq anneaux par contour : plus de sommets, et pas les
	// memes.
	EXPECT_GT (meshB->GetNVertices (), meshA->GetNVertices ());
}

TEST (TEST_cggraph_nodes_shapes, a_bar_profile_port_reproduces_its_menu_position_too)
{
	Graph graph;
	const NodeId profile = graph.AddNode (MakeNode ("profile.bar.roll"));
	const NodeId window = graph.AddNode (MakeNode ("shape.gothic.window"));
	ASSERT_EQ (graph.Connect (profile, 0, window, 1), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (window, outputs, ctx).IsOk ());
	std::shared_ptr<const Mesh> fromPort = outputs[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (fromPort, nullptr);

	// ⚠ Une egalite avec la position du menu ne suffit PAS : les deux chemins
	// partagent desormais le meme placement, donc un defaut de placement les
	// deplacerait TOUS LES DEUX et l'egalite tiendrait encore. Il faut donc une
	// propriete ABSOLUE, et la voici : une barre est SAILLANTE -- elle depasse
	// la face avant de la baie, dont la profondeur d'extrusion vaut 20 par
	// defaut. Sans le placement, la moulure serait balayee au fond de la piece.
	float zMax = -1e30f;
	for (unsigned int v = 0; v < fromPort->GetNVertices (); ++v)
	{
		float p[3];
		const_cast<Mesh &> (*fromPort).GetVertex (v, p);
		zMax = std::max (zMax, p[2]);
	}
	EXPECT_GT (zMax, 20.f) << "la barre ne fait plus saillie sur la face avant";

	ParameterizedGothicWindow shape;
	for (Parameter &p : shape.GetParameters ())
		if (p.GetName () == "Profile")
			p.SetInt (2);                       // Roll bar
	shape.Regenerate ();
	std::unique_ptr<Mesh> reference (shape.TakeMesh ());
	ASSERT_NE (reference, nullptr);

	ASSERT_EQ (fromPort->GetNVertices (), reference->GetNVertices ());
	for (unsigned int v = 0; v < reference->GetNVertices (); ++v)
	{
		float a[3], b[3];
		const_cast<Mesh &> (*fromPort).GetVertex (v, a);
		reference->GetVertex (v, b);
		for (int k = 0; k < 3; ++k)
			EXPECT_FLOAT_EQ (a[k], b[k]) << "sommet " << v;
	}
}

// ---------------------------------------------------------------------------
//  Cache -- ce que la promotion achete
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_shapes, changing_the_profile_moves_the_signature_of_the_window)
{
	Graph graph;
	const NodeId profile = graph.AddNode (MakeNode ("profile.chamfer"));
	const NodeId window = graph.AddNode (MakeNode ("shape.gothic.window"));
	ASSERT_EQ (graph.Connect (profile, 0, window, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (window, outputs, ctx).IsOk ());
	const Hash before = evaluator.GetSignature (window);

	graph.FindNode (profile)->GetParams ().SetFloat ("width", 3.0f);
	ASSERT_TRUE (evaluator.Evaluate (window, outputs, ctx).IsOk ());
	EXPECT_NE (evaluator.GetSignature (window), before)
		<< "un profil amont modifie doit invalider la baie";
}

TEST (TEST_cggraph_nodes_shapes, a_bound_that_depends_on_another_parameter_is_read_after_it_is_set)
{
	// Le L-systeme est le seul du catalogue dont une borne DEPENDE d'un autre
	// parametre : le plafond d'iterations est celui du systeme COURANT -- six
	// pour le defaut (Hilbert), neuf pour la courbe du dragon. Un adaptateur qui
	// releverait la liste des parametres UNE fois, avant de rien poser,
	// appliquerait a « Iterations » le plafond du systeme par defaut et
	// ramenerait 8 a 6 -- silencieusement, et sur une valeur pourtant licite.
	//
	// Sans ce cas, la relecture de la liste a chaque parametre serait un
	// mecanisme qu'aucune fixture n'atteint.
	const int kDragonCurve = 6;      // LSYSTEM_DRAGON_CURVE, plafond 9

	std::unique_ptr<Node> deep = MakeShapeNode ("shape.lsystem");
	ASSERT_NE (deep, nullptr);
	deep->GetParams ().SetInt ("System", kDragonCurve);
	deep->GetParams ().SetInt ("Iterations", 8);
	std::shared_ptr<const Mesh> a = EvaluateShape (deep.release ());
	ASSERT_NE (a, nullptr);

	std::unique_ptr<Node> shallow = MakeShapeNode ("shape.lsystem");
	ASSERT_NE (shallow, nullptr);
	shallow->GetParams ().SetInt ("System", kDragonCurve);
	shallow->GetParams ().SetInt ("Iterations", 6);
	std::shared_ptr<const Mesh> b = EvaluateShape (shallow.release ());
	ASSERT_NE (b, nullptr);

	EXPECT_GT (a->GetNVertices (), b->GetNVertices ())
		<< "8 iterations ont ete ramenees au plafond du systeme par DEFAUT";
}

TEST (TEST_cggraph_nodes_shapes, an_enum_out_of_range_is_brought_back_to_the_last_choice)
{
	// MakeEnum ne pose ni min ni max : la borne est la LISTE de choix, et c'est
	// l'adaptateur qui la fait respecter. Sans lui, un indice hors liste
	// atteindrait le corps emballe, qui cherche son systeme dans une table et
	// rend alors une figure vide sans le dire.
	std::unique_ptr<Node> reference = MakeShapeNode ("shape.lsystem");
	ASSERT_NE (reference, nullptr);
	ParameterizedLSystem probe;
	int lastChoice = 0;
	for (Parameter &p : probe.GetParameters ())
		if (p.GetName () == "System")
			lastChoice = static_cast<int> (p.GetChoices ().size ()) - 1;
	ASSERT_GT (lastChoice, 0);

	reference->GetParams ().SetInt ("System", lastChoice);
	std::shared_ptr<const Mesh> a = EvaluateShape (reference.release ());
	ASSERT_NE (a, nullptr);

	std::unique_ptr<Node> overflow = MakeShapeNode ("shape.lsystem");
	ASSERT_NE (overflow, nullptr);
	overflow->GetParams ().SetInt ("System", lastChoice + 500);
	std::shared_ptr<const Mesh> b = EvaluateShape (overflow.release ());
	ASSERT_NE (b, nullptr);

	EXPECT_EQ (a->GetNVertices (), b->GetNVertices ());
	EXPECT_GT (a->GetNVertices (), 0u);
}

TEST (TEST_cggraph_nodes_shapes, the_text_node_bounds_its_support_selector)
{
	// Le selecteur de support est une enumeration a quatre valeurs cote cgmesh.
	// Un entier hors plage n'a pas de sens ; l'adaptateur le borne, plutot que
	// de convertir n'importe quoi en valeur d'enumeration.
	std::vector<unsigned char> bytes = ReadAllBytes ("./test/data/fonts/DejaVuSans.ttf");
	ASSERT_FALSE (bytes.empty ());

	Graph graph;
	const NodeId fontId = graph.AddNode (MakeNode ("text.font.load"));
	static_cast<LoadFontNode *> (graph.FindNode (fontId))->SetBytes (std::move (bytes));

	// Le support vit sur text.contours, PAS sur l'extrudeur : c'est un contour
	// de plus, fondu aux lettres par la meme union 2D. On compare donc les
	// contours, et non un maillage -- une etape de moins entre le reglage et ce
	// que le cas observe.
	const NodeId frame = graph.AddNode (MakeNode ("text.contours"));
	const NodeId overflow = graph.AddNode (MakeNode ("text.contours"));
	ASSERT_EQ (graph.Connect (fontId, 0, frame, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (fontId, 0, overflow, 0), ConnectStatus::Ok);
	for (NodeId id : { frame, overflow })
	{
		graph.FindNode (id)->GetParams ().SetString ("text", "on");
		graph.FindNode (id)->GetParams ().SetFloat ("supportThickness", 0.05f);
		graph.FindNode (id)->GetParams ().SetFloat ("supportMargin", 0.1f);
	}
	graph.FindNode (frame)->GetParams ().SetInt ("support", 3);       // Frame
	graph.FindNode (overflow)->GetParams ().SetInt ("support", 99);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outA, outB;
	ASSERT_TRUE (evaluator.Evaluate (frame, outA, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (overflow, outB, ctx).IsOk ());
	typedef std::vector<ExtrudeContour> Contours;
	std::shared_ptr<const Contours> a = outA[0].Share<Contours> (Types ().extrudeContours);
	std::shared_ptr<const Contours> b = outB[0].Share<Contours> (Types ().extrudeContours);
	ASSERT_NE (a, nullptr);
	ASSERT_NE (b, nullptr);
	EXPECT_EQ (PointCount (*a), PointCount (*b));

	// Controle positif : un support de forme differente ne rend pas les memes
	// contours, sans quoi l'egalite ci-dessus ne prouverait rien.
	const NodeId plate = graph.AddNode (MakeNode ("text.contours"));
	ASSERT_EQ (graph.Connect (fontId, 0, plate, 0), ConnectStatus::Ok);
	graph.FindNode (plate)->GetParams ().SetString ("text", "on");
	graph.FindNode (plate)->GetParams ().SetInt ("support", 1);       // Plate
	ValueList outC;
	ASSERT_TRUE (evaluator.Evaluate (plate, outC, ctx).IsOk ());
	std::shared_ptr<const Contours> c = outC[0].Share<Contours> (Types ().extrudeContours);
	ASSERT_NE (c, nullptr);
	EXPECT_NE (PointCount (*a), PointCount (*c));
}

TEST (TEST_cggraph_nodes_shapes, a_boolean_parameter_reaches_the_wrapped_body_too)
{
	// Le cube expose « Triangulated ». A faux, CreateCube emet des QUADRILATERES
	// -- deux fois moins de faces pour les memes huit sommets. Sans ce cas, la
	// branche booleenne de l'adaptateur ne serait atteinte par aucune fixture.
	std::unique_ptr<Node> tri = MakeShapeNode ("shape.cube");
	ASSERT_NE (tri, nullptr);
	tri->GetParams ().SetBool ("Triangulated", true);
	std::shared_ptr<const Mesh> a = EvaluateShape (tri.release ());
	ASSERT_NE (a, nullptr);

	std::unique_ptr<Node> quad = MakeShapeNode ("shape.cube");
	ASSERT_NE (quad, nullptr);
	quad->GetParams ().SetBool ("Triangulated", false);
	std::shared_ptr<const Mesh> b = EvaluateShape (quad.release ());
	ASSERT_NE (b, nullptr);

	EXPECT_EQ (a->GetNVertices (), b->GetNVertices ());
	EXPECT_EQ (a->GetNFaces (), 12u);
	EXPECT_EQ (b->GetNFaces (), 6u);
}

TEST (TEST_cggraph_nodes_shapes, all_five_profile_nodes_publish_a_usable_profile)
{
	// Les cinq, un par un : sans cette boucle, l'arete et la doucine ne seraient
	// evaluees par aucun cas -- elles ne figurent pas au catalogue des formes,
	// donc la boucle qui parcourt les liaisons ne les atteint pas.
	struct Expected { const char *typeName; bool splay; std::size_t points; };
	const Expected expected[] = {
		{ "profile.chamfer",  true,  2u },
		{ "profile.cavetto",  true,  7u },
		{ "profile.bar.roll", false, 13u },
		{ "profile.bar.keel", false, 3u },
		{ "profile.bar.ogee", false, 13u }
	};

	for (const Expected &want : expected)
	{
		Graph graph;
		const NodeId id = graph.AddNode (MakeNode (want.typeName));
		Evaluator evaluator (graph);
		EvalContext ctx;
		ValueList outputs;
		ASSERT_TRUE (evaluator.Evaluate (id, outputs, ctx).IsOk ()) << want.typeName;
		ASSERT_EQ (outputs.size (), 1u) << want.typeName;

		const TypeDesc *type = want.splay ? Types ().splayProfile : Types ().barProfile;
		const Profile2D *profile = outputs[0].Get<Profile2D> (type);
		ASSERT_NE (profile, nullptr) << want.typeName;
		EXPECT_EQ (profile->points.size (), want.points) << want.typeName;
		// Un ebrasement part de l'origine ; une barre est une section fermee,
		// symetrique en v.
		if (want.splay)
		{
			EXPECT_EQ (profile->points[0].x, 0.0) << want.typeName;
			EXPECT_EQ (profile->points[0].y, 0.0) << want.typeName;
			EXPECT_GT (profile->points.back ().y, 0.0) << want.typeName;
		}
		else
		{
			EXPECT_NEAR (profile->points.front ().y, -profile->points.back ().y, 1e-12)
				<< want.typeName;
		}
	}
}

// ---------------------------------------------------------------------------
//  Le SECOND producteur de contours, et l'extrudeur qu'il PARTAGE avec le texte
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_shapes, the_svg_producer_feeds_the_same_extruder_as_the_text_one)
{
	// C'EST LA RAISON D'ETRE du decoupage : shape.extrude ne sait pas d'ou
	// viennent ses contours. Le cas le prouve en le branchant sur svg.contours,
	// alors que les cas de texte le branchent sur text.contours -- le meme
	// extrudeur, jamais recompile pour l'occasion.
	Graph graph;
	const NodeId fileId = graph.AddNode (MakeNode ("file.ref"));
	ASSERT_NE (graph.FindNode (fileId), nullptr);
	static_cast<FileRefNode *> (graph.FindNode (fileId))->SetPath ("./test/data/svg/square.svg");

	const NodeId contours = graph.AddNode (MakeNode ("svg.contours"));
	const NodeId extrude = graph.AddNode (MakeNode ("shape.extrude"));
	ASSERT_NE (graph.FindNode (contours), nullptr);
	ASSERT_NE (graph.FindNode (extrude), nullptr);
	ASSERT_EQ (graph.Connect (fileId, 0, contours, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (contours, 0, extrude, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList out;
	const EvalResult result = evaluator.Evaluate (extrude, out, ctx);
	ASSERT_EQ (result.status, EvalStatus::Ok) << result.detail;
	std::shared_ptr<const Mesh> mesh = out[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (mesh, nullptr);
	EXPECT_GT (mesh->GetNVertices (), 0u);
	EXPECT_GT (mesh->GetNFaces (), 0u);

	// `depth` vit sur l'extrudeur et NULLE PART ailleurs : svg.contours n'expose
	// pas `height`, qui serait un curseur sans effet. On verifie donc que c'est
	// bien l'extrudeur qui decide de l'epaisseur.
	EXPECT_EQ (graph.FindNode (contours)->GetParams ().Find ("height"), nullptr);

	// Etendue en z mesuree sur les positions, et non par bbox() : celle-ci est
	// une derivation MISE EN CACHE sans detection de peremption (cf. mesh.h),
	// donc vide sur un maillage jamais passe par computebbox().
	const float thin = ZExtent (*mesh);
	graph.FindNode (extrude)->GetParams ().SetFloat ("depth", 2.0f);
	ValueList thicker;
	ASSERT_TRUE (evaluator.Evaluate (extrude, thicker, ctx).IsOk ());
	std::shared_ptr<const Mesh> second = thicker[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (second, nullptr);
	EXPECT_GT (ZExtent (*second), thin);
}

TEST (TEST_cggraph_nodes_shapes, an_svg_hole_survives_the_flat_contour_list)
{
	// Une liste PLATE de contours ne transporte pas la regle de remplissage de
	// chaque forme : svg.contours la resout en amont, par Clipper2. Sans cette
	// passe, le creux du « a » de rose.svg se remplirait -- et le maillage aurait
	// moins de sommets, pas plus.
	Graph graph;
	const NodeId fileId = graph.AddNode (MakeNode ("file.ref"));
	static_cast<FileRefNode *> (graph.FindNode (fileId))->SetPath ("./test/data/svg/rose.svg");
	const NodeId contours = graph.AddNode (MakeNode ("svg.contours"));
	ASSERT_EQ (graph.Connect (fileId, 0, contours, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList out;
	ASSERT_TRUE (evaluator.Evaluate (contours, out, ctx).IsOk ());
	typedef std::vector<ExtrudeContour> Contours;
	std::shared_ptr<const Contours> produced = out[0].Share<Contours> (Types ().extrudeContours);
	ASSERT_NE (produced, nullptr);

	// Plus d'un contour : les trous sont des contours a part entiere, et c'est
	// exactement ce que la resolution en amont produit.
	EXPECT_GT (produced->size (), 1u);
	EXPECT_GT (PointCount (*produced), 0u);
}

// Un chemin qui ne mene nulle part est un ECHEC, pas un jeu de contours vide :
// une forme vide se propagerait en silence jusqu'a un maillage nul.
TEST (TEST_cggraph_nodes_shapes, the_svg_producer_refuses_a_path_that_leads_nowhere)
{
	Graph graph;
	const NodeId fileId = graph.AddNode (MakeNode ("file.ref"));
	static_cast<FileRefNode *> (graph.FindNode (fileId))->SetPath ("./test/data/svg/aucun_fichier.svg");
	const NodeId contours = graph.AddNode (MakeNode ("svg.contours"));
	ASSERT_EQ (graph.Connect (fileId, 0, contours, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList out;
	EXPECT_FALSE (evaluator.Evaluate (contours, out, ctx).IsOk ());
}

// ---------------------------------------------------------------------------
//  svg.contours -- la taille de la piece, en millimetres
// ---------------------------------------------------------------------------
// PREMIER filet de ce noeud : il n'en avait aucun avant `fitSize`.
//
// Sans `fitSize`, svg_to_contours rend un dessin normalise a 1.0 sous
// `centerAndFit` -- une echelle qui ne designe aucune longueur reelle. Le
// parametre en fait une cote, et c'est cette implication que le cas verifie :
// la valeur demandee EST la plus grande dimension du resultat.
//
// Contrairement aux blocs et au relief, il n'y a pas ici d'unite plus petite
// dont la taille decoulerait : un dessin vectoriel n'a pas de grain.
TEST (TEST_cggraph_nodes_shapes, the_svg_fit_size_is_the_largest_dimension_in_millimetres)
{
	cggraph::Graph graph;
	const cggraph::NodeId fileId =
		graph.AddNode (std::unique_ptr<cggraph::Node> (new cggraph_nodes::FileRefNode ()));
	static_cast<cggraph_nodes::FileRefNode *> (graph.FindNode (fileId))
		->SetPath ("./test/data/svg/rose.svg");

	const cggraph::NodeId svgId =
		graph.AddNode (std::unique_ptr<cggraph::Node> (new cggraph_nodes::SvgContoursNode ()));
	ASSERT_EQ (graph.Connect (fileId, 0, svgId, 0), cggraph::ConnectStatus::Ok);

	const auto largestSide = [&] (float fitSize) -> float {
		graph.FindNode (svgId)->GetParams ().SetFloat ("fitSize", fitSize);
		cggraph::Evaluator evaluator (graph);
		cggraph::EvalContext ctx;
		cggraph::ValueList out;
		if (!evaluator.Evaluate (svgId, out, ctx).IsOk () || out.empty ())
			return -1.0f;
		const std::shared_ptr<const std::vector<ExtrudeContour>> contours =
			out[0].Share<std::vector<ExtrudeContour>> (cggraph_nodes::Types ().extrudeContours);
		if (contours == nullptr || contours->empty ())
			return -1.0f;

		bool any = false;
		float lo[2] = { 0.0f, 0.0f }, hi[2] = { 0.0f, 0.0f };
		for (const ExtrudeContour &c : *contours)
			for (const Vector2f &pt : c.pts)
			{
				if (!any) { lo[0] = hi[0] = pt.x; lo[1] = hi[1] = pt.y; any = true; continue; }
				if (pt.x < lo[0]) lo[0] = pt.x;
				if (pt.x > hi[0]) hi[0] = pt.x;
				if (pt.y < lo[1]) lo[1] = pt.y;
				if (pt.y > hi[1]) hi[1] = pt.y;
			}
		if (!any) return -1.0f;
		const float w = hi[0] - lo[0], h = hi[1] - lo[1];
		return w > h ? w : h;
	};

	const float hundred = largestSide (100.0f);
	ASSERT_GT (hundred, 0.0f);
	EXPECT_NEAR (hundred, 100.0f, 1e-2f) << "la taille demandee EST le plus grand cote";

	// LA PROPRIETE, et non une valeur : la sortie suit la demande.
	const float forty = largestSide (40.0f);
	ASSERT_GT (forty, 0.0f);
	EXPECT_NEAR (forty, 40.0f, 1e-2f);

	// A ZERO, le noeud rend le dessin tel quel -- normalise a 1.0 par
	// centerAndFit. C'est ce qui garde lisibles les documents ecrits avant ce
	// parametre.
	const float none = largestSide (0.0f);
	ASSERT_GT (none, 0.0f);
	EXPECT_NEAR (none, 1.0f, 1e-3f);
}
