#include <gtest/gtest.h>

#include "../src/cggraph/core/evaluator.h"
#include "../src/cggraph/core/validate.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

// ===========================================================================
//  Couche A : validation AVANT calcul
// ===========================================================================
// Ce que ces tests etablissent tient en une phrase : la validation ENUMERE la
// ou l'evaluation REFUSE. Chaque cas qui les compare est monte pour que les
// deux reponses different -- un graphe a un seul manque ne prouverait rien,
// les deux API y rendant la meme chose.
//
// Types INVENTES, comme partout dans la couche A : le moteur ne connait aucune
// bibliotheque de domaine, et ces tests n'en incluent aucune.

using namespace cggraph;

namespace {

struct Payload
{
	int value = 0;
};

std::size_t SizeOfPayload (const void *)
{
	return sizeof (Payload);
}

TypeRegistry &Registry ()
{
	static TypeRegistry registry;
	return registry;
}

const TypeDesc *PayloadType ()
{
	static const TypeDesc *type = [] {
		TypeDesc desc;
		desc.name = "test.Payload";
		desc.sizeHint = &SizeOfPayload;
		desc.mutability = TypeDesc::Immutable;
		return Registry ().Register (desc);
	}();
	return type;
}

// Compte ce que la validation ne doit JAMAIS declencher. Sans ces deux
// compteurs, « la validation n'evalue pas » ne serait qu'une intention.
class CountingNode : public Node
{
public:
	CountingNode (const std::string &typeName,
	              const std::vector<std::pair<std::string, bool> > &inputs, std::size_t outputs)
	{
		m_desc.typeName = typeName;
		for (std::size_t i = 0; i < inputs.size (); ++i)
		{
			PortDesc port;
			port.name = inputs[i].first;
			port.type = PayloadType ();
			port.optional = inputs[i].second;
			m_desc.inputs.push_back (port);
		}
		for (std::size_t i = 0; i < outputs; ++i)
		{
			PortDesc port;
			port.name = "sortie";
			port.type = PayloadType ();
			m_desc.outputs.push_back (port);
		}
	}

	const NodeDesc &GetDesc () const override { return m_desc; }

	bool Compute (EvalContext &, const ValueList &, ValueList &out) override
	{
		++m_computes;
		if (!out.empty ())
			out[0] = Value::Make (PayloadType (), std::make_shared<Payload> ());
		return true;
	}

	void RefreshExternalState () override { ++m_refreshes; }

