#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../src/cggraph/core/evaluator.h"
#include "../src/cggraph/nodes/catalog.h"
#include "../src/cggraph/nodes/mesh/load_mesh.h"
#include "../src/cggraph/nodes/mesh/simplify.h"
#include "../src/cggraph/nodes/mesh/smooth_laplacian.h"
#include "../src/cggraph/nodes/node_support.h"
#include "../src/cggraph/nodes/text/load_font.h"
#include "../src/cggraph/nodes/value_types.h"
#include "../src/cgmath/font.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_half_edge.h"
#include "../src/cgmesh/smoothing_laplacian.h"
#include "../src/cgmesh/text_extrude.h"

// ===========================================================================
//  Couche B sous deux fils -- P10, et le jeton dans les boucles externes
// ===========================================================================
// Ce fichier repond a la seule question que l'audit de reentrance ne pouvait pas
// fermer par la lecture : deux evaluations concurrentes des algorithmes CABLES
// donnent-elles ce que deux evaluations sequentielles donnent ?
//
// PERIMETRE DU JETON, dit une fois : trois des six noeuds emballent un
// algorithme qui a une boucle externe -- SmoothLaplacian, Simplify,
// TextContours (l'ex-ExtrudeText, dont c'est la moitie amont qui porte la
// boucle de placement des glyphes). Les trois autres -- LoadMesh, SaveMesh, LoadFont -- emballent un
// appel monolithique d'entree-sortie ou d'analyse : il n'y a pas de boucle
// externe a instrumenter, et y en poser une reviendrait a faire traverser le
// contexte a mesh_io_obj, mesh_io_ply, mesh_io_stl et stb_truetype. C'est un
// choix, il est ecrit, et le critere ne porte donc que sur les trois premiers.
//
// MESURE, POUR QUE LE CHOIX SOIT JUGEABLE, et elle porte sa commande : sur les
// donnees de la suite, les cas qui contiennent une lecture de fichier ou une
// analyse de police entiere s'executent en 2 a 5 ms
// (`TU.exe --gtest_filter=TEST_cggraph_nodes_text.*:TEST_cggraph_nodes_mesh.*`,
// colonne de duree). Un calcul ininterruptible de cet ordre ne gele pas une
// interface. Ce n'est PAS une mesure sur un gros fichier : le jour ou un noeud
// chargera un maillage de plusieurs millions de triangles, ce raisonnement sera
// a refaire, et le cout sera alors celui de faire traverser le contexte aux
// lecteurs d'entree-sortie.

using namespace cggraph;
using namespace cggraph_nodes;

