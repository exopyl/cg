#include <gtest/gtest.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "../src/cggraph/core/evaluator.h"
#include "../src/cggraph/core/serialize.h"
#include "../src/cggraph/nodes/catalog.h"
#include "../src/cggraph/nodes/catalog_factory.h"
#include "../src/cggraph/nodes/mesh/load_mesh.h"
#include "../src/cggraph/nodes/mesh/save_mesh.h"
#include "../src/cggraph/nodes/mesh/smooth_laplacian.h"
#include "../src/cggraph/nodes/runner.h"
#include "../src/cgmesh/mesh.h"

// ===========================================================================
//  Le document, rejoue en tete haute
// ===========================================================================
// Le pilotage d'un graphe sans interface, tel qu'il vit dans
// cggraph/nodes/runner.h. C'est le SEUL point d'entree du rejeu : aucun
// executable ne l'enveloppe.
//
// Trois proprietes se prouvent ici et nulle part ailleurs, parce qu'elles
// n'existent qu'une fois le document relu : un puits ne s'execute que sur
// demande NOMMEE, un nom de fichier produit ne bouge pas d'une execution a
// l'autre, et une source relue reinterroge son fichier avant que sa signature
// ne serve d'index.

using namespace cggraph;
using namespace cggraph_nodes;

namespace {

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

bool Exists (const std::string &path)
{
	FILE *file = std::fopen (path.c_str (), "rb");
	if (file == nullptr)
		return false;
	std::fclose (file);
	return true;
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

// Chaine minimale : une source de fichier, un puits dont le nom se DEDUIT du
// contenu recu. Le jeton {hash} est ce qui rend le nommage verifiable.
struct SourceAndSink
{
	Graph graph;
	NodeId load = kInvalidNodeId;
	NodeId save = kInvalidNodeId;

	SourceAndSink (const std::string &input, const std::string &outputPattern)
	{
		load = graph.AddNode (std::unique_ptr<Node> (new LoadMeshNode (input)));
		save = graph.AddNode (std::unique_ptr<Node> (new SaveMeshNode (outputPattern)));
		graph.Connect (load, 0, save, 0);
	}
};

} // namespace

// ---------------------------------------------------------------------------
//  Effet de bord et demande explicite
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_runner, a_document_without_a_named_target_writes_nothing)
{
	// « Execute seulement sur demande explicite » n'est plus une consequence du
	// sens du tirage des qu'un PILOTE choisit ce qu'il tire : charger un
	// document et evaluer d'office ce qui n'a pas d'aval ecrirait des fichiers
	// que personne n'a demandes. La demande est donc exigee.
	const std::string input = "./cli_pull_input.obj";
	const std::string output = "./cli_pull_{hash}.obj";
	const std::string document = "./cli_pull.json";
	WriteTetrahedron (input, 1.0f, 0);

	SourceAndSink chain (input, output);
	ASSERT_EQ (SaveGraphToFile (chain.graph, document), SerializeStatus::Ok);

	RunRequest request;
	request.document = document;
	const RunReport report = cggraph_nodes::Run (request);

	EXPECT_EQ (report.status, RunStatus::NoTarget);
	EXPECT_EQ (report.nodeCount, 2u);
	EXPECT_EQ (report.linkCount, 1u);
	ASSERT_EQ (report.sinks.size (), 1u);
	EXPECT_EQ (report.sinks[0], chain.save);
	EXPECT_TRUE (report.evaluated.empty ());
	EXPECT_TRUE (report.written.empty ());

	std::remove (document.c_str ());
	std::remove (input.c_str ());
}

TEST (TEST_cggraph_runner, naming_the_sinks_is_a_demand_and_it_writes)
{
	// Versant symetrique du precedent : sans lui, « rien ne s'ecrit » serait
	// satisfait par un pilote qui n'ecrit jamais.
	const std::string input = "./cli_sinks_input.obj";
	const std::string document = "./cli_sinks.json";
	WriteTetrahedron (input, 1.0f, 0);

	SourceAndSink chain (input, "./cli_sinks_{hash}.obj");
	ASSERT_EQ (SaveGraphToFile (chain.graph, document), SerializeStatus::Ok);

	RunRequest request;
	request.document = document;
	request.allSinks = true;
	const RunReport report = cggraph_nodes::Run (request);

	ASSERT_TRUE (report.IsOk ()) << report.detail;
	ASSERT_EQ (report.evaluated.size (), 1u);
	EXPECT_EQ (report.evaluated[0], chain.save);
	ASSERT_EQ (report.written.size (), 1u);
	EXPECT_TRUE (Exists (report.written[0]));

	// Designer le noeud par son identifiant est l'autre forme de la demande, et
	// elle nomme le meme fichier.
	RunRequest byId;
	byId.document = document;
	byId.targets.push_back (chain.save);
	const RunReport second = cggraph_nodes::Run (byId);
	ASSERT_TRUE (second.IsOk ()) << second.detail;
	ASSERT_EQ (second.written.size (), 1u);
	EXPECT_EQ (second.written[0], report.written[0]);

	std::remove (report.written[0].c_str ());
	std::remove (document.c_str ());
	std::remove (input.c_str ());
}

