#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "../src/cggraph/core/evaluator.h"
#include "../src/cggraph/core/serialize.h"
#include "../src/cggraph/nodes/catalog.h"
#include "../src/cggraph/nodes/file_identity.h"
#include "../src/cggraph/nodes/img/load_image.h"
#include "../src/cggraph/nodes/io/file_ref.h"
#include "../src/cggraph/nodes/mesh/load_mesh.h"
#include "../src/cggraph/nodes/mesh/save_mesh.h"
#include "../src/cggraph/nodes/mesh/simplify.h"
#include "../src/cggraph/nodes/mesh/smooth_laplacian.h"
#include "../src/cggraph/nodes/node_support.h"
#include "../src/cggraph/nodes/shapes/extrude.h"
#include "../src/cggraph/nodes/text/text_contours.h"
#include "../src/cggraph/nodes/text/load_font.h"
#include "../src/cggraph/nodes/value_types.h"
#include "../src/cgmath/font.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_half_edge.h"
#include "../src/cgmesh/smoothing_laplacian.h"

// ===========================================================================
//  Couche B : les adaptateurs vers cgmesh et cgmath
// ===========================================================================
// Contrairement aux tests de la couche A, ceux-ci manipulent de VRAIS Mesh et
// de vraies Font : c'est ici que la frontiere d'immuabilite se prouve ou cede.
// Un adaptateur qui ecrirait dans son entree, ou qui fabriquerait un handle
// mutable a partir d'elle, serait pris par les cas de la derniere section.

using namespace cggraph;
using namespace cggraph_nodes;

namespace {

const char *kTrueTypeFont = "./test/data/fonts/DejaVuSans.ttf";

// Un tetraedre plein : quatre sommets, quatre faces, aucun bord. Ecrit avec un
// nombre de sommets variable pour que deux versions du meme fichier n'aient pas
// la meme TAILLE -- mtime + taille ne verrait pas une reecriture de meme taille
// dans la meme seconde, et c'est precisement ce que le hash sur demande couvre.
void WriteTetrahedron (const std::string &path, float scale, int extraVertices)
{
	FILE *file = std::fopen (path.c_str (), "w");
	ASSERT_NE (file, nullptr);
	std::fprintf (file, "v 0 0 0\n");
	std::fprintf (file, "v %f 0 0\n", scale);
	std::fprintf (file, "v 0 %f 0\n", scale);
	std::fprintf (file, "v 0 0 %f\n", scale);
	for (int i = 0; i < extraVertices; ++i)
		std::fprintf (file, "v %d 9 9\n", i);
	std::fprintf (file, "f 1 3 2\nf 1 2 4\nf 2 3 4\nf 1 4 3\n");
	std::fclose (file);
}

// Une grille reguliere : des sommets INTERIEURS, donc des sommets que le
// lissage deplace. Un tetraedre n'en a aucun.
std::shared_ptr<Mesh> MakeGrid (unsigned int side)
{
	std::vector<float> vertices;
	for (unsigned int j = 0; j < side; ++j)
		for (unsigned int i = 0; i < side; ++i)
		{
			vertices.push_back (static_cast<float> (i));
			vertices.push_back (static_cast<float> (j));
			// Une bosse au centre, sinon le lissage d'un plan ne change rien.
			const bool inner = i > 0 && j > 0 && i + 1 < side && j + 1 < side;
			vertices.push_back (inner ? 1.0f : 0.0f);
		}

	std::vector<unsigned int> faces;
	for (unsigned int j = 0; j + 1 < side; ++j)
		for (unsigned int i = 0; i + 1 < side; ++i)
		{
			const unsigned int a = j * side + i;
			faces.push_back (a);
			faces.push_back (a + 1);
			faces.push_back (a + side);
			faces.push_back (a + 1);
			faces.push_back (a + side + 1);
			faces.push_back (a + side);
		}

	std::shared_ptr<Mesh> mesh = std::make_shared<Mesh> ();
	mesh->SetVertices (side * side, vertices.data ());
	mesh->SetFaces (static_cast<unsigned int> (faces.size () / 3), 3, faces.data ());
	return mesh;
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

// Source de test : verse une valeur deja construite. Elle n'existe que pour
// alimenter un port depuis un test -- le catalogue n'a pas de noeud pour cela.
class ValueSourceNode : public Node
{
public:
	ValueSourceNode (const char *typeName, const TypeDesc *type, Value value)
		: m_value (std::move (value))
	{
		m_desc.typeName = typeName;
		m_desc.outputs.push_back ({ "sortie", type, false });
	}

	const NodeDesc &GetDesc () const override { return m_desc; }

	bool Compute (EvalContext &, const ValueList &, ValueList &out) override
	{
		out[0] = m_value;
		return true;
	}

private:
	NodeDesc m_desc;
	Value m_value;
};

// Puits de test : accepte une entree du type demande. Sert a verifier ce que la
// connexion REFUSE, sur deux types qu'aucun noeud du catalogue ne fait
// transiter.
class ValueSinkNode : public Node
{
public:
	ValueSinkNode (const char *typeName, const TypeDesc *type)
	{
		m_desc.typeName = typeName;
		m_desc.inputs.push_back ({ "entree", type, false });
	}

	const NodeDesc &GetDesc () const override { return m_desc; }
	bool Compute (EvalContext &, const ValueList &, ValueList &) override { return true; }

private:
	NodeDesc m_desc;
};

std::vector<unsigned char> ImageOfPositions (const Mesh &mesh)
{
	const std::vector<float> &vertices = mesh.GetVertices ();
	const unsigned char *bytes = reinterpret_cast<const unsigned char *> (vertices.data ());
	return std::vector<unsigned char> (bytes, bytes + vertices.size () * sizeof (float));
}

} // namespace

// ---------------------------------------------------------------------------
//  Catalogue et types de domaine
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_catalog, the_catalog_publishes_exactly_two_text_nodes)
{
	std::size_t text = 0;
	std::size_t mesh = 0;
	for (const CatalogEntry &entry : Catalog ())
	{
		if (std::string (entry.category) == "Texte")
			++text;
		if (std::string (entry.category) == "Maillage")
			++mesh;
	}

	// Deux, ni un ni trois : la police est un PORT -- donc un noeud a part --,
	// et l'extrusion est monolithique -- donc un seul noeud.
	EXPECT_EQ (text, 2u);
	// CINQ depuis l'etape 9 : la decomposition multi-objets rejoint les quatre
	// noeuds de maillage. Ce compte est un FILET, pas une description -- il a
	// rougi a l'ajout, ce qui est sa fonction.
	EXPECT_EQ (mesh, 5u);
}