namespace {

const char *kTrueTypeFont = "./test/data/fonts/DejaVuSans.ttf";

std::shared_ptr<Mesh> MakeGrid (unsigned int side, float bump)
{
	std::vector<float> vertices;
	for (unsigned int j = 0; j < side; ++j)
		for (unsigned int i = 0; i < side; ++i)
		{
			vertices.push_back (static_cast<float> (i));
			vertices.push_back (static_cast<float> (j));
			const bool inner = i > 0 && j > 0 && i + 1 < side && j + 1 < side;
			vertices.push_back (inner ? bump : 0.0f);
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

std::vector<unsigned char> ImageOf (const Mesh &mesh)
{
	const std::vector<float> &vertices = mesh.GetVertices ();
	const std::vector<unsigned int> triangles = mesh.GetTriangles ();
	std::vector<unsigned char> image;
	const unsigned char *v = reinterpret_cast<const unsigned char *> (vertices.data ());
	image.insert (image.end (), v, v + vertices.size () * sizeof (float));
	const unsigned char *t = reinterpret_cast<const unsigned char *> (triangles.data ());
	image.insert (image.end (), t, t + triangles.size () * sizeof (unsigned int));
	return image;
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

void WriteTetrahedron (const std::string &path, float scale)
{
	FILE *file = std::fopen (path.c_str (), "w");
	if (file == nullptr)
		return;
	std::fprintf (file, "v 0 0 0\nv %f 0 0\nv 0 %f 0\nv 0 0 %f\n", scale, scale, scale);
	std::fprintf (file, "f 1 3 2\nf 1 2 4\nf 2 3 4\nf 1 4 3\n");
	std::fclose (file);
}

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

// Barriere REUTILISABLE a deux fils. Elle est le moyen de l'entrelacement force :
// sans elle, deux fils lances a la suite se croisent rarement, et le test
// mesurerait l'ordonnanceur plutot que le code.
class RoundBarrier
{
public:
	void Wait ()
	{
		std::unique_lock<std::mutex> held (m_mutex);
		const unsigned int generation = m_generation;
		if (++m_waiting == 2)
		{
			m_waiting = 0;
			++m_generation;
			m_cv.notify_all ();
			return;
		}
		m_cv.wait (held, [this, generation] { return generation != m_generation; });
	}

private:
	std::mutex m_mutex;
	std::condition_variable m_cv;
	unsigned int m_waiting = 0;
	unsigned int m_generation = 0;
};

// Le graphe de reference de ce fichier : source -> lissage -> simplification.
// Deux algorithmes cables sur une meme branche, donc deux occasions de partager
// un etat statique.
struct Branch
{
	NodeId source = kInvalidNodeId;
	NodeId smooth = kInvalidNodeId;
	NodeId simplify = kInvalidNodeId;
};

Branch AddBranch (Graph &graph, const std::shared_ptr<Mesh> &mesh, int iterations)
{
	Branch branch;
	branch.source = graph.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.mesh", Types ().mesh, Value::Make (Types ().mesh, mesh))));
	branch.smooth =
		graph.AddNode (std::unique_ptr<Node> (new SmoothLaplacianNode (iterations, 0.7f)));
	branch.simplify = graph.AddNode (std::unique_ptr<Node> (new SimplifyNode (0.6f)));
	EXPECT_EQ (graph.Connect (branch.source, 0, branch.smooth, 0), ConnectStatus::Ok);
	EXPECT_EQ (graph.Connect (branch.smooth, 0, branch.simplify, 0), ConnectStatus::Ok);
	return branch;
}

std::vector<unsigned char> EvaluateImage (Graph &graph, NodeId id)
{
	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	const EvalResult result = evaluator.Evaluate (id, outputs, ctx);
	EXPECT_EQ (result.status, EvalStatus::Ok);
	if (outputs.empty ())
		return std::vector<unsigned char> ();
	const Mesh *mesh = outputs[0].Get<Mesh> (Types ().mesh);
	return mesh != nullptr ? ImageOf (*mesh) : std::vector<unsigned char> ();
}

} // namespace

// ---------------------------------------------------------------------------
//  Critere 6.1 -- le jeton dans la boucle externe de chaque algorithme cable
// ---------------------------------------------------------------------------
// Instrument commun aux trois : le drapeau est pose AVANT l'appel, et
// l'observable est le TRAVAIL REELLEMENT FAIT, non le code de retour seul. Un
// algorithme qui ignorerait le jeton rendrait son resultat nominal, et c'est
// cela que chaque cas compare.

TEST (TEST_cggraph_nodes_concurrency, the_smoothing_loop_returns_before_writing_anything)
{
	std::shared_ptr<Mesh> grid = MakeGrid (24, 1.0f);
	Mesh_half_edge model (grid.get ());
	const std::vector<unsigned char> before = ImageOf (*model.m_pMesh);

	std::atomic<bool> cancelled (false);
	GraphContext ctx;
	ctx.SetCancellationFlag (&cancelled);

	MeshAlgoSmoothingLaplacian algo;
	ASSERT_TRUE (algo.Apply (&model, &ctx));
	const std::vector<unsigned char> nominal = ImageOf (*model.m_pMesh);
	ASSERT_NE (nominal, before);

	cancelled.store (true);
	EXPECT_FALSE (algo.Apply (&model, &ctx));
	EXPECT_EQ (ImageOf (*model.m_pMesh), nominal);
}

TEST (TEST_cggraph_nodes_concurrency, the_decimation_loop_returns_before_collapsing_anything)
{
	Mesh_half_edge::SimplifyOptions options;

	std::shared_ptr<Mesh> grid = MakeGrid (24, 1.0f);
	const unsigned int facesBefore = grid->GetNFaces ();

	{
		Mesh_half_edge nominal (grid.get ());
		nominal.simplify (0.5f, options, nullptr);
		ASSERT_LT (nominal.m_pMesh->GetNFaces (), facesBefore);
	}

	std::atomic<bool> cancelled (true);
	GraphContext ctx;
	ctx.SetCancellationFlag (&cancelled);

	// Le maillage est deja triangule : la triangulation prealable ne peut donc
	// pas etre confondue avec une decimation qui aurait eu lieu.
	Mesh_half_edge aborted (grid.get ());
	aborted.simplify (0.5f, options, &ctx);
	EXPECT_EQ (aborted.m_pMesh->GetNFaces (), facesBefore);
}

TEST (TEST_cggraph_nodes_concurrency, the_glyph_loop_returns_before_flattening_anything)
{
	const std::vector<unsigned char> bytes = ReadAllBytes (kTrueTypeFont);
	ASSERT_FALSE (bytes.empty ());

	Font font;
	ASSERT_TRUE (font.loadFromMemory (bytes, 0));

	TextExtrudeOptions options;
	options.size = 1.0f;
	options.depth = 0.2f;

	TextExtrudeStats nominal;
	Mesh *produced = text_to_extruded_mesh (font, "MISSISSIPPI", options, &nominal, nullptr);
	ASSERT_NE (produced, nullptr);
	delete produced;
	ASSERT_EQ (nominal.glyphsFlattened, 4u);

	std::atomic<bool> cancelled (true);
	GraphContext ctx;
	ctx.SetCancellationFlag (&cancelled);

	TextExtrudeStats stats;
	Mesh *nothing = text_to_extruded_mesh (font, "MISSISSIPPI", options, &stats, &ctx);
	EXPECT_EQ (nothing, nullptr);

	// La mise en page a eu lieu -- elle precede la boucle --, aucun glyphe n'a
	// ete aplati. C'est ce qui distingue « la boucle teste le jeton » de « la
	// fonction refuse en entree ».
	EXPECT_EQ (stats.glyphsPlaced, 11u);
	EXPECT_EQ (stats.glyphsFlattened, 0u);
}

// ---------------------------------------------------------------------------
//  Critere 6.6 -- l'annulation se nomme, quel que soit le code de retour
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_concurrency, an_adapter_that_returns_false_on_abort_is_named_Aborted)
{
	std::shared_ptr<Mesh> grid = MakeGrid (12, 1.0f);

	Graph graph;
	const NodeId source = graph.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.mesh", Types ().mesh, Value::Make (Types ().mesh, grid))));
	const NodeId smooth = graph.AddNode (std::unique_ptr<Node> (new SmoothLaplacianNode (5, 1.0f)));
	ASSERT_EQ (graph.Connect (source, 0, smooth, 0), ConnectStatus::Ok);

	std::atomic<bool> cancelled (false);
	EvalContext ctx;
	ctx.SetCancellationFlag (&cancelled);
	ctx.SetProgressSink ([&cancelled] (float, const char *) { cancelled.store (true); });

	Evaluator evaluator (graph);
	ValueList outputs;
	const EvalResult result = evaluator.Evaluate (smooth, outputs, ctx);

	// L'adaptateur rend `false` -- il n'interroge plus le contexte pour choisir
	// son code de retour --, et c'est l'evaluateur qui nomme. Avec l'ordre des
	// tests d'avant, cette ligne dirait ComputeFailed.
	EXPECT_EQ (result.status, EvalStatus::Aborted);
	EXPECT_EQ (result.node, smooth);
	EXPECT_FALSE (evaluator.IsCached (smooth));
}

TEST (TEST_cggraph_nodes_concurrency, a_genuine_failure_without_abort_is_still_named_ComputeFailed)
{
	// Versant symetrique, et il est ce qui empeche le precedent d'etre satisfait
	// par un evaluateur qui repondrait Aborted a tout : un chemin vide fait
	// echouer le chargement, sans qu'aucune annulation soit en jeu.
	Graph graph;
	const NodeId load = graph.AddNode (std::unique_ptr<Node> (new LoadMeshNode (std::string ())));

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	const EvalResult result = evaluator.Evaluate (load, outputs, ctx);
	EXPECT_EQ (result.status, EvalStatus::ComputeFailed);
	EXPECT_EQ (result.node, load);
}

// ---------------------------------------------------------------------------
//  Critere 6.2 -- P10 : concurrent == sequentiel
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_concurrency, two_concurrent_evaluations_produce_what_two_sequential_ones_produce)
{
	std::shared_ptr<Mesh> left = MakeGrid (20, 1.0f);
	std::shared_ptr<Mesh> right = MakeGrid (16, 3.0f);

	// UN SEUL graphe, deux entrees distinctes -- la forme exacte du critere.
	Graph graph;
	const Branch a = AddBranch (graph, left, 3);
	const Branch b = AddBranch (graph, right, 2);

	const std::vector<unsigned char> referenceA = EvaluateImage (graph, a.simplify);
	const std::vector<unsigned char> referenceB = EvaluateImage (graph, b.simplify);
	ASSERT_FALSE (referenceA.empty ());
	ASSERT_FALSE (referenceB.empty ());
	ASSERT_NE (referenceA, referenceB);

	// Repetitions AVEC barriere de depart : c'est ce qui remplace ThreadSanitizer.
	// Un seul tour laisserait les deux fils se suivre au lieu de se croiser ; la
	// barriere les fait partir ensemble, et la repetition multiplie les
	// entrelacements visites.
	const int rounds = 16;
	RoundBarrier start;
	std::atomic<int> mismatches (0);

	std::thread worker ([&] {
		Evaluator evaluator (graph);
		for (int round = 0; round < rounds; ++round)
		{
			start.Wait ();
			EvalContext ctx;
			ValueList outputs;
			if (!evaluator.Evaluate (b.simplify, outputs, ctx).IsOk () || outputs.empty ())
			{
				++mismatches;
				continue;
			}
			const Mesh *mesh = outputs[0].Get<Mesh> (Types ().mesh);
			if (mesh == nullptr || ImageOf (*mesh) != referenceB)
				++mismatches;
		}
	});

	Evaluator evaluator (graph);
	for (int round = 0; round < rounds; ++round)
	{
		start.Wait ();
		EvalContext ctx;
		ValueList outputs;
		ASSERT_TRUE (evaluator.Evaluate (a.simplify, outputs, ctx).IsOk ());
		ASSERT_FALSE (outputs.empty ());
		const Mesh *mesh = outputs[0].Get<Mesh> (Types ().mesh);
		ASSERT_NE (mesh, nullptr);
		EXPECT_EQ (ImageOf (*mesh), referenceA);
	}
	worker.join ();
	EXPECT_EQ (mismatches.load (), 0);
}

TEST (TEST_cggraph_nodes_concurrency, two_branches_sharing_one_file_source_stay_independent)
{
	// La forme qui met en cause la RELEVEE d'etat exterieur : un seul noeud
	// source, deux branches, deux fils. Les deux evaluations ecrivent alors la
	// meme identite dans le meme jeu de parametres, et la lisent pour hacher.
	const std::string path = "concurrency_shared_source.obj";
	WriteTetrahedron (path, 2.0f);

	Graph graph;
	const NodeId load = graph.AddNode (std::unique_ptr<Node> (new LoadMeshNode (path)));
	const NodeId smooth = graph.AddNode (std::unique_ptr<Node> (new SmoothLaplacianNode (2, 0.5f)));
	const NodeId simplify = graph.AddNode (std::unique_ptr<Node> (new SimplifyNode (0.8f)));
	ASSERT_EQ (graph.Connect (load, 0, smooth, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (load, 0, simplify, 0), ConnectStatus::Ok);

	const std::vector<unsigned char> referenceSmooth = EvaluateImage (graph, smooth);
	const std::vector<unsigned char> referenceSimplify = EvaluateImage (graph, simplify);
	ASSERT_FALSE (referenceSmooth.empty ());
	ASSERT_FALSE (referenceSimplify.empty ());

	const int rounds = 16;
	RoundBarrier start;
	std::atomic<int> mismatches (0);

	std::thread worker ([&] {
		Evaluator evaluator (graph);
		for (int round = 0; round < rounds; ++round)
		{
			start.Wait ();
			EvalContext ctx;
			ValueList outputs;
			if (!evaluator.Evaluate (simplify, outputs, ctx).IsOk () || outputs.empty ())
			{
				++mismatches;
				continue;
			}
			const Mesh *mesh = outputs[0].Get<Mesh> (Types ().mesh);
			if (mesh == nullptr || ImageOf (*mesh) != referenceSimplify)
				++mismatches;
		}
	});

	Evaluator evaluator (graph);
	for (int round = 0; round < rounds; ++round)
	{
		start.Wait ();
		EvalContext ctx;
		ValueList outputs;
		ASSERT_TRUE (evaluator.Evaluate (smooth, outputs, ctx).IsOk ());
		ASSERT_FALSE (outputs.empty ());
		const Mesh *mesh = outputs[0].Get<Mesh> (Types ().mesh);
		ASSERT_NE (mesh, nullptr);
		EXPECT_EQ (ImageOf (*mesh), referenceSmooth);
	}
	worker.join ();
	EXPECT_EQ (mismatches.load (), 0);

	// Une seule lecture du fichier par evaluation qui la merite : le compteur est
	// atomique, donc le total est exact meme sous deux fils.
	const LoadMeshNode *source = static_cast<const LoadMeshNode *> (graph.FindNode (load));
	EXPECT_GT (source->GetReadCount (), 0u);
	std::remove (path.c_str ());
}

// ---------------------------------------------------------------------------
//  L'invariant qui rend l'exclusion suffisante
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_nodes_concurrency, refreshing_external_state_only_ever_writes_internal_parameters)
{
	// La pre-passe est la SEULE ecriture d'etat de noeud pendant une evaluation,
	// et elle est exclusive. Cela ne suffit que si elle n'ecrit rien qu'un calcul
	// concurrent puisse lire -- donc rien qui soit Public. Ce cas parcourt le
	// CATALOGUE : un noeud ajoute plus tard y passe sans qu'on y pense.
	for (const CatalogEntry &entry : Catalog ())
	{
		std::unique_ptr<Node> node = MakeNode (entry.typeName);
		ASSERT_NE (node, nullptr) << entry.typeName;

		std::vector<std::string> before;
		for (const ParamEntry &param : node->GetParams ().GetEntries ())
			before.push_back (param.name + "=" + param.value.stringValue + "|"
			                  + std::to_string (param.value.intValue) + "|"
			                  + std::to_string (param.value.floatValue) + "|"
			                  + std::to_string (param.value.boolValue ? 1 : 0));

		node->RefreshExternalState ();

		std::size_t index = 0;
		for (const ParamEntry &param : node->GetParams ().GetEntries ())
		{
			ASSERT_LT (index, before.size ()) << entry.typeName;
			const std::string after = param.name + "=" + param.value.stringValue + "|"
			                          + std::to_string (param.value.intValue) + "|"
			                          + std::to_string (param.value.floatValue) + "|"
			                          + std::to_string (param.value.boolValue ? 1 : 0);
			if (after != before[index])
				EXPECT_EQ (param.visibility, ParamVisibility::Internal)
					<< entry.typeName << " / " << param.name;
			++index;
		}
		EXPECT_EQ (index, before.size ()) << entry.typeName;
	}
}

TEST (TEST_cggraph_nodes_concurrency, an_internal_parameter_is_not_readable_from_a_computation)
{
	LoadMeshNode node ("quelque_chose.obj");
	node.RefreshExternalState ();

	// L'identite est bien la, et elle a ete relevee.
	const ParamEntry *identity = node.GetParams ().FindEntry ("source.identity");
	ASSERT_NE (identity, nullptr);
	EXPECT_EQ (identity->visibility, ParamVisibility::Internal);

	// Mais un adaptateur ne peut pas la lire depuis Compute : c'est la seule
	// valeur qu'un autre fil puisse ecrire pendant qu'il calcule. Le refus est
	// dans le lecteur commun, pas dans la vigilance de chaque adaptateur.
	EXPECT_EQ (GetString (node.GetParams (), "source.identity", "refuse"), std::string ("refuse"));
	EXPECT_EQ (GetString (node.GetParams (), "path", "refuse"),
	           std::string ("quelque_chose.obj"));
}