TEST (TEST_cggraph_runner, an_unknown_target_is_refused_by_number)
{
	const std::string input = "./cli_unknown_input.obj";
	const std::string document = "./cli_unknown.json";
	WriteTetrahedron (input, 1.0f, 0);

	SourceAndSink chain (input, "./cli_unknown_{hash}.obj");
	ASSERT_EQ (SaveGraphToFile (chain.graph, document), SerializeStatus::Ok);

	RunRequest request;
	request.document = document;
	request.targets.push_back (99);
	const RunReport report = cggraph_nodes::Run (request);
	EXPECT_EQ (report.status, RunStatus::UnknownTarget);
	EXPECT_EQ (report.detail, "99");
	EXPECT_TRUE (report.written.empty ());

	RunRequest missing;
	missing.document = "./no_such_document.json";
	EXPECT_EQ (cggraph_nodes::Run (missing).status, RunStatus::LoadFailed);

	std::remove (document.c_str ());
	std::remove (input.c_str ());
}

// ---------------------------------------------------------------------------
//  Nommage deterministe
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_runner, two_runs_of_the_same_document_name_the_same_file)
{
	const std::string input = "./cli_stable_input.obj";
	const std::string document = "./cli_stable.json";
	WriteTetrahedron (input, 1.0f, 0);

	SourceAndSink chain (input, "./cli_stable_{hash}.obj");
	ASSERT_EQ (SaveGraphToFile (chain.graph, document), SerializeStatus::Ok);

	RunRequest request;
	request.document = document;
	request.allSinks = true;

	const RunReport first = cggraph_nodes::Run (request);
	ASSERT_TRUE (first.IsOk ()) << first.detail;
	const RunReport second = cggraph_nodes::Run (request);
	ASSERT_TRUE (second.IsOk ()) << second.detail;
	EXPECT_EQ (first.written, second.written);

	// Et le compte des ECRITURES ne doit pas entrer dans le nom. Le puits n'est
	// jamais mis en cache : le tirer deux fois dans le MEME graphe le fait
	// ecrire deux fois, ce qui est la seule situation ou un compteur
	// d'executions se verrait.
	Graph graph;
	const CatalogFactory factory;
	ASSERT_TRUE (LoadGraphFromFile (document, factory, graph).IsOk ());
	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (chain.save, outputs, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (chain.save, outputs, ctx).IsOk ());

	SaveMeshNode *sink = static_cast<SaveMeshNode *> (graph.FindNode (chain.save));
	ASSERT_NE (sink, nullptr);
	EXPECT_EQ (sink->GetWriteCount (), 2u);
	ASSERT_EQ (sink->GetWrittenPaths ().size (), 2u);
	EXPECT_EQ (sink->GetWrittenPaths ()[0], sink->GetWrittenPaths ()[1]);
	EXPECT_EQ (sink->GetWrittenPaths ()[0], first.written[0]);

	std::remove (first.written[0].c_str ());
	std::remove (document.c_str ());
	std::remove (input.c_str ());
}