	std::size_t GetComputeCount () const { return m_computes; }
	std::size_t GetRefreshCount () const { return m_refreshes; }

private:
	NodeDesc m_desc;
	std::size_t m_computes = 0;
	std::size_t m_refreshes = 0;
};

NodeId AddCounting (Graph &graph, const std::string &typeName,
                    const std::vector<std::pair<std::string, bool> > &inputs, std::size_t outputs,
                    CountingNode **kept = nullptr)
{
	std::unique_ptr<CountingNode> node (new CountingNode (typeName, inputs, outputs));
	CountingNode *raw = node.get ();
	const NodeId id = graph.AddNode (std::move (node));
	if (kept != nullptr)
		*kept = raw;
	return id;
}

std::vector<std::pair<std::string, bool> > Ports (const std::string &a, bool aOptional)
{
	std::vector<std::pair<std::string, bool> > ports;
	ports.push_back (std::make_pair (a, aOptional));
	return ports;
}

std::vector<std::pair<std::string, bool> > Ports (const std::string &a, bool aOptional,
                                                  const std::string &b, bool bOptional)
{
	std::vector<std::pair<std::string, bool> > ports = Ports (a, aOptional);
	ports.push_back (std::make_pair (b, bOptional));
	return ports;
}

const InputStatus *FindInput (const NodeValidation &validation, const std::string &name)
{
	for (std::size_t i = 0; i < validation.inputs.size (); ++i)
		if (validation.inputs[i].name == name)
			return &validation.inputs[i];
	return nullptr;
}

const NodeValidation *FindNode (const BranchValidation &validation, NodeId id)
{
	for (std::size_t i = 0; i < validation.nodes.size (); ++i)
		if (validation.nodes[i].node == id)
			return &validation.nodes[i];
	return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
//  Le noeud seul
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_validate, a_node_whose_inputs_are_all_wired_is_ready)
{
	Graph graph;
	const NodeId source = AddCounting (graph, "test.Source", {}, 1);
	const NodeId sink = AddCounting (graph, "test.Sink", Ports ("entree", false), 1);
	ASSERT_EQ (ConnectStatus::Ok, graph.Connect (source, 0, sink, 0));

	const NodeValidation validation = ValidateNode (graph, sink);
	EXPECT_EQ (NodeReadiness::Ready, validation.readiness);
	ASSERT_EQ (1u, validation.inputs.size ());
	EXPECT_EQ (PortState::Connected, validation.inputs[0].state);
	EXPECT_EQ (0u, validation.GetMissingRequiredCount ());
}

TEST (TEST_cggraph_validate, a_free_required_input_and_a_free_optional_one_are_two_STATES_not_one)
{
	// Le coeur du critere : sans etat nomme, « non cable » serait la meme
	// reponse pour les deux, et l'editeur griserait les deux ou aucune.
	Graph graph;
	const NodeId node = AddCounting (graph, "test.Two", Ports ("obligatoire", false, "zone", true), 1);

	const NodeValidation validation = ValidateNode (graph, node);
	EXPECT_EQ (NodeReadiness::MissingRequiredInput, validation.readiness);

	const InputStatus *required = FindInput (validation, "obligatoire");
	const InputStatus *optional = FindInput (validation, "zone");
	ASSERT_NE (nullptr, required);
	ASSERT_NE (nullptr, optional);
	EXPECT_EQ (PortState::MissingRequired, required->state);
	EXPECT_EQ (PortState::MissingOptional, optional->state);
	EXPECT_EQ (1u, validation.GetMissingRequiredCount ());
}

TEST (TEST_cggraph_validate, the_ports_are_published_in_declaration_order_with_their_type)
{
	Graph graph;
	std::vector<std::pair<std::string, bool> > ports = Ports ("a", false, "b", true);
	ports.push_back (std::make_pair (std::string ("c"), false));
	const NodeId node = AddCounting (graph, "test.Three", ports, 1);

	const NodeValidation validation = ValidateNode (graph, node);
	ASSERT_EQ (3u, validation.inputs.size ());
	EXPECT_EQ ("a", validation.inputs[0].name);
	EXPECT_EQ ("b", validation.inputs[1].name);
	EXPECT_EQ ("c", validation.inputs[2].name);
	EXPECT_EQ (0u, validation.inputs[0].port);
	EXPECT_EQ (2u, validation.inputs[2].port);
	EXPECT_EQ (PayloadType (), validation.inputs[1].type);
}

TEST (TEST_cggraph_validate, an_unknown_node_is_a_NAMED_state_and_not_an_empty_list)
{
	// Une liste vide se confondrait avec « ce noeud n'a aucune entree », qui est
	// un etat legitime -- toute source en est un.
	Graph graph;
	const NodeId source = AddCounting (graph, "test.Source", {}, 1);

	const NodeValidation known = ValidateNode (graph, source);
	EXPECT_EQ (NodeReadiness::Ready, known.readiness);
	EXPECT_TRUE (known.inputs.empty ());

	const NodeValidation unknown = ValidateNode (graph, source + 100);
	EXPECT_EQ (NodeReadiness::UnknownNode, unknown.readiness);
	EXPECT_TRUE (unknown.inputs.empty ());
}

TEST (TEST_cggraph_validate, debranching_an_input_returns_it_to_missing)
{
	Graph graph;
	const NodeId source = AddCounting (graph, "test.Source", {}, 1);
	const NodeId sink = AddCounting (graph, "test.Sink", Ports ("entree", false), 1);
	ASSERT_EQ (ConnectStatus::Ok, graph.Connect (source, 0, sink, 0));
	ASSERT_EQ (PortState::Connected, ValidateNode (graph, sink).inputs[0].state);

	ASSERT_TRUE (graph.Disconnect (sink, 0));
	const NodeValidation validation = ValidateNode (graph, sink);
	EXPECT_EQ (PortState::MissingRequired, validation.inputs[0].state);
	EXPECT_EQ (NodeReadiness::MissingRequiredInput, validation.readiness);
}

// ---------------------------------------------------------------------------
//  Enumerer, la ou l'evaluation refuse
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_validate, validation_lists_ALL_the_missing_ports_where_evaluate_names_only_the_first)
{
	Graph graph;
	const NodeId node = AddCounting (graph, "test.Two", Ports ("premier", false, "second", false), 1);

	// Evaluate : un seul nom, celui du premier refus rencontre.
	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	const EvalResult result = evaluator.Evaluate (node, outputs, ctx);
	EXPECT_EQ (EvalStatus::MissingInput, result.status);
	EXPECT_EQ ("premier", result.detail);

	// Validate : les deux, d'un coup, sans calculer.
	const NodeValidation validation = ValidateNode (graph, node);
	EXPECT_EQ (2u, validation.GetMissingRequiredCount ());
	ASSERT_NE (nullptr, FindInput (validation, "premier"));
	ASSERT_NE (nullptr, FindInput (validation, "second"));
	EXPECT_EQ (PortState::MissingRequired, FindInput (validation, "premier")->state);
	EXPECT_EQ (PortState::MissingRequired, FindInput (validation, "second")->state);
}

