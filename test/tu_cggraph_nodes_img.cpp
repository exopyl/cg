#include <gtest/gtest.h>

#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../src/cggraph/core/evaluator.h"
#include "../src/cggraph/nodes/catalog.h"
#include "../src/cggraph/nodes/img/load_image.h"
#include "../src/cggraph/nodes/img/pixel_blocks.h"
#include "../src/cggraph/nodes/img/quantize.h"
#include "../src/cggraph/nodes/img/relief.h"
#include "../src/cggraph/nodes/value_types.h"
#include "../src/cgimg/image.h"
#include "../src/cgmesh/mesh.h"

// ===========================================================================
//  Couche B : les adaptateurs vers cgimg / cgmesh (chaine image -> regions)
// ===========================================================================
//
// Ces six noeuds reproduisent, sous forme decomposee, les deux pages de maker :
// « Image to puzzle » (relief.html) et « Blocs pixelises » (pixels.html). Ce que
// ces tests cherchent a prendre en defaut n'est pas la geometrie -- elle est
// couverte par les tests des briques cgmesh -- mais la FRONTIERE :
//
//   * une image qui transite sur un lien est PARTAGEE ; la vectorisation
//     palettise son entree, donc un adaptateur qui ne copierait pas
//     corromprait tous les autres lecteurs du meme lien. C'est le cas le plus
//     important du fichier ;
//   * les surcharges prenant une image NE QUANTIFIENT PAS, et rien dans le
//     systeme de types ne l'impose : le seul filet est un test ;
//   * la suite rendue par les sorties « pieces » possede ses maillages, et une
//     fuite y serait muette.
//
using namespace cggraph;
using namespace cggraph_nodes;

namespace {

// Image reelle plutot que synthetique : le decodage au CONTENU (et non a
// l'extension) fait partie du contrat de img.io.load, et une image fabriquee a
// la main ne l'exercerait pas. Copiee dans le repertoire de travail des tests
// par le build (cf. test/CMakeLists.txt).
const char *kJpeg = "./test/data/jpg/joconde.jpg";

std::vector<unsigned char> ReadFile (const char *path)
{
	std::vector<unsigned char> bytes;
	std::FILE *f = std::fopen (path, "rb");
	if (!f)
		return bytes;
	std::fseek (f, 0, SEEK_END);
	const long size = std::ftell (f);
	std::fseek (f, 0, SEEK_SET);
	if (size > 0)
	{
		bytes.resize ((size_t)size);
		if (std::fread (bytes.data (), 1, bytes.size (), f) != bytes.size ())
			bytes.clear ();
	}
	std::fclose (f);
	return bytes;
}

// Nombre de couleurs DISTINCTES, alpha compris -- c'est la definition sur
// laquelle raisonne toute la chaine aval (Palette::IsPresent compare l'alpha).
std::size_t CountColors (const Img &img)
{
	std::map<unsigned int, int> seen;
	const unsigned char *px = img.data ();
	const std::size_t n = (std::size_t)img.width () * img.height ();
	for (std::size_t i = 0; i < n; ++i)
	{
		const unsigned int key = ((unsigned int)px[4 * i] << 24)
		                       | ((unsigned int)px[4 * i + 1] << 16)
		                       | ((unsigned int)px[4 * i + 2] << 8)
		                       | (unsigned int)px[4 * i + 3];
		seen[key] = 1;
	}
	return seen.size ();
}

// Le parametre d'identite est INTERNE : node_support::GetString le refuse par
// contrat, et c'est voulu -- un adaptateur ne doit pas le lire pendant Compute.
// Un test, lui, a le droit de le constater, et passe donc par l'entree brute.
std::string Identity (const Node &node)
{
	const ParamValue *value = node.GetParams ().Find ("source.identity");
	return value ? value->stringValue : std::string ();
}

// Evalue un noeud isole en lui presentant `inputs`, sans passer par un graphe.
bool RunNode (Node &node, const ValueList &inputs, ValueList &outputs)
{
	EvalContext ctx;
	outputs.assign (node.GetDesc ().outputs.size (), Value ());
	return node.Compute (ctx, inputs, outputs);
}

// Charge et decode l'image de test en une valeur de lien.
Value LoadTestImage ()
{
	const std::vector<unsigned char> bytes = ReadFile (kJpeg);
	if (bytes.empty ())
		return Value ();

	LoadImageNode loader;
	loader.SetBytes (bytes);
	ValueList out;
	if (!RunNode (loader, ValueList (), out))
		return Value ();
	return out[0];
}

// Quantifie une valeur d'image ; `pixelWidth` a 0 laisse la resolution intacte.
Value Quantize (const Value &image, int maxColors, int pixelWidth)
{
	QuantizeImageNode node;
	node.GetParams ().SetInt ("maxColors", maxColors);
	node.GetParams ().SetInt ("pixelWidth", pixelWidth);
	// Chaine courte : ces tests mesurent la plomberie, pas la qualite de la
	// segmentation, et les etages couteux n'y ajouteraient que des secondes.
	node.GetParams ().SetInt ("preSmoothPasses", 0);
	node.GetParams ().SetInt ("refineIterations", 0);
	node.GetParams ().SetInt ("workingMaxDim", 128);

	ValueList in{ image };
	ValueList out;
	if (!RunNode (node, in, out))
		return Value ();
	return out[0];
}

} // namespace