TEST (TEST_cggraph_runner, a_replayed_document_produces_the_same_file_as_the_direct_run)
{
	// Le rejeu est le premier consommateur qui evalue un graphe SANS le
	// contexte qui l'a construit. Une signature incomplete ou une relecture
	// infidele se voient ici, et par un resultat qui diverge.
	const std::string input = "./cli_same_input.obj";
	const std::string document = "./cli_same.json";
	WriteTetrahedron (input, 2.0f, 0);

	SourceAndSink chain (input, "./cli_same_{hash}.obj");
	chain.graph.FindNode (chain.load)->GetParams ().SetString ("path", input);

	Evaluator direct (chain.graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (direct.Evaluate (chain.save, outputs, ctx).IsOk ());

	const SaveMeshNode *sink = static_cast<const SaveMeshNode *> (chain.graph.FindNode (chain.save));
	ASSERT_EQ (sink->GetWrittenPaths ().size (), 1u);
	const std::string expectedPath = sink->GetWrittenPaths ()[0];
	const std::vector<unsigned char> expectedBytes = ReadAllBytes (expectedPath);
	ASSERT_FALSE (expectedBytes.empty ());

	ASSERT_EQ (SaveGraphToFile (chain.graph, document), SerializeStatus::Ok);
	ASSERT_EQ (std::remove (expectedPath.c_str ()), 0);
	ASSERT_FALSE (Exists (expectedPath));

	RunRequest request;
	request.document = document;
	request.allSinks = true;
	const RunReport report = cggraph_nodes::Run (request);
	ASSERT_TRUE (report.IsOk ()) << report.detail;
	ASSERT_EQ (report.written.size (), 1u);
	EXPECT_EQ (report.written[0], expectedPath);
	EXPECT_EQ (ReadAllBytes (expectedPath), expectedBytes);

	std::remove (expectedPath.c_str ());
	std::remove (document.c_str ());
	std::remove (input.c_str ());
}

// ---------------------------------------------------------------------------
//  Identite de source, apres relecture
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_runner, a_reloaded_graph_restats_its_source_before_serving_its_cache)
{
	// Le trou ferme a l'etape 2 ne doit pas se rouvrir par la porte de la
	// serialisation : un graphe RELU tient l'identite de source que le document
	// portait, donc celle du dernier enregistrement. Sans releve avant le calcul
	// de la signature, il resservirait ce resultat perime sans jamais planter.
	const std::string input = "./cli_restat_input.obj";
	const std::string document = "./cli_restat.json";
	WriteTetrahedron (input, 1.0f, 0);

	SourceAndSink chain (input, "./cli_restat_{hash}.obj");
	ASSERT_EQ (SaveGraphToFile (chain.graph, document), SerializeStatus::Ok);

	Graph graph;
	const CatalogFactory factory;
	ASSERT_TRUE (LoadGraphFromFile (document, factory, graph).IsOk ());

	LoadMeshNode *source = static_cast<LoadMeshNode *> (graph.FindNode (chain.load));
	SaveMeshNode *sink = static_cast<SaveMeshNode *> (graph.FindNode (chain.save));
	ASSERT_NE (source, nullptr);
	ASSERT_NE (sink, nullptr);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (chain.save, outputs, ctx).IsOk ());
	EXPECT_EQ (source->GetReadCount (), 1u);
	ASSERT_EQ (sink->GetWrittenPaths ().size (), 1u);

	// Le fichier change SOUS le graphe deja charge.
	WriteTetrahedron (input, 3.0f, 2);
	ASSERT_TRUE (evaluator.Evaluate (chain.save, outputs, ctx).IsOk ());

	EXPECT_EQ (source->GetReadCount (), 2u);
	ASSERT_EQ (sink->GetWrittenPaths ().size (), 2u);
	EXPECT_NE (sink->GetWrittenPaths ()[0], sink->GetWrittenPaths ()[1]);

	// Versant symetrique : un fichier inchange n'est pas relu, faute de quoi le
	// releve d'identite couterait ce que le cache existe pour economiser.
	ASSERT_TRUE (evaluator.Evaluate (chain.save, outputs, ctx).IsOk ());
	EXPECT_EQ (source->GetReadCount (), 2u);

	std::remove (sink->GetWrittenPaths ()[0].c_str ());
	std::remove (sink->GetWrittenPaths ()[1].c_str ());
	std::remove (document.c_str ());
	std::remove (input.c_str ());
}