TEST (TEST_cggraph_validate, the_branch_reports_every_incomplete_node_where_evaluate_stops_at_the_first)
{
	// Deux noeuds incomplets sur la meme branche, a deux profondeurs
	// differentes : l'evaluation s'arrete au premier qu'elle rencontre en
	// descendant, la validation les rend tous les deux.
	Graph graph;
	const NodeId upstream = AddCounting (graph, "test.Upstream", Ports ("amont", false), 1);
	const NodeId downstream =
	    AddCounting (graph, "test.Downstream", Ports ("aval", false, "second", false), 1);
	ASSERT_EQ (ConnectStatus::Ok, graph.Connect (upstream, 0, downstream, 0));

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	const EvalResult result = evaluator.Evaluate (downstream, outputs, ctx);
	EXPECT_EQ (EvalStatus::MissingInput, result.status);
	EXPECT_EQ (upstream, result.node);
	EXPECT_EQ ("amont", result.detail);

	const BranchValidation validation = ValidateBranch (graph, downstream);
	EXPECT_EQ (NodeReadiness::MissingRequiredInput, validation.readiness);
	EXPECT_EQ (2u, validation.nodes.size ());
	EXPECT_EQ (2u, validation.GetMissingRequiredCount ());

	const NodeValidation *up = FindNode (validation, upstream);
	const NodeValidation *down = FindNode (validation, downstream);
	ASSERT_NE (nullptr, up);
	ASSERT_NE (nullptr, down);
	EXPECT_EQ (1u, up->GetMissingRequiredCount ());
	EXPECT_EQ (1u, down->GetMissingRequiredCount ());
	ASSERT_NE (nullptr, FindInput (*down, "second"));
	EXPECT_EQ (PortState::MissingRequired, FindInput (*down, "second")->state);
}

TEST (TEST_cggraph_validate, a_node_ready_ON_ITS_OWN_can_sit_on_a_branch_that_is_not)
{
	// C'est la difference entre les deux API, et elle se voit sur un seul
	// graphe : la boite se dessine complete, le bouton « calculer » reste gris.
	Graph graph;
	const NodeId upstream = AddCounting (graph, "test.Upstream", Ports ("amont", false), 1);
	const NodeId downstream = AddCounting (graph, "test.Downstream", Ports ("aval", false), 1);
	ASSERT_EQ (ConnectStatus::Ok, graph.Connect (upstream, 0, downstream, 0));

	EXPECT_EQ (NodeReadiness::Ready, ValidateNode (graph, downstream).readiness);
	EXPECT_EQ (NodeReadiness::MissingRequiredInput, ValidateBranch (graph, downstream).readiness);
}