TEST (TEST_cggraph_nodes_catalog, the_native_only_entries_are_named_one_by_one)
{
	// Le catalogue est UNIQUE, mais il n'est pas le meme sur les deux cibles :
	// certaines entrees emballent des corps de cgmesh absents de la liste
	// EMSCRIPTEN, et sont retirees sous __EMSCRIPTEN__ -- source ET
	// enregistrement.
	//
	// ⚠ CE TEST LISAIT LA CATEGORIE COMME UN SUBSTITUT DE LA PORTABILITE, et le
	// substitut a cede : `mesh.io.load_parts` est natif seulement -- vmeshes_io
	// n'est pas dans la liste EMSCRIPTEN -- et sa categorie est « Maillage »,
	// parce que la categorie decrit ce qu'un noeud FAIT, pas ou il se compile.
	// La liste est donc nommee, une entree a la fois : c'est la seule forme qui
	// ne puisse pas mentir.
	//
	// ⚠ Ce test tourne en NATIF : il ne peut pas observer la cible web. Ce qu'il
	// tient, c'est l'invariant qui la decrit. Ce qui se passe REELLEMENT au lien
	// WASM se mesure sur l'artefact, pas ici.
	const std::vector<std::string> nativeOnly = {
		"mesh.color.map", "mesh.hull.convex", "mesh.align.icp", "mesh.ambient_occlusion",
		"mesh.thickness", "mesh.curvature", "mesh.io.load_parts"
	};

	std::vector<std::string> portable;
	std::size_t analysis = 0;
	for (const CatalogEntry &entry : Catalog ())
	{
		if (std::find (nativeOnly.begin (), nativeOnly.end (), std::string (entry.typeName))
		    != nativeOnly.end ())
			++analysis;
		else
			portable.push_back (entry.typeName);
	}

	// Les six premiers, dans l'ordre : maillage et texte. Les GENERATEURS
	// suivent -- 26 formes par l'adaptateur generique, la fenetre gothique par
	// son descripteur ecrit a la main, cinq profils --, tous portables eux
	// aussi : leurs corps vivent dans parameterized_shapes.cpp et dans les
	// generateurs qu'il appelle, tous dans la liste EMSCRIPTEN de cgmesh.
	const std::vector<std::string> expectedFirst = {
		"mesh.io.load", "mesh.smooth.laplacian", "mesh.simplify", "mesh.io.save",
		"text.font.load", "text.contours",
		// CONTOUR 2D -- les deux moities de l'ancien text.extrude, plus le
		// producteur SVG qui partage desormais l'extrudeur.
		"svg.contours", "shape.extrude",
		// IMAGE -- portables comme les precedents : image_relief.cpp,
		// image_pixel_blocks.cpp, image_region_pipeline.cpp et
		// image_vectorization.cpp sont tous dans la liste EMSCRIPTEN de cgmesh,
		// et cgimg est globe en entier. Nommes ici et pas seulement comptes :
		// une cardinalite ne dit pas lequel a ete renomme.
		"file.ref",
		"img.io.load", "img.quantize", "img.relief", "img.relief.layers",
		"img.pixel_blocks", "img.pixel_blocks.parts"
	};
	ASSERT_GE (portable.size (), expectedFirst.size ());
	EXPECT_EQ (std::vector<std::string> (portable.begin (),
	                                     portable.begin () + expectedFirst.size ()),
	           expectedFirst);
	EXPECT_EQ (analysis, nativeOnly.size ());
	EXPECT_EQ (analysis, 7u);

	std::size_t shapes = 0;
	std::size_t profiles = 0;
	for (const CatalogEntry &entry : Catalog ())
	{
		if (std::string (entry.category) == "Formes") ++shapes;
		if (std::string (entry.category) == "Profil") ++profiles;
	}
	std::size_t flow = 0;
	std::size_t images = 0;
	for (const CatalogEntry &entry : Catalog ())
	{
		if (std::string (entry.category) == "Flux") ++flow;
		if (std::string (entry.category) == "Image") ++images;
	}

	EXPECT_EQ (shapes, 27u);      // 26 generiques + la fenetre gothique
	EXPECT_EQ (profiles, 5u);
	// Les cinq du flux sont PORTABLES : elles n'emballent aucun corps de
	// cgmesh, seulement le serialiseur et l'evaluateur de la couche A.
	EXPECT_EQ (flow, 5u);
	// Les six de l'image sont PORTABLES elles aussi : une source, le tronc de
	// quantification, et deux extrudeurs a deux sorties chacun.
	EXPECT_EQ (images, 6u);
	// Le noeud FICHIER, seul de sa categorie : il ne lit que <cstdio>, donc
	// portable comme les six precedents.
	std::size_t files = 0;
	for (const CatalogEntry &entry : Catalog ())
		if (std::string (entry.category) == "Fichier") ++files;
	EXPECT_EQ (files, 1u);
	// Le contour 2D en ajoute deux au decompte : l'ancien text.extrude, un
	// noeud, est devenu text.contours + shape.extrude, et svg.contours est
	// arrive avec eux. Tous portables -- text_extrude.cpp, import_svg.cpp et
	// extrude_contours.cpp sont dans la liste EMSCRIPTEN de cgmesh.
	EXPECT_EQ (portable.size (), 52u);
	EXPECT_EQ (Catalog ().size (), 59u);
}

TEST (TEST_cggraph_nodes_catalog, every_entry_either_carries_a_caveat_or_declares_it_has_none)
{
	// Le critere « chaque corps emballe a ete LU » n'a de dents que si l'absence
	// de reserve est une AFFIRMATION. Les entrees sans caveat sont donc nommees
	// ici une par une : une entree neuve qui n'en porterait pas et ne figurerait
	// pas dans cette liste fait rougir ce cas.
	const std::vector<std::string> noCaveat = {
		"text.font.load",       // Font::loadFromFile : rien a signaler
		"profile.chamfer",      // deux points, aucun corps de cgmesh derriere
		"profile.cavetto",      // idem, une boucle de sinus
		"flow.out"              // repete son entree ; aucun corps derriere
	};
	for (const CatalogEntry &entry : Catalog ())
	{
		const bool declared =
			std::find (noCaveat.begin (), noCaveat.end (), std::string (entry.typeName))
			!= noCaveat.end ();
		if (declared)
			EXPECT_EQ (entry.caveat, nullptr) << entry.typeName;
		else
			EXPECT_NE (entry.caveat, nullptr) << entry.typeName;
	}
}

TEST (TEST_cggraph_nodes_catalog, every_advertised_type_is_buildable_and_names_itself)
{
	for (const CatalogEntry &entry : Catalog ())
	{
		std::unique_ptr<Node> node = MakeNode (entry.typeName);
		ASSERT_NE (node, nullptr) << entry.typeName;
		// Le catalogue et le descripteur doivent nommer la meme chose : deux
		// identifiants pour un noeud, c'est un document qui ne se recharge pas.
		EXPECT_EQ (node->GetDesc ().typeName, std::string (entry.typeName));
	}
	EXPECT_EQ (MakeNode ("mesh.does.not.exist"), nullptr);
}