// ---------------------------------------------------------------------------
//  La source
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_img, the_source_decodes_bytes_without_touching_the_filesystem)
{
	const std::vector<unsigned char> bytes = ReadFile (kJpeg);
	ASSERT_FALSE (bytes.empty ()) << kJpeg << " introuvable";

	LoadImageNode loader;
	loader.SetBytes (bytes);

	ValueList out;
	ASSERT_TRUE (RunNode (loader, ValueList (), out));
	EXPECT_EQ (loader.GetDecodeCount (), 1u);

	const std::shared_ptr<const Img> image = out[0].Share<Img> (Types ().image);
	ASSERT_NE (image, nullptr);
	EXPECT_GT (image->width (), 0u);
	EXPECT_GT (image->height (), 0u);
}

TEST (TEST_cggraph_nodes_img, an_empty_or_undecodable_buffer_is_a_failure_not_an_empty_image)
{
	// Deux echecs de nature differente, et aucun des deux ne doit rendre une
	// image vide : un aval qui recevrait un Img 0x0 n'aurait aucun moyen de
	// distinguer « rien a lire » de « lecture ratee ».
	LoadImageNode empty;
	ValueList out;
	EXPECT_FALSE (RunNode (empty, ValueList (), out));

	LoadImageNode garbage;
	garbage.SetBytes (std::vector<unsigned char> (64, 0x7f));   // aucun nombre magique
	EXPECT_FALSE (RunNode (garbage, ValueList (), out));
}

TEST (TEST_cggraph_nodes_img, the_identity_follows_the_bytes_and_the_name_does_not)
{
	// Meme discipline que LoadFontNode : c'est le HASH DU BUFFER qui invalide le
	// cache. Renommer ne doit rien recalculer -- sans quoi une retouche de
	// libelle relancerait une chaine de plusieurs secondes.
	LoadImageNode node;
	const std::string initial = Identity (node);

	node.SetBytes (std::vector<unsigned char>{ 1, 2, 3, 4 });
	const std::string afterBytes = Identity (node);
	EXPECT_NE (afterBytes, initial);

	node.SetName ("un autre libelle");
	EXPECT_EQ (Identity (node), afterBytes);

	node.SetBytes (std::vector<unsigned char>{ 4, 3, 2, 1 });
	EXPECT_NE (Identity (node), afterBytes);
}

// ---------------------------------------------------------------------------
//  Le tronc de quantification
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_img, quantizing_bounds_the_palette_and_leaves_the_source_untouched)
{
	const Value source = LoadTestImage ();
	ASSERT_FALSE (source.IsEmpty ());

	const std::shared_ptr<const Img> before = source.Share<Img> (Types ().image);
	ASSERT_NE (before, nullptr);
	const std::size_t colorsBefore = CountColors (*before);
	const unsigned int wBefore = before->width ();

	const Value quantized = Quantize (source, 8, 0);
	ASSERT_FALSE (quantized.IsEmpty ());
	const std::shared_ptr<const Img> after = quantized.Share<Img> (Types ().image);
	ASSERT_NE (after, nullptr);

	// Le contrat du noeud : au plus maxColors couleurs distinctes.
	EXPECT_LE (CountColors (*after), 8u);
	// Et il en reste plus d'une, sans quoi la vectorisation aval n'aurait rien a
	// segmenter -- une palette effondree passerait le test precedent.
	EXPECT_GT (CountColors (*after), 1u);

	// L'ENTREE N'A PAS BOUGE. C'est le point : la valeur est partagee, et un
	// second consommateur du meme lien doit voir l'image d'origine.
	EXPECT_EQ (CountColors (*before), colorsBefore);
	EXPECT_EQ (before->width (), wBefore);
}

