#include <gtest/gtest.h>

#include "../src/cggraph/core/param_set.h"
#include "../src/cggraph/core/signature.h"

#include <memory>
#include <string>
#include <vector>

// ===========================================================================
//  Couche A : jeu de parametres et signature structurelle
// ===========================================================================
// Ces tests ne connaissent PAS le cache, et c'est leur raison d'etre. Une
// signature incomplete rend un resultat faux sans jamais planter : elle doit
// donc etre prise au filet avant qu'un cache existe pour en masquer les effets.
//
// Le filet ci-dessous fait varier CHAQUE parametre declare semantique d'un
// catalogue de types de noeuds et exige que la signature bouge, puis fait
// varier chaque parametre non semantique et exige qu'elle ne bouge pas. Il
// echoue si un seul des deux versants cede. Le catalogue est une table : la
// couche B l'etendra en y ajoutant ses propres types de noeuds.

using namespace cggraph;

namespace {

struct ParamSpec
{
	const char *name;
	ParamType type;
	ParamRole role;
	bool driven;
};

struct NodeSpec
{
	const char *typeName;
	std::vector<ParamSpec> params;
};

// variant 0 et variant 1 sont deux valeurs distinctes du meme parametre.
void Apply (ParamSet &params, const ParamSpec &spec, int variant)
{
	if (spec.driven)
	{
		params.SetDriven (spec.name, spec.type, variant == 0 ? "width / 2" : "width / 3", spec.role);
		return;
	}

	switch (spec.type)
	{
	case ParamType::Int:
		params.SetInt (spec.name, variant == 0 ? 3 : 4, spec.role);
		break;
	case ParamType::Float:
		params.SetFloat (spec.name, variant == 0 ? 0.5f : 0.75f, spec.role);
		break;
	case ParamType::Bool:
		params.SetBool (spec.name, variant != 0, spec.role);
		break;
	case ParamType::String:
		params.SetString (spec.name, variant == 0 ? "alpha" : "beta", spec.role);
		break;
	}
}

// Les quatre types de valeur et les deux roles sont couverts, Driven compris.
const std::vector<NodeSpec> &NodeCatalog ()
{
	static const std::vector<NodeSpec> catalog = {
		{ "test.LoadShape",
		  { { "path", ParamType::String, ParamRole::Semantic, false },
		    { "scale", ParamType::Float, ParamRole::Semantic, false },
		    { "verbose", ParamType::Bool, ParamRole::NonSemantic, false } } },
		{ "test.Smooth",
		  { { "iterations", ParamType::Int, ParamRole::Semantic, false },
		    { "lambda", ParamType::Float, ParamRole::Semantic, false },
		    { "preserveBorder", ParamType::Bool, ParamRole::Semantic, false },
		    { "logPath", ParamType::String, ParamRole::NonSemantic, false },
		    { "threads", ParamType::Int, ParamRole::NonSemantic, false } } },
		{ "test.Simplify",
		  { { "ratio", ParamType::Float, ParamRole::Semantic, false },
		    { "target", ParamType::Int, ParamRole::Semantic, false },
		    { "progressLabel", ParamType::String, ParamRole::NonSemantic, false },
		    { "quiet", ParamType::Bool, ParamRole::NonSemantic, false } } },
		{ "test.Scatter",
		  { { "count", ParamType::Int, ParamRole::Semantic, true },
		    { "seed", ParamType::Int, ParamRole::Semantic, false },
		    { "previewRatio", ParamType::Float, ParamRole::NonSemantic, false } } }
	};
	return catalog;
}

class StubNode : public Node
{
public:
	StubNode (const std::string &typeName, std::size_t inputs, std::size_t outputs, const TypeDesc *type)
	{
		m_desc.typeName = typeName;
		for (std::size_t i = 0; i < inputs; ++i)
		{
			PortDesc port;
			port.name = "in";
			port.type = type;
			m_desc.inputs.push_back (port);
		}
		for (std::size_t i = 0; i < outputs; ++i)
		{
			PortDesc port;
			port.name = "out";
			port.type = type;
			m_desc.outputs.push_back (port);
		}
	}

	const NodeDesc &GetDesc () const override { return m_desc; }

	bool Compute (EvalContext &, const ValueList &, ValueList &) override { return false; }

private:
	NodeDesc m_desc;
};

NodeId AddStub (Graph &graph, const std::string &typeName, std::size_t inputs, std::size_t outputs,
                const TypeDesc *type)
{
	return graph.AddNode (std::unique_ptr<Node> (new StubNode (typeName, inputs, outputs, type)));
}

TypeDesc MakeImmutableDesc (const std::string &name)
{
	TypeDesc desc;
	desc.name = name;
	desc.mutability = TypeDesc::Immutable;
	return desc;
}

} // namespace