TEST (TEST_cggraph_nodes_catalog, the_catalog_names_what_the_wrapped_bodies_really_do)
{
	// Une entree du catalogue ne se lit pas seulement dans la declaration de ce
	// qu'elle emballe : les quatre reserves ci-dessous viennent de la LECTURE des
	// corps, et chacune designe un endroit ou le nom promet plus que le code.
	// Le cinquieme cas -- la police -- n'en a pas, et l'absence est affirmee.
	struct Expected { const char *typeName; bool hasCaveat; const char *mustMention; };
	const Expected expected[] = {
		{ "mesh.io.load", true, "import_obj" },
		{ "mesh.smooth.laplacian", true, "UNE passe" },
		{ "mesh.simplify", true, "PROXY" },
		{ "mesh.io.save", true, "casse" },
		{ "text.font.load", false, nullptr },
		{ "text.contours", true, "TOUJOURS fusionnes" },
		{ "svg.contours", true, "Clipper2" },
		{ "shape.extrude", true, "plaque de support" },
		// Les six noeuds d'analyse, et la meme discipline : chaque reserve vient
		// de la LECTURE du corps, pas de sa declaration.
		{ "mesh.color.map", true, "defined" },
		{ "mesh.hull.convex", true, "double_triangle" },
		{ "mesh.align.icp", true, "POINTEUR NU" },
		{ "mesh.ambient_occlusion", true, "CABLE" },
		{ "mesh.thickness", true, "[1, 256]" },
		{ "mesh.curvature", true, "#if 0" }
	};

	for (const Expected &want : expected)
	{
		const CatalogEntry *found = nullptr;
		for (const CatalogEntry &entry : Catalog ())
			if (std::string (entry.typeName) == want.typeName)
				found = &entry;
		ASSERT_NE (found, nullptr) << want.typeName;

		if (!want.hasCaveat)
		{
			EXPECT_EQ (found->caveat, nullptr) << want.typeName;
			continue;
		}
		ASSERT_NE (found->caveat, nullptr) << want.typeName;
		EXPECT_NE (std::string (found->caveat).find (want.mustMention), std::string::npos)
			<< want.typeName;
	}
}

TEST (TEST_cggraph_nodes_catalog, the_font_type_carries_no_clone_and_the_mesh_type_does)
{
	// Font n'est pas copiable PAR DECLARATION : un descripteur Forkable pour
	// elle ne compilerait pas. clone nul est donc un etat legitime, pas un trou.
	ASSERT_NE (Types ().font, nullptr);
	EXPECT_EQ (Types ().font->clone, nullptr);
	EXPECT_EQ (Types ().font->mutability, TypeDesc::Immutable);

	ASSERT_NE (Types ().mesh, nullptr);
	EXPECT_NE (Types ().mesh->clone, nullptr);
	EXPECT_EQ (Types ().mesh->mutability, TypeDesc::Forkable);
}

TEST (TEST_cggraph_nodes_catalog, curved_and_flattened_contours_are_two_distinct_types)
{
	ASSERT_NE (Types ().glyphContours, nullptr);
	ASSERT_NE (Types ().extrudeContours, nullptr);
	EXPECT_NE (Types ().glyphContours, Types ().extrudeContours);

	Graph graph;
	std::shared_ptr<std::vector<GlyphContour>> curved =
		std::make_shared<std::vector<GlyphContour>> (1);
	const NodeId source = graph.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.curved", Types ().glyphContours,
		                     Value::Make (Types ().glyphContours, curved))));
	const NodeId flattenedSink = graph.AddNode (std::unique_ptr<Node> (
		new ValueSinkNode ("test.flattened.sink", Types ().extrudeContours)));
	const NodeId curvedSink = graph.AddNode (std::unique_ptr<Node> (
		new ValueSinkNode ("test.curved.sink", Types ().glyphContours)));

	// Refus : un contour en unites de police n'est pas un contour aplati.
	EXPECT_EQ (graph.Connect (source, 0, flattenedSink, 0), ConnectStatus::TypeMismatch);
	// Et acceptation sur le type juste, sans quoi le refus ci-dessus ne
	// prouverait rien -- il pourrait tenir a n'importe quoi d'autre.
	EXPECT_EQ (graph.Connect (source, 0, curvedSink, 0), ConnectStatus::Ok);
}

// ---------------------------------------------------------------------------
//  Identite de source
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_mesh, an_unchanged_file_is_not_reread_and_a_changed_one_is)
{
	const std::string path = "cggraph_identity.obj";
	WriteTetrahedron (path, 1.0f, 0);

	Graph graph;
	const NodeId id = graph.AddNode (std::unique_ptr<Node> (new LoadMeshNode (path)));
	LoadMeshNode *loader = static_cast<LoadMeshNode *> (graph.FindNode (id));

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;

	ASSERT_TRUE (evaluator.Evaluate (id, outputs, ctx).IsOk ());
	EXPECT_EQ (loader->GetReadCount (), 1u);

	// Le fichier n'a pas bouge : le cache sert, le fichier n'est pas rouvert.
	ASSERT_TRUE (evaluator.Evaluate (id, outputs, ctx).IsOk ());
	EXPECT_EQ (loader->GetReadCount (), 1u);

	// Le fichier change de contenu ET de taille : la signature doit suivre.
	const Hash before = evaluator.GetSignature (id);
	WriteTetrahedron (path, 2.0f, 3);
	ASSERT_TRUE (evaluator.Evaluate (id, outputs, ctx).IsOk ());
	EXPECT_EQ (loader->GetReadCount (), 2u);
	EXPECT_NE (evaluator.GetSignature (id), before);

	std::remove (path.c_str ());
}

TEST (TEST_cggraph_nodes_mesh, the_content_hash_is_available_on_demand_only)
{
	const std::string path = "cggraph_identity_hash.obj";
	WriteTetrahedron (path, 1.0f, 0);

	LoadMeshNode loader (path);
	loader.RefreshExternalState ();
	const ParamValue *identity = loader.GetParams ().Find ("source.identity");
	ASSERT_NE (identity, nullptr);
	// Par defaut, la porte est un stat : le fichier n'est pas ouvert pour
	// decider s'il faut le relire, sans quoi le cache n'economiserait rien.
	EXPECT_EQ (identity->stringValue.compare (0, 5, "stat:"), 0);

	const std::string statKey = identity->stringValue;
	loader.GetParams ().SetBool ("verifyHash", true);
	loader.RefreshExternalState ();
	EXPECT_EQ (identity->stringValue.compare (0, 5, "hash:"), 0);
	EXPECT_NE (identity->stringValue, statKey);

	// Le hash voit ce que mtime + taille ne voit pas : meme taille, contenu
	// different.
	const std::string hashKey = identity->stringValue;
	WriteTetrahedron (path, 3.0f, 0);
	loader.RefreshExternalState ();
	EXPECT_NE (identity->stringValue, hashKey);

	std::remove (path.c_str ());
}