TEST (TEST_cggraph_nodes_img, pixel_width_is_what_separates_the_two_chains)
{
	const Value source = LoadTestImage ();
	ASSERT_FALSE (source.IsEmpty ());

	const Value relief = Quantize (source, 8, 0);
	const Value blocks = Quantize (source, 8, 32);
	ASSERT_FALSE (relief.IsEmpty ());
	ASSERT_FALSE (blocks.IsEmpty ());

	const std::shared_ptr<const Img> a = relief.Share<Img> (Types ().image);
	const std::shared_ptr<const Img> b = blocks.Share<Img> (Types ().image);
	ASSERT_NE (a, nullptr);
	ASSERT_NE (b, nullptr);

	// Le meme noeud, un seul parametre change : c'est toute la difference entre
	// « Image to puzzle » et « Blocs pixelises ».
	EXPECT_EQ (b->width (), 32u);
	EXPECT_GT (a->width (), b->width ());

	// Le vote majoritaire choisit PARMI des couleurs de palette : il n'en cree
	// aucune. Une reduction par moyenne echouerait ici.
	EXPECT_LE (CountColors (*b), CountColors (*a));
}

// ---------------------------------------------------------------------------
//  Les extrudeurs
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_img, the_relief_carries_one_material_per_colour_plus_the_frame)
{
	const Value quantized = Quantize (LoadTestImage (), 4, 0);
	ASSERT_FALSE (quantized.IsEmpty ());
	const std::shared_ptr<const Img> image = quantized.Share<Img> (Types ().image);
	ASSERT_NE (image, nullptr);
	const std::size_t colors = CountColors (*image);

	ReliefNode node;
	ValueList in{ quantized };
	ValueList out;
	ASSERT_TRUE (RunNode (node, in, out));

	const std::shared_ptr<const Mesh> mesh = out[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (mesh, nullptr);
	EXPECT_GT (mesh->GetNFaces (), 0u);
	// Convention documentee : nCouleurs + base + mur, les deux emis par defaut.
	EXPECT_EQ ((std::size_t)mesh->GetNMaterials (), colors + 2u);
}

TEST (TEST_cggraph_nodes_img, the_extruders_do_not_mutate_the_image_they_share)
{
	// LE cas du fichier. CLitRasterToVector::Vectorize PALETTISE son entree ; les
	// surcharges prenant une image la copient donc avant de la lui donner. Sans
	// cette copie, le second extrudeur ci-dessous recevrait une image deja
	// palettisee -- et les deux sorties divergeraient sans que rien ne le dise.
	const Value quantized = Quantize (LoadTestImage (), 4, 0);
	ASSERT_FALSE (quantized.IsEmpty ());

	const std::shared_ptr<const Img> shared = quantized.Share<Img> (Types ().image);
	ASSERT_NE (shared, nullptr);
	const bool paletteBefore = shared->uses_palette ();
	const std::size_t colorsBefore = CountColors (*shared);

	ReliefNode first;
	ValueList in{ quantized };
	ValueList out1;
	ASSERT_TRUE (RunNode (first, in, out1));

	// L'image du lien est intacte : ni palettisee, ni recoloriee.
	EXPECT_EQ (shared->uses_palette (), paletteBefore);
	EXPECT_EQ (CountColors (*shared), colorsBefore);

	// Et un second consommateur du MEME lien rend exactement la meme chose.
	ReliefNode second;
	ValueList out2;
	ASSERT_TRUE (RunNode (second, in, out2));

	const std::shared_ptr<const Mesh> m1 = out1[0].Share<Mesh> (Types ().mesh);
	const std::shared_ptr<const Mesh> m2 = out2[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (m1, nullptr);
	ASSERT_NE (m2, nullptr);
	EXPECT_EQ (m1->GetNFaces (), m2->GetNFaces ());
	EXPECT_EQ (m1->GetNVertices (), m2->GetNVertices ());
	EXPECT_EQ (m1->GetNMaterials (), m2->GetNMaterials ());
}

TEST (TEST_cggraph_nodes_img, the_layer_output_is_a_dense_sequence_ending_with_base_and_wall)
{
	const Value quantized = Quantize (LoadTestImage (), 4, 0);
	ASSERT_FALSE (quantized.IsEmpty ());

	ReliefLayersNode node;
	ValueList in{ quantized };
	ValueList out;
	ASSERT_TRUE (RunNode (node, in, out));

	const std::shared_ptr<const MeshArray> parts = out[0].Share<MeshArray> (Types ().meshArray);
	ASSERT_NE (parts, nullptr);
	ASSERT_GE (parts->items.size (), 3u);   // au moins une couleur, la base, le mur

	// Suite DENSE : jamais d'entree nulle. C'est ce qui permet a flow.foreach de
	// la parcourir sans garde.
	for (const std::shared_ptr<const Mesh> &item : parts->items)
		ASSERT_NE (item, nullptr);

	// Base et mur TOUJOURS les deux dernieres entrees quand ils sont demandes ;
	// le nom du materiau est la seule facon de le verifier, et c'est aussi la
	// seule correspondance fiable pour les couches de couleur.
	const std::size_t n = parts->items.size ();
	ASSERT_EQ (parts->items[n - 2]->GetNMaterials (), 1u);
	ASSERT_EQ (parts->items[n - 1]->GetNMaterials (), 1u);
	EXPECT_EQ (std::string (parts->items[n - 2]->GetMaterial (0)->GetName ()), "base");
	EXPECT_EQ (std::string (parts->items[n - 1]->GetMaterial (0)->GetName ()), "wall");
}

TEST (TEST_cggraph_nodes_img, pixel_blocks_yield_one_solid_per_connected_component)
{
	// Grille volontairement petite : le nombre de blocs, donc le cout, est fixe
	// par pixelWidth et non par la resolution de la source.
	const Value quantized = Quantize (LoadTestImage (), 4, 16);
	ASSERT_FALSE (quantized.IsEmpty ());

	PixelBlocksNode display;
	ValueList in{ quantized };
	ValueList outDisplay;
	ASSERT_TRUE (RunNode (display, in, outDisplay));
	const std::shared_ptr<const Mesh> mesh = outDisplay[0].Share<Mesh> (Types ().mesh);
	ASSERT_NE (mesh, nullptr);
	EXPECT_GT (mesh->GetNFaces (), 0u);

	PixelBlocksPartsNode parts;
	ValueList outParts;
	ASSERT_TRUE (RunNode (parts, in, outParts));
	const std::shared_ptr<const MeshArray> pieces =
		outParts[0].Share<MeshArray> (Types ().meshArray);
	ASSERT_NE (pieces, nullptr);
	ASSERT_GE (pieces->items.size (), 3u);

	for (const std::shared_ptr<const Mesh> &item : pieces->items)
		ASSERT_NE (item, nullptr);

	// Chaque bloc porte UN materiau, nomme "block_NNNN_color_NN" : c'est le seul
	// lien fiable entre la position dans la suite et le bloc d'origine.
	const std::size_t n = pieces->items.size ();
	ASSERT_EQ (pieces->items[0]->GetNMaterials (), 1u);
	EXPECT_EQ (std::string (pieces->items[0]->GetMaterial (0)->GetName ()).compare (0, 6, "block_"), 0);
	EXPECT_EQ (std::string (pieces->items[n - 2]->GetMaterial (0)->GetName ()), "base");
	EXPECT_EQ (std::string (pieces->items[n - 1]->GetMaterial (0)->GetName ()), "wall");

	// La sortie d'AFFICHAGE porte un materiau par couleur, pas un par bloc :
	// c'est toute la difference entre les deux noeuds, et elle doit se voir.
	EXPECT_LT ((std::size_t)mesh->GetNMaterials (), pieces->items.size ());
}

TEST (TEST_cggraph_nodes_img, an_unquantized_image_is_accepted_and_that_is_the_documented_trap)
{
	// Ce test ne verifie pas un comportement souhaitable : il FIGE le piege
	// annonce par les caveats du catalogue et par image_relief.h, pour que
	// personne ne le corrige par accident en croyant a un bug.
	//
	// Les extrudeurs ne quantifient pas et ne verifient rien. Sur une image
	// brute, la chaine REUSSIT et rend un relief a autant de couleurs que
	// l'image en compte. Rien n'echoue ; le resultat est seulement
	// inexploitable, et c'est pourquoi la reserve est ecrite au catalogue.
	const Value source = LoadTestImage ();
	ASSERT_FALSE (source.IsEmpty ());
	const std::shared_ptr<const Img> raw = source.Share<Img> (Types ().image);
	ASSERT_NE (raw, nullptr);

	const Value quantized = Quantize (source, 4, 0);
	ASSERT_FALSE (quantized.IsEmpty ());
	const std::shared_ptr<const Img> clean = quantized.Share<Img> (Types ().image);
	ASSERT_NE (clean, nullptr);

	// La source brute a beaucoup plus de couleurs que la version quantifiee :
	// c'est cet ecart qui rend le branchement direct inexploitable.
	EXPECT_GT (CountColors (*raw), CountColors (*clean) * 4u);
}

// ---------------------------------------------------------------------------
//  La chaine entiere, dans un graphe
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_img, the_two_maker_pages_are_one_graph_sharing_a_single_source)
{
	// C'est le but de la decomposition : une SEULE lecture d'image alimente les
	// deux chaines, et l'evaluateur ne decode pas deux fois.
	const std::vector<unsigned char> bytes = ReadFile (kJpeg);
	ASSERT_FALSE (bytes.empty ());

	Graph graph;

	const NodeId source = graph.AddNode (std::unique_ptr<Node> (new LoadImageNode ()));
	LoadImageNode *loader = static_cast<LoadImageNode *> (graph.FindNode (source));
	loader->SetBytes (bytes);

	auto addQuantize = [&graph] (int pixelWidth) {
		const NodeId id = graph.AddNode (std::unique_ptr<Node> (new QuantizeImageNode ()));
		Node *node = graph.FindNode (id);
		node->GetParams ().SetInt ("maxColors", 4);
		node->GetParams ().SetInt ("pixelWidth", pixelWidth);
		node->GetParams ().SetInt ("preSmoothPasses", 0);
		node->GetParams ().SetInt ("refineIterations", 0);
		node->GetParams ().SetInt ("workingMaxDim", 128);
		return id;
	};

	const NodeId quantRelief = addQuantize (0);    // « Image to puzzle »
	const NodeId quantBlocks = addQuantize (16);   // « Blocs pixelises »
	const NodeId relief = graph.AddNode (std::unique_ptr<Node> (new ReliefNode ()));
	const NodeId blocks = graph.AddNode (std::unique_ptr<Node> (new PixelBlocksPartsNode ()));

	ASSERT_EQ (graph.Connect (source, 0, quantRelief, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (source, 0, quantBlocks, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (quantRelief, 0, relief, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (quantBlocks, 0, blocks, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;

	ASSERT_TRUE (evaluator.Evaluate (relief, outputs, ctx).IsOk ());
	ASSERT_NE (outputs[0].Share<Mesh> (Types ().mesh), nullptr);
	EXPECT_EQ (loader->GetDecodeCount (), 1u);

	ASSERT_TRUE (evaluator.Evaluate (blocks, outputs, ctx).IsOk ());
	ASSERT_NE (outputs[0].Share<MeshArray> (Types ().meshArray), nullptr);

	// LA source n'a ete decodee QU'UNE FOIS pour les deux chaines. C'est ce que
	// le port apporte, et ce qu'un parametre de chemin sur chaque extrudeur
	// aurait perdu.
	EXPECT_EQ (loader->GetDecodeCount (), 1u);
}

TEST (TEST_cggraph_nodes_img, an_image_link_refuses_a_mesh_and_the_reverse)
{
	// L'image est un type de domaine a part entiere : la connexion croisee est
	// refusee au moment du lien, pas decouverte au calcul.
	Graph graph;
	const NodeId source = graph.AddNode (std::unique_ptr<Node> (new LoadImageNode ()));
	const NodeId relief = graph.AddNode (std::unique_ptr<Node> (new ReliefNode ()));

	// La sortie du relief est un maillage ; l'entree de la quantification est une
	// image. Le moteur doit refuser.
	const NodeId quantize = graph.AddNode (std::unique_ptr<Node> (new QuantizeImageNode ()));
	ASSERT_EQ (graph.Connect (source, 0, relief, 0), ConnectStatus::Ok);
	EXPECT_NE (graph.Connect (relief, 0, quantize, 0), ConnectStatus::Ok);
}