TEST (TEST_cggraph_validate, a_complete_branch_is_ready_and_the_evaluation_agrees)
{
	// Versant symetrique : sans lui, « la branche n'est pas prete » serait
	// satisfait par une validation qui refuse tout.
	Graph graph;
	const NodeId source = AddCounting (graph, "test.Source", {}, 1);
	const NodeId middle = AddCounting (graph, "test.Middle", Ports ("entree", false, "zone", true), 1);
	const NodeId sink = AddCounting (graph, "test.Sink", Ports ("entree", false), 1);
	ASSERT_EQ (ConnectStatus::Ok, graph.Connect (source, 0, middle, 0));
	ASSERT_EQ (ConnectStatus::Ok, graph.Connect (middle, 0, sink, 0));

	const BranchValidation validation = ValidateBranch (graph, sink);
	EXPECT_EQ (NodeReadiness::Ready, validation.readiness);
	EXPECT_EQ (0u, validation.GetMissingRequiredCount ());
	EXPECT_EQ (3u, validation.nodes.size ());

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	EXPECT_EQ (EvalStatus::Ok, evaluator.Evaluate (sink, outputs, ctx).status);
}

TEST (TEST_cggraph_validate, the_branch_lists_upstream_first_and_a_shared_node_only_once)
{
	// Ordre reproductible, et un losange : sans deduplication, le noeud partage
	// compterait deux fois son port manquant et l'editeur signalerait deux fois
	// la meme boite.
	Graph graph;
	const NodeId shared = AddCounting (graph, "test.Shared", Ports ("manquante", false), 1);
	const NodeId left = AddCounting (graph, "test.Left", Ports ("entree", false), 1);
	const NodeId right = AddCounting (graph, "test.Right", Ports ("entree", false), 1);
	const NodeId join = AddCounting (graph, "test.Join", Ports ("a", false, "b", false), 1);
	ASSERT_EQ (ConnectStatus::Ok, graph.Connect (shared, 0, left, 0));
	ASSERT_EQ (ConnectStatus::Ok, graph.Connect (shared, 0, right, 0));
	ASSERT_EQ (ConnectStatus::Ok, graph.Connect (left, 0, join, 0));
	ASSERT_EQ (ConnectStatus::Ok, graph.Connect (right, 0, join, 1));

	const BranchValidation validation = ValidateBranch (graph, join);
	ASSERT_EQ (4u, validation.nodes.size ());
	EXPECT_EQ (shared, validation.nodes[0].node);
	EXPECT_EQ (join, validation.nodes[3].node);
	EXPECT_EQ (1u, validation.GetMissingRequiredCount ());
}

TEST (TEST_cggraph_validate, the_branch_of_an_unknown_node_is_unknown_and_not_ready)
{
	Graph graph;
	const NodeId source = AddCounting (graph, "test.Source", {}, 1);

	const BranchValidation validation = ValidateBranch (graph, source + 100);
	EXPECT_EQ (NodeReadiness::UnknownNode, validation.readiness);
	ASSERT_EQ (1u, validation.nodes.size ());
	EXPECT_EQ (NodeReadiness::UnknownNode, validation.nodes[0].readiness);
}

// ---------------------------------------------------------------------------
//  Ce que la validation ne fait pas
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_validate, validating_neither_computes_nor_touches_the_outside_world)
{
	// « Avant calcul » n'est pas une figure de style. Un RefreshExternalState
	// declenche ici irait lire le disque a chaque frame de l'editeur.
	Graph graph;
	CountingNode *sourceNode = nullptr;
	CountingNode *sinkNode = nullptr;
	const NodeId source = AddCounting (graph, "test.Source", {}, 1, &sourceNode);
	const NodeId sink = AddCounting (graph, "test.Sink", Ports ("entree", false), 1, &sinkNode);
	ASSERT_EQ (ConnectStatus::Ok, graph.Connect (source, 0, sink, 0));

	ValidateNode (graph, sink);
	ValidateBranch (graph, sink);

	EXPECT_EQ (0u, sourceNode->GetComputeCount ());
	EXPECT_EQ (0u, sinkNode->GetComputeCount ());
	EXPECT_EQ (0u, sourceNode->GetRefreshCount ());
	EXPECT_EQ (0u, sinkNode->GetRefreshCount ());

	// Et le temoin dit bien quelque chose : l'evaluation, elle, les incremente.
	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_EQ (EvalStatus::Ok, evaluator.Evaluate (sink, outputs, ctx).status);
	EXPECT_EQ (1u, sourceNode->GetComputeCount ());
	EXPECT_EQ (1u, sourceNode->GetRefreshCount ());
}
