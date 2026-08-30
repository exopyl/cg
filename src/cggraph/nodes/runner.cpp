#include "runner.h"

#include "catalog_factory.h"
#include "node_support.h"

namespace cggraph_nodes
{

namespace
{

using cggraph::Graph;
using cggraph::NodeId;

RunReport Refuse (RunStatus status, const std::string &detail)
{
	RunReport report;
	report.status = status;
	report.detail = detail;
	return report;
}

void Describe (const Graph &graph, const cggraph::LoadResult &loaded, RunReport &report)
{
	report.nodeCount = graph.GetNodeCount ();
	report.linkCount = graph.GetLinks ().size ();
	report.incompatible = loaded.incompatible;
	report.sinks = FindSinks (graph);
}

// Ce que les puits ont ecrit, releve APRES l'evaluation : un puits qui n'a pas
// ete tire n'a rien a declarer, et c'est precisement ce que le test de la
// demande explicite verifie.
void CollectWritten (const Graph &graph, RunReport &report)
{
	for (NodeId id : graph.GetNodeIds ())
	{
		const cggraph::Node *node = graph.FindNode (id);
		const FileSink *sink = dynamic_cast<const FileSink *> (node);
		if (sink == nullptr)
			continue;
		for (const std::string &path : sink->GetWrittenPaths ())
			report.written.push_back (path);
	}
}

RunReport Open (const std::string &document, Graph &graph)
{
	const CatalogFactory factory;
	const cggraph::LoadResult loaded = cggraph::LoadGraphFromFile (document, factory, graph);
	if (!loaded.IsOk ())
	{
		std::string detail = cggraph::ToString (loaded.status);
		if (!loaded.detail.empty ())
			detail += " (" + loaded.detail + ")";
		return Refuse (RunStatus::LoadFailed, detail);
	}

	RunReport report;
	Describe (graph, loaded, report);
	return report;
}

} // namespace

const char *ToString (RunStatus status)
{
	switch (status)
	{
	case RunStatus::Ok:
		return "ok";
	case RunStatus::LoadFailed:
		return "document illisible";
	case RunStatus::NoTarget:
		return "aucune cible designee";
	case RunStatus::UnknownTarget:
		return "cible inconnue";
	case RunStatus::IncompatibleDocument:
		return "document ecrit sous une autre version";
	case RunStatus::EvaluationFailed:
		return "evaluation refusee";
	}
	return "statut inconnu";
}

std::vector<NodeId> FindSinks (const Graph &graph)
{
	std::vector<NodeId> sinks;
	for (NodeId id : graph.GetNodeIds ())
	{
		bool hasDownstream = false;
		for (const cggraph::Link &link : graph.GetLinks ())
			if (link.from == id)
			{
				hasDownstream = true;
				break;
			}
		if (!hasDownstream)
			sinks.push_back (id);
	}
	return sinks;
}

RunReport Check (const std::string &document)
{
	Graph graph;
	RunReport report = Open (document, graph);
	if (report.status != RunStatus::Ok)
		return report;

	if (!report.incompatible.empty ())
		report.status = RunStatus::IncompatibleDocument;
	return report;
}

RunReport Run (const RunRequest &request)
{
	Graph graph;
	RunReport report = Open (request.document, graph);
	if (report.status != RunStatus::Ok)
		return report;

	// Avant toute evaluation : un document non migre se lit, se regarde et se
	// modifie ; il ne se lance pas. Le refus porte sur le DOCUMENT et non sur
	// la branche tiree -- lancer la moitie compatible d'un graphe produirait un
	// resultat qu'aucune version n'a jamais decrit.
	if (!report.incompatible.empty ())
	{
		report.status = RunStatus::IncompatibleDocument;
		return report;
	}

	std::vector<NodeId> targets = request.targets;
	if (request.allSinks)
		for (NodeId id : report.sinks)
			targets.push_back (id);

	// Aucune cible : rien n'est evalue, donc aucun effet de bord ne se produit.
	// Un pilote qui choisirait de lui-meme quoi tirer ecrirait des fichiers que
	// personne n'a demandes.
	if (targets.empty ())
	{
		report.status = RunStatus::NoTarget;
		return report;
	}

	cggraph::Evaluator evaluator (graph);
	cggraph::EvalContext ctx;

	for (NodeId id : targets)
	{
		if (graph.FindNode (id) == nullptr)
		{
			report.status = RunStatus::UnknownTarget;
			report.detail = std::to_string (id);
			CollectWritten (graph, report);
			return report;
		}

		cggraph::ValueList outputs;
		const cggraph::EvalResult result = evaluator.Evaluate (id, outputs, ctx);
		if (!result.IsOk ())
		{
			report.status = RunStatus::EvaluationFailed;
			report.evalStatus = result.status;
			report.detail = result.detail;
			report.evaluated.push_back (id);
			CollectWritten (graph, report);
			return report;
		}
		report.evaluated.push_back (id);
	}

	CollectWritten (graph, report);
	return report;
}

} // namespace cggraph_nodes