// ---------------------------------------------------------------------------
//  Jeu de parametres
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_param_set, a_value_is_literal_or_driven_in_the_storage)
{
	ParamSet params;
	const ParamValue *literal = params.SetInt ("iterations", 12);
	const ParamValue *driven = params.SetDriven ("count", ParamType::Int, "width / 2");

	ASSERT_NE (literal, nullptr);
	ASSERT_NE (driven, nullptr);

	EXPECT_EQ (literal->kind, ParamKind::Literal);
	EXPECT_EQ (literal->intValue, 12);
	EXPECT_TRUE (literal->expression.empty ());

	EXPECT_EQ (driven->kind, ParamKind::Driven);
	EXPECT_EQ (driven->type, ParamType::Int);
	EXPECT_EQ (driven->expression, "width / 2");

	ASSERT_NE (params.FindDriven (), nullptr);
	EXPECT_EQ (params.FindDriven ()->name, "count");
}

TEST (TEST_cggraph_param_set, a_set_without_driven_value_reports_none)
{
	ParamSet params;
	params.SetFloat ("lambda", 0.5f);
	params.SetString ("label", "front");
	EXPECT_EQ (params.FindDriven (), nullptr);
}

TEST (TEST_cggraph_param_set, an_empty_name_is_refused)
{
	ParamSet params;
	EXPECT_EQ (params.SetInt ("", 1), nullptr);
	EXPECT_EQ (params.SetDriven ("", ParamType::Int, "a"), nullptr);
	EXPECT_EQ (params.GetCount (), 0u);
}

TEST (TEST_cggraph_param_set, setting_an_existing_name_replaces_it_in_place)
{
	ParamSet params;
	ParamValue *first = params.SetInt ("mode", 1);
	ASSERT_NE (first, nullptr);
	ParamValue *again = params.SetString ("mode", "flat");
	EXPECT_EQ (again, first);
	EXPECT_EQ (params.GetCount (), 1u);
	EXPECT_EQ (first->type, ParamType::String);
	EXPECT_EQ (first->stringValue, "flat");
	EXPECT_EQ (first->kind, ParamKind::Literal);
}

TEST (TEST_cggraph_param_set, values_keep_their_address_when_the_set_grows)
{
	ParamSet params;
	ParamValue *scale = params.SetFloat ("scale", 1.0f);
	ParamValue *label = params.SetString ("label", "front");
	ASSERT_NE (scale, nullptr);
	ASSERT_NE (label, nullptr);

	// Ce sont ces adresses-la que la projection vers un panneau distribue, en
	// lecture comme en ecriture.
	float *scaleCell = &scale->floatValue;
	std::string *labelCell = &label->stringValue;

	for (int i = 0; i < 512; ++i)
		ASSERT_NE (params.SetInt ("filler" + std::to_string (i), i), nullptr);

	EXPECT_EQ (params.Find ("scale"), scale);
	EXPECT_EQ (&scale->floatValue, scaleCell);
	EXPECT_EQ (&label->stringValue, labelCell);

	*scaleCell = 2.5f;
	EXPECT_FLOAT_EQ (params.Find ("scale")->floatValue, 2.5f);
	EXPECT_EQ (*labelCell, "front");
	EXPECT_EQ (params.GetCount (), 514u);
}

// ---------------------------------------------------------------------------
//  Le filet : chaque parametre semantique bouge, chaque autre ne bouge pas
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_signature, every_semantic_parameter_moves_the_signature)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeImmutableDesc ("test.Shape"));
	ASSERT_NE (type, nullptr);

	std::size_t checked = 0;
	for (const NodeSpec &spec : NodeCatalog ())
	{
		Graph graph;
		const NodeId id = AddStub (graph, spec.typeName, 0, 1, type);
		Node *node = graph.FindNode (id);
		ASSERT_NE (node, nullptr);

		for (const ParamSpec &param : spec.params)
			Apply (node->GetParams (), param, 0);

		const Hash base = Signature (graph, id);
		ASSERT_NE (base, kNoSignature);

		for (const ParamSpec &param : spec.params)
		{
			if (param.role != ParamRole::Semantic)
				continue;
			SCOPED_TRACE (std::string (spec.typeName) + "." + param.name);
			++checked;

			Apply (node->GetParams (), param, 1);
			EXPECT_NE (Signature (graph, id), base);

			Apply (node->GetParams (), param, 0);
			ASSERT_EQ (Signature (graph, id), base);
		}
	}

	// Un filet qui n'exerce rien ne peut pas echouer : le compte est verifie.
	EXPECT_EQ (checked, 9u);
}

TEST (TEST_cggraph_signature, no_non_semantic_parameter_moves_the_signature)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeImmutableDesc ("test.Shape"));
	ASSERT_NE (type, nullptr);

	std::size_t checked = 0;
	for (const NodeSpec &spec : NodeCatalog ())
	{
		Graph graph;
		const NodeId id = AddStub (graph, spec.typeName, 0, 1, type);
		Node *node = graph.FindNode (id);
		ASSERT_NE (node, nullptr);

		for (const ParamSpec &param : spec.params)
			Apply (node->GetParams (), param, 0);

		const Hash base = Signature (graph, id);
		ASSERT_NE (base, kNoSignature);

		for (const ParamSpec &param : spec.params)
		{
			if (param.role != ParamRole::NonSemantic)
				continue;
			SCOPED_TRACE (std::string (spec.typeName) + "." + param.name);
			++checked;

			Apply (node->GetParams (), param, 1);
			EXPECT_EQ (Signature (graph, id), base);
		}
	}

	EXPECT_EQ (checked, 6u);
}