// ---------------------------------------------------------------------------
//  Les quatre noeuds de maillage
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_mesh, a_four_node_graph_writes_a_file_that_reads_back)
{
	const std::string source = "cggraph_chain_in.obj";
	const std::string target = "cggraph_chain_out.obj";
	WriteTetrahedron (source, 1.0f, 0);

	Graph graph;
	const NodeId load = graph.AddNode (std::unique_ptr<Node> (new LoadMeshNode (source)));
	const NodeId smooth = graph.AddNode (std::unique_ptr<Node> (new SmoothLaplacianNode (2, 0.5f)));
	const NodeId simplify = graph.AddNode (std::unique_ptr<Node> (new SimplifyNode (1.0f)));
	const NodeId save = graph.AddNode (std::unique_ptr<Node> (new SaveMeshNode (target)));

	ASSERT_EQ (graph.Connect (load, 0, smooth, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (smooth, 0, simplify, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (simplify, 0, save, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	const EvalResult result = evaluator.Evaluate (save, outputs, ctx);
	ASSERT_EQ (result.status, EvalStatus::Ok) << result.detail;
	EXPECT_TRUE (outputs.empty ());

	// Le maillage relu est compare a celui que la branche a produit, non a un
	// simple compte : un fichier qui se relit n'est pas un fichier qui porte le
	// bon maillage. La tolerance est celle de l'OBJ, qui ecrit en %f.
	ValueList produced;
	ASSERT_TRUE (evaluator.Evaluate (simplify, produced, ctx).IsOk ());
	const Mesh *expected = produced[0].Get<Mesh> (Types ().mesh);
	ASSERT_NE (expected, nullptr);

	Mesh reloaded;
	ASSERT_EQ (reloaded.load (target.c_str ()), 0);
	ASSERT_EQ (reloaded.GetNVertices (), expected->GetNVertices ());
	ASSERT_EQ (reloaded.GetNFaces (), expected->GetNFaces ());
	ASSERT_EQ (reloaded.GetVertices ().size (), expected->GetVertices ().size ());
	for (std::size_t i = 0; i < expected->GetVertices ().size (); ++i)
		EXPECT_NEAR (reloaded.GetVertices ()[i], expected->GetVertices ()[i], 1e-5f)
			<< "composante " << i;

	// Le puits porte un EFFET DE BORD : il n'entre pas au cache, donc re-evaluer
	// reecrit le fichier. Sa branche amont, elle, est servie par le cache -- le
	// nombre de lectures de la source ne bouge pas.
	SaveMeshNode *writer = static_cast<SaveMeshNode *> (graph.FindNode (save));
	LoadMeshNode *reader = static_cast<LoadMeshNode *> (graph.FindNode (load));
	EXPECT_TRUE (graph.FindNode (save)->GetDesc ().sideEffect);
	EXPECT_EQ (writer->GetWriteCount (), 1u);
	EXPECT_EQ (reader->GetReadCount (), 1u);
	EXPECT_FALSE (evaluator.IsCached (save));
	ASSERT_TRUE (evaluator.Evaluate (save, outputs, ctx).IsOk ());
	EXPECT_EQ (writer->GetWriteCount (), 2u);
	EXPECT_EQ (reader->GetReadCount (), 1u);

	std::remove (source.c_str ());
	std::remove (target.c_str ());
}

TEST (TEST_cggraph_nodes_mesh, smoothing_moves_the_mesh_and_leaves_its_input_untouched)
{
	std::shared_ptr<Mesh> grid = MakeGrid (5);
	const std::vector<unsigned char> imageBefore = ImageOfPositions (*grid);
	const uint64_t revisionBefore = grid->GetRevision ();

	Graph graph;
	const NodeId source = graph.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.mesh", Types ().mesh, Value::Make (Types ().mesh, grid))));
	const NodeId smooth = graph.AddNode (std::unique_ptr<Node> (new SmoothLaplacianNode (1, 1.0f)));
	ASSERT_EQ (graph.Connect (source, 0, smooth, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (smooth, outputs, ctx).IsOk ());
	ASSERT_EQ (outputs.size (), 1u);

	const Mesh *smoothed = outputs[0].Get<Mesh> (Types ().mesh);
	ASSERT_NE (smoothed, nullptr);
	EXPECT_NE (ImageOfPositions (*smoothed), imageBefore);

	// L'entree, elle, n'a pas bouge d'un octet -- ni sa geometrie, ni sa
	// revision.
	EXPECT_EQ (ImageOfPositions (*grid), imageBefore);
	EXPECT_EQ (grid->GetRevision (), revisionBefore);
}

TEST (TEST_cggraph_nodes_mesh, lambda_and_iterations_both_change_the_result)
{
	std::shared_ptr<Mesh> grid = MakeGrid (5);
	const std::vector<unsigned char> original = ImageOfPositions (*grid);

	Graph graph;
	const NodeId source = graph.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.mesh", Types ().mesh, Value::Make (Types ().mesh, grid))));
	const NodeId smooth = graph.AddNode (std::unique_ptr<Node> (new SmoothLaplacianNode (1, 1.0f)));
	ASSERT_EQ (graph.Connect (source, 0, smooth, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;

	ASSERT_TRUE (evaluator.Evaluate (smooth, outputs, ctx).IsOk ());
	const std::vector<unsigned char> onePass = ImageOfPositions (*outputs[0].Get<Mesh> (Types ().mesh));

	// Apply ne fait qu'UNE passe : si le noeud ne bouclait pas, cinq iterations
	// rendraient le meme maillage qu'une seule.
	graph.FindNode (smooth)->GetParams ().SetInt ("iterations", 5);
	ASSERT_TRUE (evaluator.Evaluate (smooth, outputs, ctx).IsOk ());
	EXPECT_NE (ImageOfPositions (*outputs[0].Get<Mesh> (Types ().mesh)), onePass);

	// lambda = 0 : le melange ramene chaque sommet ou il etait. Le parametre a
	// donc bien un effet, et le declarer n'etait pas une promesse en l'air.
	graph.FindNode (smooth)->GetParams ().SetFloat ("lambda", 0.0f);
	ASSERT_TRUE (evaluator.Evaluate (smooth, outputs, ctx).IsOk ());
	EXPECT_EQ (ImageOfPositions (*outputs[0].Get<Mesh> (Types ().mesh)), original);
}

TEST (TEST_cggraph_nodes_mesh, an_unfed_zone_smooths_everything_and_a_fed_zone_smooths_only_it)
{
	std::shared_ptr<Mesh> grid = MakeGrid (5);

	Graph graph;
	const NodeId source = graph.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.mesh", Types ().mesh, Value::Make (Types ().mesh, grid))));
	const NodeId smooth = graph.AddNode (std::unique_ptr<Node> (new SmoothLaplacianNode (1, 1.0f)));
	ASSERT_EQ (graph.Connect (source, 0, smooth, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;

	// Zone non alimentee : le port etant OPTIONNEL, le calcul a lieu au lieu
	// d'etre refuse par nom.
	const EvalResult unfed = evaluator.Evaluate (smooth, outputs, ctx);
	ASSERT_EQ (unfed.status, EvalStatus::Ok) << unfed.detail;
	const std::vector<unsigned char> whole = ImageOfPositions (*outputs[0].Get<Mesh> (Types ().mesh));
	EXPECT_NE (whole, ImageOfPositions (*grid));

	// Zone alimentee, reduite a UN sommet interieur : tout le reste doit rester
	// a sa place.
	std::shared_ptr<Selection> zone = std::make_shared<Selection> ();
	zone->vertices.push_back (12u);   // centre d'une grille 5 x 5
	const NodeId zoneSource = graph.AddNode (std::unique_ptr<Node> (new ValueSourceNode (
		"test.zone", Types ().selection, Value::Make (Types ().selection, zone))));
	ASSERT_EQ (graph.Connect (zoneSource, 0, smooth, 1), ConnectStatus::Ok);

	ASSERT_TRUE (evaluator.Evaluate (smooth, outputs, ctx).IsOk ());
	const Mesh *zoned = outputs[0].Get<Mesh> (Types ().mesh);
	ASSERT_NE (zoned, nullptr);
	EXPECT_NE (ImageOfPositions (*zoned), whole);

	const std::vector<float> &after = zoned->GetVertices ();
	const std::vector<float> &before = grid->GetVertices ();
	ASSERT_EQ (after.size (), before.size ());
	for (std::size_t i = 0; i < after.size (); ++i)
	{
		if (i / 3 == 12u)
			continue;
		EXPECT_FLOAT_EQ (after[i], before[i]) << "sommet " << (i / 3) << " hors zone a bouge";
	}
}

TEST (TEST_cggraph_nodes_mesh, simplify_reduces_the_face_count)
{
	std::shared_ptr<Mesh> grid = MakeGrid (9);
	const unsigned int facesBefore = grid->GetNFaces ();

	Graph graph;
	const NodeId source = graph.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.mesh", Types ().mesh, Value::Make (Types ().mesh, grid))));
	const NodeId simplify = graph.AddNode (std::unique_ptr<Node> (new SimplifyNode (0.5f)));
	ASSERT_EQ (graph.Connect (source, 0, simplify, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (simplify, outputs, ctx).IsOk ());

	const Mesh *decimated = outputs[0].Get<Mesh> (Types ().mesh);
	ASSERT_NE (decimated, nullptr);
	EXPECT_LT (decimated->GetNFaces (), facesBefore);
	EXPECT_GT (decimated->GetNFaces (), 0u);
	EXPECT_EQ (grid->GetNFaces (), facesBefore);
}

// ---------------------------------------------------------------------------
//  Le jeton d'annulation, du moteur jusqu'a l'algorithme
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_mesh, the_token_reaches_the_algorithm_itself)
{
	std::shared_ptr<Mesh> grid = MakeGrid (5);
	Mesh_half_edge model (grid.get ());
	const std::vector<unsigned char> before = ImageOfPositions (*model.m_pMesh);

	std::atomic<bool> cancelled (false);
	GraphContext context;
	context.SetCancellationFlag (&cancelled);

	MeshAlgoSmoothingLaplacian algo;
	ASSERT_TRUE (algo.Apply (&model, &context));
	EXPECT_NE (ImageOfPositions (*model.m_pMesh), before);

	// Jeton pose : l'algorithme rend false et n'a rien ecrit.
	const std::vector<unsigned char> midway = ImageOfPositions (*model.m_pMesh);
	cancelled.store (true);
	EXPECT_FALSE (algo.Apply (&model, &context));
	EXPECT_EQ (ImageOfPositions (*model.m_pMesh), midway);
}

TEST (TEST_cggraph_nodes_mesh, an_abort_raised_during_the_computation_stops_the_iterations)
{
	std::shared_ptr<Mesh> grid = MakeGrid (5);

	Graph graph;
	const NodeId source = graph.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.mesh", Types ().mesh, Value::Make (Types ().mesh, grid))));
	const NodeId smooth = graph.AddNode (std::unique_ptr<Node> (new SmoothLaplacianNode (5, 1.0f)));
	ASSERT_EQ (graph.Connect (source, 0, smooth, 0), ConnectStatus::Ok);

	std::atomic<bool> cancelled (false);
	EvalContext ctx;
	ctx.SetCancellationFlag (&cancelled);

	// Le declencheur est la PREMIERE progression rendue par le noeud : le
	// drapeau se leve donc pendant le calcul, entre deux passes, et non avant.
	std::vector<float> steps;
	ctx.SetProgressSink ([&steps, &cancelled] (float t, const char *) {
		steps.push_back (t);
		cancelled.store (true);
	});

	Evaluator evaluator (graph);
	ValueList outputs;
	const EvalResult result = evaluator.Evaluate (smooth, outputs, ctx);

	EXPECT_EQ (result.status, EvalStatus::Aborted);
	EXPECT_EQ (result.node, smooth);
	EXPECT_FALSE (evaluator.IsCached (smooth));
	// UNE progression sur cinq iterations demandees : c'est l'algorithme qui a
	// refuse la seconde passe. S'il ignorait le jeton, il y en aurait cinq.
	ASSERT_EQ (steps.size (), 1u);
	EXPECT_FLOAT_EQ (steps[0], 0.2f);
}

// ---------------------------------------------------------------------------
//  Les deux noeuds de texte
// ---------------------------------------------------------------------------

// Monte la chaine « police -> contours -> extrusion » et rend les trois
// identifiants. C'est le remplacant de l'ancien text.extrude monolithique : deux
// noeuds la ou il n'y en avait qu'un, et l'extrudeur est desormais partage avec
// le SVG.
struct TextChain
{
	Graph graph;
	NodeId font = 0;
	NodeId contours = 0;
	NodeId extrude = 0;
};

static bool BuildTextChain (TextChain &chain, const std::vector<unsigned char> &fontBytes,
                            const std::string &text)
{
	chain.font = chain.graph.AddNode (std::unique_ptr<Node> (new LoadFontNode ()));
	chain.contours = chain.graph.AddNode (std::unique_ptr<Node> (new TextContoursNode (text)));
	chain.extrude = chain.graph.AddNode (std::unique_ptr<Node> (new ExtrudeNode ()));
	static_cast<LoadFontNode *> (chain.graph.FindNode (chain.font))->SetBytes (fontBytes);
	return chain.graph.Connect (chain.font, 0, chain.contours, 0) == ConnectStatus::Ok
	       && chain.graph.Connect (chain.contours, 0, chain.extrude, 0) == ConnectStatus::Ok;
}

// Extrude un texte multiligne sous un alignement donne et rend l'empreinte de
// ses positions. Multiligne DELIBEREMENT : sur une seule ligne l'alignement ne
// deplace rien, et le test ne prouverait rien.
static std::vector<unsigned char> ExtrudeAligned (const std::vector<unsigned char> &fontBytes,
                                                  int align)
{
	TextChain chain;
	if (!BuildTextChain (chain, fontBytes, "I\nMMMM"))
		return std::vector<unsigned char> ();
	chain.graph.FindNode (chain.contours)->GetParams ().SetInt ("align", align);

	Evaluator evaluator (chain.graph);
	EvalContext ctx;
	ValueList outputs;
	if (!evaluator.Evaluate (chain.extrude, outputs, ctx).IsOk ())
		return std::vector<unsigned char> ();
	const Mesh *mesh = outputs[0].Get<Mesh> (Types ().mesh);
	return mesh != nullptr ? ImageOfPositions (*mesh) : std::vector<unsigned char> ();
}

TEST (TEST_cggraph_nodes_text, the_alignment_reaches_the_layout_and_is_bounded)
{
	// PARITE avec la page « Texte 3D » de maker, dont la forme
	// ParameterizedText3D expose les trois alignements. Le noeud ne les exposait
	// pas : TextExtrudeOptions::align restait a son defaut, et tout texte
	// multiligne sortait ferre a gauche.
	const std::vector<unsigned char> bytes = ReadAllBytes (kTrueTypeFont);
	ASSERT_FALSE (bytes.empty ());

	const std::vector<unsigned char> left   = ExtrudeAligned (bytes, 0);
	const std::vector<unsigned char> center = ExtrudeAligned (bytes, 1);
	const std::vector<unsigned char> right  = ExtrudeAligned (bytes, 2);
	ASSERT_FALSE (left.empty ());
	ASSERT_FALSE (center.empty ());
	ASSERT_FALSE (right.empty ());

	// Les trois placent la ligne courte differemment : le parametre ATTEINT
	// vraiment la mise en page, ce qu'une simple lecture de ParamSet ne dirait pas.
	EXPECT_NE (left, center);
	EXPECT_NE (center, right);
	EXPECT_NE (left, right);

	// Deterministe : meme reglage, meme geometrie.
	EXPECT_EQ (ExtrudeAligned (bytes, 1), center);

	// Borne, comme `support` : hors de [0, 2] la valeur est ramenee dans
	// l'intervalle plutot que transtypee en une valeur d'enumeration inexistante.
	EXPECT_EQ (ExtrudeAligned (bytes, -5), left);
	EXPECT_EQ (ExtrudeAligned (bytes, 99), right);
}

TEST (TEST_cggraph_nodes_text, a_document_written_before_align_existed_still_reads_as_left)
{
	// L'ajout d'`align` n'a PAS incremente la version du descripteur, et c'est
	// delibere : IsVersionCompatible est une egalite stricte sans crochet de
	// migration, donc un bump aurait condamne tous les documents existants.
	// Ce cas fige la condition qui rend ce choix legitime -- l'absence du
	// parametre doit se calculer exactement comme l'ancien comportement.
	const std::vector<unsigned char> bytes = ReadAllBytes (kTrueTypeFont);
	ASSERT_FALSE (bytes.empty ());

	TextChain chain;
	ASSERT_TRUE (BuildTextChain (chain, bytes, "I\nMMMM"));
	// Un document d'avant l'ajout ne porte pas d'entree "align" : la relecture
	// vide le jeu et n'en repose aucune. On reproduit exactement cet etat.
	Node *contours = chain.graph.FindNode (chain.contours);
	contours->GetParams ().Clear ();
	contours->GetParams ().SetString ("text", "I\nMMMM");

	Evaluator evaluator (chain.graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (chain.extrude, outputs, ctx).IsOk ());
	const Mesh *mesh = outputs[0].Get<Mesh> (Types ().mesh);
	ASSERT_NE (mesh, nullptr);
	EXPECT_EQ (ImageOfPositions (*mesh), ExtrudeAligned (bytes, 0));
}

TEST (TEST_cggraph_nodes_text, the_byte_fed_sources_are_reachable_through_the_ByteSource_contract)
{
	// C'EST LE CONTRAT DONT DEPEND maker/graph_api.cpp : graphSetBytes retrouve
	// la source par un dynamic_cast vers ByteSource, sans enumerer les types de
	// noeuds. Si une source cessait d'implementer l'interface, rien d'autre que
	// ce cas ne le dirait -- la facade se contenterait de rendre false, et
	// l'import de fichier deviendrait silencieusement inoperant dans l'editeur web.
	LoadFontNode fontNode;
	LoadImageNode imageNode;
	SimplifyNode notASource (0.5f);

	EXPECT_NE (dynamic_cast<ByteSource *> (&fontNode), nullptr);
	EXPECT_NE (dynamic_cast<ByteSource *> (&imageNode), nullptr);
	EXPECT_EQ (dynamic_cast<ByteSource *> (&notASource), nullptr);

	// Et l'interface suffit reellement a alimenter la source : c'est par elle,
	// et non par le type concret, que la facade ecrit.
	ByteSource *source = &fontNode;
	source->SetName ("DejaVu");
	source->SetBytes (ReadAllBytes (kTrueTypeFont));

	EvalContext ctx;
	ValueList out (1);
	ASSERT_TRUE (fontNode.Compute (ctx, ValueList (), out));
	EXPECT_NE (out[0].Get<Font> (Types ().font), nullptr);
}

TEST (TEST_cggraph_nodes_text, the_font_travels_on_a_port_and_the_text_is_extruded)
{
	const std::vector<unsigned char> bytes = ReadAllBytes (kTrueTypeFont);
	ASSERT_FALSE (bytes.empty ());

	TextChain chain;
	ASSERT_TRUE (BuildTextChain (chain, bytes, "AB"));

	// La police est un PORT d'entree, non un parametre : c'est la seule forme
	// qui la relie a la signature de ce qui la consomme. Et le contour 2D en est
	// un aussi, entre les deux moities de l'ancien noeud monolithique : c'est ce
	// port qui rend l'extrudeur reutilisable par svg.contours.
	const Node *contours = chain.graph.FindNode (chain.contours);
	ASSERT_EQ (contours->GetDesc ().inputs.size (), 1u);
	EXPECT_EQ (contours->GetDesc ().inputs[0].name, std::string ("police"));
	EXPECT_EQ (contours->GetDesc ().inputs[0].type, Types ().font);
	const Node *extruder = chain.graph.FindNode (chain.extrude);
	ASSERT_EQ (extruder->GetDesc ().inputs.size (), 1u);
	EXPECT_EQ (extruder->GetDesc ().inputs[0].type, Types ().extrudeContours);

	Evaluator evaluator (chain.graph);
	EvalContext ctx;
	ValueList outputs;
	const EvalResult result = evaluator.Evaluate (chain.extrude, outputs, ctx);
	ASSERT_EQ (result.status, EvalStatus::Ok) << result.detail;

	const Mesh *mesh = outputs[0].Get<Mesh> (Types ().mesh);
	ASSERT_NE (mesh, nullptr);
	EXPECT_GT (mesh->GetNVertices (), 0u);
	EXPECT_GT (mesh->GetNFaces (), 0u);

	// La police n'est analysee qu'une fois : un reglage aval ne la re-parse pas.
	// Le decoupage RENFORCE le cas -- `depth` vit maintenant deux noeuds plus
	// loin, et le cache doit donc reutiliser aussi les contours, pas seulement
	// la police.
	LoadFontNode *loader = static_cast<LoadFontNode *> (chain.graph.FindNode (chain.font));
	EXPECT_EQ (loader->GetParseCount (), 1u);
	chain.graph.FindNode (chain.extrude)->GetParams ().SetFloat ("depth", 0.5f);
	ASSERT_TRUE (evaluator.Evaluate (chain.extrude, outputs, ctx).IsOk ());
	EXPECT_EQ (loader->GetParseCount (), 1u);
}

TEST (TEST_cggraph_nodes_text, one_byte_of_the_buffer_moves_the_signature_and_the_name_does_not)
{
	std::vector<unsigned char> bytes = ReadAllBytes (kTrueTypeFont);
	ASSERT_FALSE (bytes.empty ());

	Graph graph;
	const NodeId id = graph.AddNode (std::unique_ptr<Node> (new LoadFontNode ()));
	LoadFontNode *loader = static_cast<LoadFontNode *> (graph.FindNode (id));
	loader->SetBytes (bytes);
	loader->SetName ("DejaVu");

	const Hash reference = Signature (graph, id);

	// Le nom est decoratif : il ne participe a aucun calcul.
	loader->SetName ("un tout autre nom");
	EXPECT_EQ (Signature (graph, id), reference);

	// Un octet du buffer, lui, change ce qui sera produit.
	bytes[bytes.size () / 2] = static_cast<unsigned char> (bytes[bytes.size () / 2] ^ 0xFF);
	loader->SetBytes (bytes);
	EXPECT_NE (Signature (graph, id), reference);

	// Et l'identite versee est bien un hash, jamais un mtime : aucun fichier
	// n'est interroge pour une ressource qui vit en memoire.
	const ParamValue *identity = loader->GetParams ().Find ("source.identity");
	ASSERT_NE (identity, nullptr);
	EXPECT_EQ (identity->stringValue.compare (0, 5, "hash:"), 0);
}

TEST (TEST_cggraph_nodes_text, mississippi_flattens_four_glyphs_out_of_eleven)
{
	const std::vector<unsigned char> bytes = ReadAllBytes (kTrueTypeFont);
	ASSERT_FALSE (bytes.empty ());

	TextChain chain;
	ASSERT_TRUE (BuildTextChain (chain, bytes, "MISSISSIPPI"));

	Evaluator evaluator (chain.graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (chain.contours, outputs, ctx).IsOk ());

	// Onze glyphes places, quatre aplatissements : M, I, S, P. C'est cette
	// memoisation par glyphe qu'un decoupage du noeud detruirait, et que le
	// cache du graphe ne pourrait pas recuperer -- sa granularite est le noeud.
	// ELLE SURVIT au decoupage contours/extrusion, parce que la coupure a ete
	// faite en AVAL d'elle : elle vit dans text_to_contours.
	const TextContoursNode *node =
		static_cast<const TextContoursNode *> (chain.graph.FindNode (chain.contours));
	EXPECT_EQ (node->GetGlyphsPlaced (), 11u);
	EXPECT_EQ (node->GetGlyphsFlattened (), 4u);
}

// ---------------------------------------------------------------------------
//  Visibilite : la tenue de livre des noeuds sources
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_mesh, the_file_identity_is_semantic_and_internal_at_once)
{
	// Les deux proprietes ensemble, et c'est tout l'objet d'un second axe :
	// Semantic parce que c'est ce parametre qui invalide le cache quand le
	// fichier change, Internal parce que le regler a la main ne veut rien dire.
	// Un axe unique obligerait a sacrifier l'une des deux.
	const std::string path = "cggraph_identity_visibility.obj";
	WriteTetrahedron (path, 1.0f, 0);

	LoadMeshNode loader (path);
	const ParamEntry *identity = loader.GetParams ().FindEntry ("source.identity");
	ASSERT_NE (identity, nullptr);
	EXPECT_EQ (identity->role, ParamRole::Semantic);
	EXPECT_EQ (identity->visibility, ParamVisibility::Internal);

	// Le noeud REECRIT ce parametre a chaque rafraichissement. Une reecriture
	// qui laisserait retomber la visibilite sur son defaut rendrait le champ
	// editable a la premiere lecture du disque -- c'est-a-dire toujours.
	loader.RefreshExternalState ();
	identity = loader.GetParams ().FindEntry ("source.identity");
	ASSERT_NE (identity, nullptr);
	EXPECT_EQ (identity->visibility, ParamVisibility::Internal);
	EXPECT_EQ (identity->role, ParamRole::Semantic);

	// Temoin : les parametres que l'utilisateur regle, eux, restent publics.
	ASSERT_NE (loader.GetParams ().FindEntry ("path"), nullptr);
	EXPECT_EQ (loader.GetParams ().FindEntry ("path")->visibility, ParamVisibility::Public);
	ASSERT_NE (loader.GetParams ().FindEntry ("verifyHash"), nullptr);
	EXPECT_EQ (loader.GetParams ().FindEntry ("verifyHash")->visibility, ParamVisibility::Public);

	std::remove (path.c_str ());
}

TEST (TEST_cggraph_nodes_text, the_font_identity_is_semantic_and_internal_at_once)
{
	const std::vector<unsigned char> bytes = ReadAllBytes (kTrueTypeFont);
	ASSERT_FALSE (bytes.empty ());

	LoadFontNode loader;
	const ParamEntry *identity = loader.GetParams ().FindEntry ("source.identity");
	ASSERT_NE (identity, nullptr);
	EXPECT_EQ (identity->role, ParamRole::Semantic);
	EXPECT_EQ (identity->visibility, ParamVisibility::Internal);

	loader.SetBytes (bytes);
	identity = loader.GetParams ().FindEntry ("source.identity");
	ASSERT_NE (identity, nullptr);
	EXPECT_EQ (identity->visibility, ParamVisibility::Internal);
	EXPECT_EQ (identity->role, ParamRole::Semantic);

	// Le nom est l'inverse exact sur les deux axes : non semantique et public.
	// Sans lui, « tout est interne » satisferait le cas.
	loader.SetName ("DejaVu");
	const ParamEntry *name = loader.GetParams ().FindEntry ("name");
	ASSERT_NE (name, nullptr);
	EXPECT_EQ (name->role, ParamRole::NonSemantic);
	EXPECT_EQ (name->visibility, ParamVisibility::Public);
}

// ---------------------------------------------------------------------------
//  Le sizeHint du type mesh
// ---------------------------------------------------------------------------
// Le budget memoire de l'evaluateur EST ce chiffre : une entree annoncee deux
// fois trop legere fait evincer deux fois trop tard, et le pic reel double.
// Le filet compare donc a la somme des tableaux REELLEMENT alloues, terme a
// terme -- pas a une constante, qui ne dirait rien de ce qui manque.
TEST (TEST_cggraph_nodes_catalog, the_mesh_size_hint_counts_every_per_face_array)
{
	ASSERT_NE (Types ().mesh, nullptr);
	ASSERT_NE (Types ().mesh->sizeHint, nullptr);

	Mesh mesh;
	const float v[12] = { 0.f, 0.f, 0.f,  1.f, 0.f, 0.f,  0.f, 1.f, 0.f,  0.f, 0.f, 1.f };
	mesh.SetVertices (4, v);
	unsigned int f[12] = { 0,1,2,  0,2,3,  0,3,1,  1,3,2 };
	mesh.SetFaces (4, 3, f);
	// Les trois tableaux que l'ancienne version omettait. ComputeNormals ()
	// remplit d'un coup les normales par sommet ET par face.
	mesh.ComputeNormals ();
	mesh.SetVertexColors (std::vector<float> { 1.f,0.f,0.f, 0.f,1.f,0.f, 0.f,0.f,1.f, 1.f,1.f,0.f });

	ASSERT_EQ (mesh.GetFaceNormals ().size (), 12u) << "temoin : sans elles le cas ne prouve rien";
	ASSERT_EQ (mesh.GetVertexColors ().size (), 12u);

	const std::size_t attendu =
		sizeof (Mesh)
		+ mesh.GetVertices ().size () * sizeof (float)
		+ mesh.GetVertexNormals ().size () * sizeof (float)
		+ mesh.GetFaceNormals ().size () * sizeof (float)
		+ mesh.GetVertexColors ().size () * sizeof (float)
		+ mesh.GetTextureCoordinates ().size () * sizeof (float)
		+ static_cast<std::size_t> (mesh.GetNFaces ()) * 3u * sizeof (unsigned int)
		+ static_cast<std::size_t> (mesh.GetNFaces ()) * sizeof (unsigned int);

	EXPECT_EQ (Types ().mesh->sizeHint (&mesh), attendu);

	// Et le chiffre doit MONTER quand un de ces tableaux apparait : un sizeHint
	// qui les ignore rendrait la meme valeur avant et apres.
	Mesh nu;
	nu.SetVertices (4, v);
	nu.SetFaces (4, 3, f);
	EXPECT_LT (Types ().mesh->sizeHint (&nu), Types ().mesh->sizeHint (&mesh));
}


// ---------------------------------------------------------------------------
//  Le noeud FICHIER
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_file, the_path_is_serialised_so_a_saved_document_can_name_its_resource)
{
	// LA raison d'etre du noeud. Sans lui, un document enregistre porte
	// l'empreinte d'un contenu qu'il n'a plus et ne sait meme pas le nommer.
	Graph graph;
	const NodeId id = graph.AddNode (std::unique_ptr<Node> (new FileRefNode ()));
	static_cast<FileRefNode *> (graph.FindNode (id))->SetPath (kTrueTypeFont);

	const std::string document = SaveGraph (graph);
	EXPECT_NE (document.find (kTrueTypeFont), std::string::npos)
		<< "le chemin doit etre serialise : sans lui le document ne se rouvre pas";

	// Et le document reste un DIAGNOSTIC, pas une archive : rien du contenu n'y
	// entre, quelle que soit la taille du fichier designe.
	EXPECT_LT (document.size (), 2048u) << "le document a grossi : du contenu y a fui";
}

TEST (TEST_cggraph_nodes_file, it_designates_without_reading_and_refuses_a_path_that_leads_nowhere)
{
	// « Il ne lit rien » n'est pas qu'une formule : ce que le noeud publie est le
	// CHEMIN lui-meme. Mais il verifie l'existence, sinon l'echec surviendrait un
	// cran plus loin, dans un chargeur qui dirait « police illisible » la ou le
	// vrai probleme est un chemin faux.
	Graph graph;
	const NodeId id = graph.AddNode (std::unique_ptr<Node> (new FileRefNode ()));
	FileRefNode *file = static_cast<FileRefNode *> (graph.FindNode (id));

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;

	file->SetPath ("chemin/qui/n/existe/pas.ttf");
	EXPECT_FALSE (evaluator.Evaluate (id, outputs, ctx).IsOk ());

	file->SetPath (kTrueTypeFont);
	ASSERT_TRUE (evaluator.Evaluate (id, outputs, ctx).IsOk ());
	const std::string *published = outputs[0].Get<std::string> (Types ().path);
	ASSERT_NE (published, nullptr);
	EXPECT_EQ (*published, std::string (kTrueTypeFont));
}

TEST (TEST_cggraph_nodes_file, a_font_reads_from_the_port_when_it_is_connected)
{
	// L'entree est OPTIONNELLE : le meme noeud sert les deux voies, et c'est ce
	// qui permet de l'avoir ajoutee sans incrementer la version du descripteur.
	Graph graph;
	const NodeId source = graph.AddNode (std::unique_ptr<Node> (new FileRefNode ()));
	const NodeId font = graph.AddNode (std::unique_ptr<Node> (new LoadFontNode ()));
	static_cast<FileRefNode *> (graph.FindNode (source))->SetPath (kTrueTypeFont);

	const NodeDesc &desc = graph.FindNode (font)->GetDesc ();
	ASSERT_EQ (desc.inputs.size (), 1u);
	EXPECT_TRUE (desc.inputs[0].optional) << "l'entree doit rester optionnelle";
	EXPECT_EQ (desc.inputs[0].type, Types ().path);
	EXPECT_EQ (desc.version, 1) << "ajouter une entree OPTIONNELLE ne doit pas bumper la version";

	ASSERT_EQ (graph.Connect (source, 0, font, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (font, outputs, ctx).IsOk ());
	EXPECT_NE (outputs[0].Get<Font> (Types ().font), nullptr);
}

TEST (TEST_cggraph_nodes_file, a_font_still_works_with_nothing_connected)
{
	// Le cas des documents deja ecrits : entree non connectee, octets internes.
	// S'il cassait, les trois gabarits livres cesseraient de se calculer.
	LoadFontNode node;
	node.SetBytes (ReadAllBytes (kTrueTypeFont));

	EvalContext ctx;
	ValueList in (1);          // l'entree existe mais reste VIDE
	ValueList out (1);
	ASSERT_TRUE (node.Compute (ctx, in, out));
	EXPECT_NE (out[0].Get<Font> (Types ().font), nullptr);
}

TEST (TEST_cggraph_nodes_file, the_identity_follows_the_file_and_not_the_path_string)
{
	// Ce qui relie la signature de l'aval au CONTENU : le lien ne porte qu'un
	// nom, donc sans ce parametre semantique un fichier modifie resservirait un
	// resultat de cache perime.
	FileRefNode node;
	node.SetPath ("chemin/qui/n/existe/pas.ttf");
	const std::string absent = node.GetParams ().Find ("source.identity")->stringValue;

	node.SetPath (kTrueTypeFont);
	const std::string present = node.GetParams ().Find ("source.identity")->stringValue;

	EXPECT_NE (absent, present) << "un fichier existant et un chemin mort ont la meme identite";
	EXPECT_FALSE (present.empty ());

	// Le nom decoratif suit le chemin, mais il est HORS signature.
	const ParamEntry *name = node.GetParams ().FindEntry ("name");
	ASSERT_NE (name, nullptr);
	EXPECT_EQ (name->role, ParamRole::NonSemantic);
	EXPECT_NE (name->value.stringValue.find ("DejaVuSans"), std::string::npos);
}
