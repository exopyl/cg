#include "subgraph_support.h"

#include <algorithm>
#include <filesystem>

#include "../catalog_factory.h"
#include "../node_support.h"
#include "boundary.h"

namespace cggraph_nodes
{
namespace flow
{

namespace
{

// Pile des references en cours de chargement SUR CE FIL. Elle est thread_local
// et non globale : deux fils qui chargent chacun sa hierarchie de documents ne
// se genent pas, et une garde partagee refuserait a l'un ce que l'autre fait.
thread_local std::vector<std::string> g_loading;

std::string Canonical (const std::string &reference)
{
	// weakly_canonical resout ".." et les liens sans exiger que le fichier
	// existe. Elle leve sur certaines entrees ; le repli sur la chaine brute
	// n'est pas une garde plus faible, c'est la garde exacte devenue une garde
	// par orthographe -- la profondeur maximale prend alors le relais.
	try
	{
		return std::filesystem::weakly_canonical (std::filesystem::path (reference)).string ();
	}
	catch (const std::exception &)
	{
		return reference;
	}
}

class LoadingGuard
{
public:
	explicit LoadingGuard (const std::string &key) { g_loading.push_back (key); }
	~LoadingGuard () { g_loading.pop_back (); }

	LoadingGuard (const LoadingGuard &) = delete;
	LoadingGuard &operator= (const LoadingGuard &) = delete;
};

bool Less (const Boundary &left, const Boundary &right)
{
	// L'identifiant departage deux slots egaux. Sans ce second critere, l'ordre
	// des ports d'un document dependrait de l'ordre de parcours du graphe, donc
	// il pourrait changer sans que le document change.
	if (left.slot != right.slot)
		return left.slot < right.slot;
	return left.id < right.id;
}

Boundary Describe (const cggraph::Graph &graph, cggraph::NodeId id)
{
	Boundary boundary;
	boundary.id = id;
	boundary.name = kDefaultPortName;

	const cggraph::Node *node = graph.FindNode (id);
	if (node == nullptr)
		return boundary;

	boundary.slot = GetInt (node->GetParams (), "slot", 0);
	const std::string name = GetString (node->GetParams (), "nom", std::string ());
	if (!name.empty ())
		boundary.name = name;
	return boundary;
}

} // namespace

const char *ToString (LoadStatus status)
{
	switch (status)
	{
	case LoadStatus::Ok:
		return "ok";
	case LoadStatus::NoReference:
		return "noReference";
	case LoadStatus::FileNotReadable:
		return "fileNotReadable";
	case LoadStatus::DocumentInvalid:
		return "documentInvalid";
	case LoadStatus::Recursive:
		return "recursive";
	case LoadStatus::TooDeep:
		return "tooDeep";
	}
	return "unknown";
}

LoadStatus LoadSubgraph (const std::string &reference, SubgraphInstance &instance,
                         std::string &detail)
{
	detail.clear ();
	if (reference.empty ())
		return LoadStatus::NoReference;

	const std::string key = Canonical (reference);
	if (std::find (g_loading.begin (), g_loading.end (), key) != g_loading.end ())
	{
		detail = reference;
		return LoadStatus::Recursive;
	}
	if (g_loading.size () >= kMaxSubgraphDepth)
	{
		detail = reference;
		return LoadStatus::TooDeep;
	}

	const LoadingGuard guard (key);

	const CatalogFactory factory;
	const cggraph::LoadResult result =
		cggraph::LoadGraphFromFile (reference, factory, instance.graph);
	if (!result.IsOk ())
	{
		detail = std::string (cggraph::ToString (result.status));
		if (!result.detail.empty ())
			detail += ": " + result.detail;
		return result.status == cggraph::SerializeStatus::FileNotReadable
			       ? LoadStatus::FileNotReadable
			       : LoadStatus::DocumentInvalid;
	}

	for (cggraph::NodeId id : instance.graph.GetNodeIds ())
	{
		const cggraph::Node *node = instance.graph.FindNode (id);
		if (node == nullptr)
			continue;

		const cggraph::NodeDesc &desc = node->GetDesc ();
		if (desc.typeName == kInputTypeName)
			instance.inputs.push_back (Describe (instance.graph, id));
		else if (desc.typeName == kOutputTypeName)
			instance.outputs.push_back (Describe (instance.graph, id));

		// Un noeud de flux imbrique a DEJA derive son propre drapeau de son
		// propre document : lire le descripteur suffit, et la propriete remonte
		// d'elle-meme sur toute la profondeur.
		if (desc.sideEffect)
			instance.sinks.push_back (id);
	}

	std::sort (instance.inputs.begin (), instance.inputs.end (), Less);
	std::sort (instance.outputs.begin (), instance.outputs.end (), Less);
	return LoadStatus::Ok;
}

std::vector<std::string> CollectWritten (const SubgraphInstance &instance)
{
	std::vector<std::string> written;
	for (cggraph::NodeId sink : instance.sinks)
	{
		const FileSink *file = dynamic_cast<const FileSink *> (instance.graph.FindNode (sink));
		if (file == nullptr)
			continue;
		const std::vector<std::string> paths = file->GetWrittenPaths ();
		written.insert (written.end (), paths.begin (), paths.end ());
	}
	return written;
}

PassResult RunPass (SubgraphInstance &instance, cggraph::Evaluator &evaluator,
                    cggraph::EvalContext &ctx, const std::string &binding,
                    const std::vector<cggraph::Value> &in, std::vector<cggraph::Value> &out,
                    bool runSinks)
{
	PassResult result;

	for (std::size_t i = 0; i < instance.inputs.size (); ++i)
	{
		GraphInputNode *port =
			dynamic_cast<GraphInputNode *> (instance.graph.FindNode (instance.inputs[i].id));
		if (port == nullptr)
		{
			result.status = cggraph::EvalStatus::UnknownNode;
			result.node = instance.inputs[i].id;
			return result;
		}
		port->Bind (i < in.size () ? in[i] : cggraph::Value (), binding);
	}

	out.assign (instance.outputs.size (), cggraph::Value ());
	for (std::size_t i = 0; i < instance.outputs.size (); ++i)
	{
		cggraph::ValueList produced;
		const cggraph::EvalResult inner =
			evaluator.Evaluate (instance.outputs[i].id, produced, ctx);
		if (!inner.IsOk ())
		{
			result.status = inner.status;
			result.node = inner.node;
			result.detail = inner.detail;
			return result;
		}
		if (!produced.empty ())
			out[i] = produced[0];
	}

	// Les puits APRES les sorties, et seulement sur demande. Apres, parce que le
	// cache interne rempli par les sorties leur epargne le recalcul de la branche
	// commune ; sur demande, parce que « execute seulement sur demande explicite »
	// (nodal.md §11.4) ne survit pas a un pilote qui choisit lui-meme quoi tirer,
	// et un noeud hote EST un tel pilote.
	if (runSinks)
		for (cggraph::NodeId sink : instance.sinks)
		{
			cggraph::ValueList produced;
			const cggraph::EvalResult inner = evaluator.Evaluate (sink, produced, ctx);
			if (!inner.IsOk ())
			{
				result.status = inner.status;
				result.node = inner.node;
				result.detail = inner.detail;
				return result;
			}
		}

	return result;
}

} // namespace flow
} // namespace cggraph_nodes