TEST (TEST_cggraph_signature, a_driven_parameter_hashes_its_source)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeImmutableDesc ("test.Shape"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId id = AddStub (graph, "test.Scatter", 0, 1, type);
	Node *node = graph.FindNode (id);
	ASSERT_NE (node, nullptr);

	node->GetParams ().SetDriven ("count", ParamType::Int, "width / 2");
	const Hash base = Signature (graph, id);

	node->GetParams ().SetDriven ("count", ParamType::Int, "width / 3");
	EXPECT_NE (Signature (graph, id), base);

	// Une valeur litterale et une expression ne sont pas le meme parametre,
	// meme quand elles portent le meme type.
	node->GetParams ().SetInt ("count", 7);
	const Hash literal = Signature (graph, id);
	node->GetParams ().SetDriven ("count", ParamType::Int, "7");
	EXPECT_NE (Signature (graph, id), literal);
}

// ---------------------------------------------------------------------------
//  Ce que la signature couvre en propre
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_signature, an_upstream_change_moves_the_downstream_signature)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeImmutableDesc ("test.Shape"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId load = AddStub (graph, "test.LoadShape", 0, 1, type);
	const NodeId smooth = AddStub (graph, "test.Smooth", 1, 1, type);
	const NodeId save = AddStub (graph, "test.SaveShape", 1, 1, type);
	ASSERT_EQ (graph.Connect (load, 0, smooth, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (smooth, 0, save, 0), ConnectStatus::Ok);

	graph.FindNode (load)->GetParams ().SetString ("path", "shape.dat");
	graph.FindNode (smooth)->GetParams ().SetInt ("iterations", 4);

	const Hash baseSmooth = Signature (graph, smooth);
	const Hash baseSave = Signature (graph, save);

	graph.FindNode (load)->GetParams ().SetString ("path", "other.dat");
	EXPECT_NE (Signature (graph, smooth), baseSmooth);
	EXPECT_NE (Signature (graph, save), baseSave);
}

TEST (TEST_cggraph_signature, moving_a_box_on_screen_does_not_move_the_signature)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeImmutableDesc ("test.Shape"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId load = AddStub (graph, "test.LoadShape", 0, 1, type);
	const NodeId smooth = AddStub (graph, "test.Smooth", 1, 1, type);
	ASSERT_EQ (graph.Connect (load, 0, smooth, 0), ConnectStatus::Ok);

	const Hash base = Signature (graph, smooth);

	ASSERT_TRUE (graph.SetNodePosition (load, 120.0f, -40.0f));
	ASSERT_TRUE (graph.SetNodePosition (smooth, 900.0f, 900.0f));
	EXPECT_EQ (Signature (graph, smooth), base);

	float x = 0.0f;
	float y = 0.0f;
	ASSERT_TRUE (graph.GetNodePosition (load, x, y));
	EXPECT_FLOAT_EQ (x, 120.0f);
	EXPECT_FLOAT_EQ (y, -40.0f);
	EXPECT_FALSE (graph.SetNodePosition (999, 0.0f, 0.0f));
}

TEST (TEST_cggraph_signature, the_node_type_and_the_read_port_are_part_of_it)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeImmutableDesc ("test.Shape"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId source = AddStub (graph, "test.Split", 0, 2, type);
	const NodeId left = AddStub (graph, "test.Sink", 1, 1, type);
	const NodeId right = AddStub (graph, "test.Sink", 1, 1, type);
	const NodeId other = AddStub (graph, "test.OtherSink", 1, 1, type);

	ASSERT_EQ (graph.Connect (source, 0, left, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (source, 1, right, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (source, 0, other, 0), ConnectStatus::Ok);

	// Meme type de noeud, memes parametres, meme amont : seul le port lu differe.
	EXPECT_NE (Signature (graph, left), Signature (graph, right));
	// Meme port lu, meme amont : seul le type de noeud differe.
	EXPECT_NE (Signature (graph, left), Signature (graph, other));
}

TEST (TEST_cggraph_signature, an_unconnected_input_is_not_a_connected_one)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeImmutableDesc ("test.Shape"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId source = AddStub (graph, "test.LoadShape", 0, 1, type);
	const NodeId sink = AddStub (graph, "test.Smooth", 1, 1, type);

	const Hash idle = Signature (graph, sink);
	ASSERT_EQ (graph.Connect (source, 0, sink, 0), ConnectStatus::Ok);
	EXPECT_NE (Signature (graph, sink), idle);

	EXPECT_TRUE (graph.Disconnect (sink, 0));
	EXPECT_EQ (Signature (graph, sink), idle);
}

TEST (TEST_cggraph_signature, an_unknown_node_has_no_signature)
{
	Graph graph;
	EXPECT_EQ (Signature (graph, 999), kNoSignature);
	EXPECT_EQ (Signature (graph, kInvalidNodeId), kNoSignature);
}