// ---------------------------------------------------------------------------
//  Versionnement, vu du pilote
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_runner, a_document_of_another_node_version_is_read_but_not_launched)
{
	const std::string input = "./cli_version_input.obj";
	const std::string document = "./cli_version.json";
	const std::string output = "./cli_version_out.obj";
	WriteTetrahedron (input, 1.0f, 0);
	std::remove (output.c_str ());

	SourceAndSink chain (input, output);
	ASSERT_EQ (SaveGraphToFile (chain.graph, document), SerializeStatus::Ok);

	// Le document est reecrit en declarant le puits sous une version que le
	// catalogue ne publie pas.
	{
		std::vector<unsigned char> bytes = ReadAllBytes (document);
		std::string text (bytes.begin (), bytes.end ());
		const std::string::size_type at = text.rfind ("\"version\": 1");
		ASSERT_NE (at, std::string::npos);
		text.replace (at, std::string ("\"version\": 1").size (), "\"version\": 9");
		FILE *file = std::fopen (document.c_str (), "wb");
		ASSERT_NE (file, nullptr);
		std::fwrite (text.data (), 1, text.size (), file);
		std::fclose (file);
	}

	// LU : le document se charge, le graphe est complet, le puits est visible.
	const RunReport checked = cggraph_nodes::Check (document);
	EXPECT_EQ (checked.status, RunStatus::IncompatibleDocument);
	EXPECT_EQ (checked.nodeCount, 2u);
	EXPECT_EQ (checked.linkCount, 1u);
	ASSERT_EQ (checked.incompatible.size (), 1u);
	EXPECT_EQ (checked.incompatible[0], chain.save);

	// NON LANCE : meme sous une demande explicite, et rien n'est ecrit.
	RunRequest request;
	request.document = document;
	request.allSinks = true;
	const RunReport report = cggraph_nodes::Run (request);
	EXPECT_EQ (report.status, RunStatus::IncompatibleDocument);
	EXPECT_TRUE (report.evaluated.empty ());
	EXPECT_TRUE (report.written.empty ());
	EXPECT_FALSE (Exists (output));

	std::remove (document.c_str ());
	std::remove (input.c_str ());
}

TEST (TEST_cggraph_runner, a_document_at_the_published_versions_runs)
{
	// Versant symetrique du precedent : sans lui, « le document perime ne se
	// lance pas » serait satisfait par un pilote qui ne lance jamais rien.
	const std::string input = "./cli_current_input.obj";
	const std::string document = "./cli_current.json";
	WriteTetrahedron (input, 1.0f, 0);

	SourceAndSink chain (input, "./cli_current_{hash}.obj");
	ASSERT_EQ (SaveGraphToFile (chain.graph, document), SerializeStatus::Ok);

	const RunReport checked = cggraph_nodes::Check (document);
	ASSERT_TRUE (checked.IsOk ()) << checked.detail;
	EXPECT_TRUE (checked.incompatible.empty ());
	EXPECT_TRUE (checked.written.empty ());

	RunRequest request;
	request.document = document;
	request.allSinks = true;
	const RunReport report = cggraph_nodes::Run (request);
	ASSERT_TRUE (report.IsOk ()) << report.detail;
	ASSERT_EQ (report.written.size (), 1u);

	std::remove (report.written[0].c_str ());
	std::remove (document.c_str ());
	std::remove (input.c_str ());
}

// ---------------------------------------------------------------------------
//  La fabrique et le catalogue
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_runner, every_catalog_type_survives_the_round_trip)
{
	// La fabrique n'est que le catalogue vu par l'interface du moteur : une
	// seconde liste de types divergerait de la premiere. Ce cas echoue des
	// qu'un type publie ne sait pas se reconstruire depuis son nom.
	Graph graph;
	std::vector<NodeId> ids;
	for (const CatalogEntry &entry : Catalog ())
	{
		const NodeId id = graph.AddNode (MakeNode (entry.typeName));
		ASSERT_NE (id, kInvalidNodeId) << entry.typeName;
		graph.SetNodePosition (id, static_cast<float> (id) * 40.0f, -12.5f);
		ids.push_back (id);
	}
	ASSERT_EQ (ids.size (), Catalog ().size ());

	const std::string text = SaveGraph (graph);
	Graph reloaded;
	const CatalogFactory factory;
	const LoadResult result = LoadGraph (text, factory, reloaded);
	ASSERT_TRUE (result.IsOk ()) << result.detail;
	EXPECT_TRUE (result.incompatible.empty ());
	EXPECT_EQ (SaveGraph (reloaded), text);

	for (NodeId id : ids)
	{
		const Node *before = graph.FindNode (id);
		const Node *after = reloaded.FindNode (id);
		ASSERT_NE (after, nullptr);
		EXPECT_EQ (after->GetDesc ().typeName, before->GetDesc ().typeName);
		EXPECT_EQ (after->GetParams ().GetCount (), before->GetParams ().GetCount ());
		EXPECT_EQ (Signature (reloaded, id), Signature (graph, id));
	}
}
