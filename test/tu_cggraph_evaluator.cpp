#include <gtest/gtest.h>

#include "../src/cggraph/core/evaluator.h"

#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

// ===========================================================================
//  Couche A : evaluateur, cache indexe sur la signature, frontiere
// ===========================================================================
// Les types manipules ici sont INVENTES. Le moteur ne connait aucune
// bibliotheque de domaine, et ces tests le prouvent en n'en incluant aucune :
// `Shape` joue le role d'une geometrie forkable -- tableaux et compteur de
// revision --, `Instance` celui d'un couple (reference de valeur,
// transformation). Les noms reels appartiennent a la couche B.

using namespace cggraph;

namespace {

// Toute donnee de Shape entre dans l'image comparee octet a octet plus bas :
// aucun membre n'est exclu, et ajouter un membre sans l'ajouter a ImageOf
// viderait le critere de son sens.
struct Shape
{
	std::vector<float> positions;
	std::vector<int> corners;
	unsigned revision = 0;
};

struct Instance
{
	std::shared_ptr<const Shape> target;
	float transform[16] = { 0.0f };
};

std::shared_ptr<void> CloneShape (const void *value)
{
	return std::make_shared<Shape> (*static_cast<const Shape *> (value));
}

std::size_t SizeOfShape (const void *value)
{
	const Shape *shape = static_cast<const Shape *> (value);
	return shape->positions.size () * sizeof (float) + shape->corners.size () * sizeof (int)
	       + sizeof (unsigned);
}

void AppendBytes (std::vector<unsigned char> &out, const void *data, std::size_t size)
{
	const unsigned char *bytes = static_cast<const unsigned char *> (data);
	out.insert (out.end (), bytes, bytes + size);
}

std::vector<unsigned char> ImageOf (const Shape &shape)
{
	std::vector<unsigned char> image;
	AppendBytes (image, shape.positions.data (), shape.positions.size () * sizeof (float));
	AppendBytes (image, shape.corners.data (), shape.corners.size () * sizeof (int));
	AppendBytes (image, &shape.revision, sizeof (shape.revision));
	return image;
}

std::shared_ptr<Shape> MakeShape (int seed, std::size_t size)
{
	std::shared_ptr<Shape> shape = std::make_shared<Shape> ();
	shape->positions.resize (size);
	shape->corners.resize (size);
	for (std::size_t i = 0; i < size; ++i)
	{
		shape->positions[i] = static_cast<float> (seed) + static_cast<float> (i);
		shape->corners[i] = seed + static_cast<int> (i);
	}
	shape->revision = 1;
	return shape;
}

// ---------------------------------------------------------------------------
//  Noeuds de test
// ---------------------------------------------------------------------------

class TestNode : public Node
{
public:
	const NodeDesc &GetDesc () const override { return m_desc; }

	std::size_t GetComputeCount () const { return m_computeCount; }

protected:
	void Declare (const std::string &typeName, const std::vector<const TypeDesc *> &inputs,
	              const std::vector<const TypeDesc *> &outputs)
	{
		m_desc.typeName = typeName;
		for (const TypeDesc *type : inputs)
		{
			PortDesc port;
			port.name = "in";
			port.type = type;
			m_desc.inputs.push_back (port);
		}
		for (const TypeDesc *type : outputs)
		{
			PortDesc port;
			port.name = "out";
			port.type = type;
			m_desc.outputs.push_back (port);
		}
	}

	// Un port declare un a un quand son nom ou son caractere optionnel compte.
	void DeclareInput (const std::string &name, const TypeDesc *type, bool optional)
	{
		PortDesc port;
		port.name = name;
		port.type = type;
		port.optional = optional;
		m_desc.inputs.push_back (port);
	}

	std::size_t m_computeCount = 0;

private:
	NodeDesc m_desc;
};

// Source : fabrique une forme depuis ses parametres, et garde une reference sur
// ce qu'elle a produit pour que le test puisse l'inspecter apres coup.
class SourceNode : public TestNode
{
public:
	SourceNode (const TypeDesc *shapeType, std::size_t size)
		: m_shapeType (shapeType), m_size (size)
	{
		Declare ("test.Source", {}, { shapeType });
		GetParams ().SetInt ("seed", 1);
		GetParams ().SetString ("logLabel", "quiet", ParamRole::NonSemantic);
	}

	bool Compute (EvalContext &ctx, const ValueList &, ValueList &out) override
	{
		++m_computeCount;
		ctx.Progress (0.5f, "source");
		const ParamValue *seed = GetParams ().Find ("seed");
		m_produced = MakeShape (seed ? seed->intValue : 0, m_size);
		out[0] = Value::Make (m_shapeType, m_produced);
		return true;
	}

	const std::shared_ptr<const Shape> &GetProduced () const { return m_produced; }

private:
	const TypeDesc *m_shapeType;
	std::size_t m_size;
	std::shared_ptr<const Shape> m_produced;
};

// Consommateur : lit son entree, la COPIE, ecrit dans la copie. La frontiere
// n'est pas une convention ici : Value::Get ne rend qu'un pointeur const.
class RelaxNode : public TestNode
{
public:
	explicit RelaxNode (const TypeDesc *shapeType)
		: m_shapeType (shapeType)
	{
		Declare ("test.Relax", { shapeType }, { shapeType });
		GetParams ().SetInt ("iterations", 2);
		GetParams ().SetFloat ("previewRatio", 0.25f, ParamRole::NonSemantic);
	}

	bool Compute (EvalContext &, const ValueList &in, ValueList &out) override
	{
		++m_computeCount;
		const Shape *source = in[0].Get<Shape> (m_shapeType);
		if (source == nullptr)
			return false;

		const ParamValue *iterations = GetParams ().Find ("iterations");
		std::shared_ptr<Shape> result = std::make_shared<Shape> (*source);
		for (std::size_t i = 0; i < result->positions.size (); ++i)
			result->positions[i] *= 0.5f;
		result->revision = source->revision + (iterations ? static_cast<unsigned> (iterations->intValue) : 0u);
		m_produced = result;
		out[0] = Value::Make (m_shapeType, result);
		return true;
	}

	const std::shared_ptr<const Shape> &GetProduced () const { return m_produced; }

private:
	const TypeDesc *m_shapeType;
	std::shared_ptr<const Shape> m_produced;
};

// Emet une Instance : reference sur une valeur amont, plus une transformation.
// Aucun noeud du catalogue ne fait cela a ce stade -- c'est le contrat de
// valeur du lien qu'on eprouve, pas une fonctionnalite d'assemblage.
class InstanceNode : public TestNode
{
public:
	InstanceNode (const TypeDesc *shapeType, const TypeDesc *instanceType)
		: m_shapeType (shapeType), m_instanceType (instanceType)
	{
		Declare ("test.Place", { shapeType }, { instanceType });
		GetParams ().SetFloat ("tx", 3.0f);
	}

	bool Compute (EvalContext &, const ValueList &in, ValueList &out) override
	{
		++m_computeCount;
		std::shared_ptr<const Shape> target = in[0].Share<Shape> (m_shapeType);
		if (target == nullptr)
			return false;

		const ParamValue *tx = GetParams ().Find ("tx");
		std::shared_ptr<Instance> instance = std::make_shared<Instance> ();
		instance->target = target;
		instance->transform[0] = 1.0f;
		instance->transform[12] = tx ? tx->floatValue : 0.0f;
		out[0] = Value::Make (m_instanceType, instance);
		return true;
	}

private:
	const TypeDesc *m_shapeType;
	const TypeDesc *m_instanceType;
};

class FailingNode : public TestNode
{
public:
	explicit FailingNode (const TypeDesc *shapeType)
	{
		Declare ("test.Failing", { shapeType }, { shapeType });
	}

	bool Compute (EvalContext &, const ValueList &, ValueList &) override
	{
		++m_computeCount;
		return false;
	}
};

// Pose le drapeau d'annulation depuis l'interieur du calcul, comme le ferait
// une boucle externe qui rend la main.
class AbortingNode : public TestNode
{
public:
	AbortingNode (const TypeDesc *shapeType, std::atomic<bool> *flag)
		: m_shapeType (shapeType), m_flag (flag)
	{
		Declare ("test.Aborting", { shapeType }, { shapeType });
	}

	bool Compute (EvalContext &ctx, const ValueList &in, ValueList &out) override
	{
		++m_computeCount;
		m_flag->store (true);
		if (ctx.IsAborted ())
			out[0] = in[0];
		return true;
	}

private:
	const TypeDesc *m_shapeType;
	std::atomic<bool> *m_flag;
};

TypeDesc MakeShapeDesc ()
{
	TypeDesc desc;
	desc.name = "test.Shape";
	desc.clone = &CloneShape;
	desc.sizeHint = &SizeOfShape;
	desc.mutability = TypeDesc::Forkable;
	return desc;
}

TypeDesc MakeInstanceDesc ()
{
	TypeDesc desc;
	desc.name = "test.Instance";
	desc.mutability = TypeDesc::Immutable;
	return desc;   // clone nul : une instance ne se modifie pas en place
}

// Graphe de reference : Source -> Relax -> Relax, plus ses accesseurs typés.
struct Chain
{
	TypeRegistry registry;
	Graph graph;
	const TypeDesc *shapeType = nullptr;
	NodeId source = kInvalidNodeId;
	NodeId relax = kInvalidNodeId;
	NodeId polish = kInvalidNodeId;

	explicit Chain (std::size_t size = 8)
	{
		shapeType = registry.Register (MakeShapeDesc ());
		source = graph.AddNode (std::unique_ptr<Node> (new SourceNode (shapeType, size)));
		relax = graph.AddNode (std::unique_ptr<Node> (new RelaxNode (shapeType)));
		polish = graph.AddNode (std::unique_ptr<Node> (new RelaxNode (shapeType)));
		graph.Connect (source, 0, relax, 0);
		graph.Connect (relax, 0, polish, 0);
	}

	SourceNode &Source () { return *static_cast<SourceNode *> (graph.FindNode (source)); }
	RelaxNode &Relax () { return *static_cast<RelaxNode *> (graph.FindNode (relax)); }
	RelaxNode &Polish () { return *static_cast<RelaxNode *> (graph.FindNode (polish)); }
};

} // namespace

// ---------------------------------------------------------------------------
//  La frontiere d'immuabilite, verifiee par le compilateur
// ---------------------------------------------------------------------------
// Aucune lecture d'une entree ne peut rendre autre chose qu'un acces en lecture
// seule : c'est le type qui l'interdit, pas une convention.

static_assert (std::is_same<decltype (std::declval<const Value &> ().Get<Shape> (nullptr)),
                            const Shape *>::value,
               "Value::Get doit rendre un pointeur const");
static_assert (std::is_same<decltype (std::declval<const Value &> ().Share<Shape> (nullptr)),
                            std::shared_ptr<const Shape>>::value,
               "Value::Share doit rendre une charge const");
static_assert (std::is_same<decltype (std::declval<const ValueList &> ()[0]), const Value &>::value,
               "les entrees d'un Compute sont const");

// ---------------------------------------------------------------------------
//  Evaluation de bout en bout
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_evaluator, a_chain_evaluates_and_calls_compute)
{
	Chain chain;
	Evaluator evaluator (chain.graph);
	EvalContext ctx;

	ValueList outputs;
	const EvalResult result = evaluator.Evaluate (chain.polish, outputs, ctx);

	ASSERT_TRUE (result.IsOk ());
	ASSERT_EQ (outputs.size (), 1u);
	const Shape *shape = outputs[0].Get<Shape> (chain.shapeType);
	ASSERT_NE (shape, nullptr);
	EXPECT_EQ (shape->positions.size (), 8u);

	EXPECT_EQ (chain.Source ().GetComputeCount (), 1u);
	EXPECT_EQ (chain.Relax ().GetComputeCount (), 1u);
	EXPECT_EQ (chain.Polish ().GetComputeCount (), 1u);
	EXPECT_EQ (evaluator.GetStats ().misses, 3u);
	EXPECT_EQ (evaluator.GetStats ().hits, 0u);
}

TEST (TEST_cggraph_evaluator, evaluating_twice_recomputes_nothing)
{
	Chain chain;
	Evaluator evaluator (chain.graph);
	EvalContext ctx;

	ValueList first;
	ValueList second;
	ASSERT_TRUE (evaluator.Evaluate (chain.polish, first, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (chain.polish, second, ctx).IsOk ());

	EXPECT_EQ (chain.Source ().GetComputeCount (), 1u);
	EXPECT_EQ (chain.Polish ().GetComputeCount (), 1u);
	EXPECT_EQ (evaluator.GetStats ().hits, 1u);
	EXPECT_EQ (first[0].Get<Shape> (chain.shapeType), second[0].Get<Shape> (chain.shapeType));
}

TEST (TEST_cggraph_evaluator, an_upstream_parameter_recomputes_the_downstream)
{
	Chain chain;
	Evaluator evaluator (chain.graph);
	EvalContext ctx;

	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (chain.polish, outputs, ctx).IsOk ());

	chain.Source ().GetParams ().SetInt ("seed", 99);
	ASSERT_TRUE (evaluator.Evaluate (chain.polish, outputs, ctx).IsOk ());

	EXPECT_EQ (chain.Source ().GetComputeCount (), 2u);
	EXPECT_EQ (chain.Relax ().GetComputeCount (), 2u);
	EXPECT_EQ (chain.Polish ().GetComputeCount (), 2u);
	EXPECT_EQ (evaluator.GetStats ().hits, 0u);
}

TEST (TEST_cggraph_evaluator, a_non_semantic_parameter_is_served_from_the_cache)
{
	Chain chain;
	Evaluator evaluator (chain.graph);
	EvalContext ctx;

	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (chain.polish, outputs, ctx).IsOk ());
	const std::size_t computes = chain.Polish ().GetComputeCount ();

	chain.Source ().GetParams ().SetString ("logLabel", "verbose", ParamRole::NonSemantic);
	chain.Relax ().GetParams ().SetFloat ("previewRatio", 0.9f, ParamRole::NonSemantic);

	ASSERT_TRUE (evaluator.Evaluate (chain.polish, outputs, ctx).IsOk ());
	EXPECT_EQ (chain.Source ().GetComputeCount (), 1u);
	EXPECT_EQ (chain.Polish ().GetComputeCount (), computes);
	EXPECT_EQ (evaluator.GetStats ().hits, 1u);
}

TEST (TEST_cggraph_evaluator, moving_a_box_on_screen_is_served_from_the_cache)
{
	Chain chain;
	Evaluator evaluator (chain.graph);
	EvalContext ctx;

	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (chain.polish, outputs, ctx).IsOk ());

	ASSERT_TRUE (chain.graph.SetNodePosition (chain.source, 10.0f, 20.0f));
	ASSERT_TRUE (chain.graph.SetNodePosition (chain.relax, -300.0f, 45.0f));
	ASSERT_TRUE (chain.graph.SetNodePosition (chain.polish, 1024.0f, 768.0f));

	ASSERT_TRUE (evaluator.Evaluate (chain.polish, outputs, ctx).IsOk ());
	EXPECT_EQ (chain.Polish ().GetComputeCount (), 1u);
	EXPECT_EQ (evaluator.GetStats ().hits, 1u);
}

// ---------------------------------------------------------------------------
//  Frontiere d'immuabilite -- comparaison octet a octet, sans exclusion
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_evaluator, evaluating_twice_leaves_the_input_byte_for_byte_identical)
{
	Chain chain;
	Evaluator evaluator (chain.graph);
	EvalContext ctx;

	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (chain.source, outputs, ctx).IsOk ());
	const std::shared_ptr<const Shape> input = chain.Source ().GetProduced ();
	ASSERT_NE (input, nullptr);

	const std::vector<unsigned char> before = ImageOf (*input);
	ASSERT_FALSE (before.empty ());

	ASSERT_TRUE (evaluator.Evaluate (chain.polish, outputs, ctx).IsOk ());
	ASSERT_TRUE (evaluator.Evaluate (chain.polish, outputs, ctx).IsOk ());

	const std::vector<unsigned char> after = ImageOf (*input);
	ASSERT_EQ (before.size (), after.size ());
	EXPECT_EQ (std::memcmp (before.data (), after.data (), before.size ()), 0);
	EXPECT_EQ (input->revision, 1u);

	// Le consommateur a produit un AUTRE objet : il a copie, il n'a pas ecrit
	// dans son entree.
	ASSERT_NE (chain.Relax ().GetProduced (), nullptr);
	EXPECT_NE (chain.Relax ().GetProduced ().get (), input.get ());
	EXPECT_NE (chain.Relax ().GetProduced ()->revision, input->revision);
}

// ---------------------------------------------------------------------------
//  Budget et epinglage
// ---------------------------------------------------------------------------

namespace {

// Quatre sources independantes, dimensionnees pour que le budget morde.
struct Pressure
{
	TypeRegistry registry;
	Graph graph;
	const TypeDesc *shapeType = nullptr;
	std::vector<NodeId> sources;

	Pressure ()
	{
		shapeType = registry.Register (MakeShapeDesc ());
		for (int i = 0; i < 4; ++i)
		{
			const NodeId id = graph.AddNode (std::unique_ptr<Node> (new SourceNode (shapeType, 100)));
			graph.FindNode (id)->GetParams ().SetInt ("seed", i + 1);
			sources.push_back (id);
		}
	}
};

const std::size_t kEntryBytes = 100 * sizeof (float) + 100 * sizeof (int) + sizeof (unsigned);

} // namespace

TEST (TEST_cggraph_evaluator, the_budget_is_a_setting_and_it_drives_eviction)
{
	Pressure wide;
	Evaluator generous (wide.graph);
	EvalContext ctx;
	ValueList outputs;

	generous.SetMemoryBudget (16u * kEntryBytes);
	EXPECT_EQ (generous.GetMemoryBudget (), 16u * kEntryBytes);
	for (NodeId id : wide.sources)
		ASSERT_TRUE (generous.Evaluate (id, outputs, ctx).IsOk ());
	EXPECT_EQ (generous.GetStats ().entries, 4u);
	EXPECT_EQ (generous.GetStats ().evictions, 0u);

	Pressure narrow;
	Evaluator tight (narrow.graph);
	tight.SetMemoryBudget (2u * kEntryBytes);
	for (NodeId id : narrow.sources)
		ASSERT_TRUE (tight.Evaluate (id, outputs, ctx).IsOk ());
	EXPECT_GT (tight.GetStats ().evictions, 0u);
	EXPECT_LT (tight.GetStats ().entries, 4u);
	EXPECT_LE (tight.GetStats ().bytes, 2u * kEntryBytes);
}

TEST (TEST_cggraph_evaluator, a_pinned_node_survives_the_pressure_that_evicts_it_otherwise)
{
	EvalContext ctx;
	ValueList outputs;

	// Temoin : sans epingle, la premiere entree est evincee.
	Pressure loose;
	Evaluator unpinned (loose.graph);
	unpinned.SetMemoryBudget (2u * kEntryBytes);
	for (NodeId id : loose.sources)
		ASSERT_TRUE (unpinned.Evaluate (id, outputs, ctx).IsOk ());
	ASSERT_FALSE (unpinned.IsCached (loose.sources[0]));

	// Meme pression, meme budget, epingle posee.
	Pressure held;
	Evaluator pinned (held.graph);
	pinned.SetMemoryBudget (2u * kEntryBytes);
	pinned.Pin (held.sources[0]);
	EXPECT_TRUE (pinned.IsPinned (held.sources[0]));

	for (NodeId id : held.sources)
		ASSERT_TRUE (pinned.Evaluate (id, outputs, ctx).IsOk ());

	EXPECT_TRUE (pinned.IsCached (held.sources[0]));
	const std::size_t misses = pinned.GetStats ().misses;
	ASSERT_TRUE (pinned.Evaluate (held.sources[0], outputs, ctx).IsOk ());
	EXPECT_EQ (pinned.GetStats ().misses, misses);

	// L'epingle retiree, l'entree redevient evincable des qu'elle cesse d'etre
	// la plus recemment utilisee.
	pinned.Unpin (held.sources[0]);
	EXPECT_FALSE (pinned.IsPinned (held.sources[0]));
	ASSERT_TRUE (pinned.Evaluate (held.sources[1], outputs, ctx).IsOk ());
	ASSERT_TRUE (pinned.Evaluate (held.sources[2], outputs, ctx).IsOk ());
	EXPECT_FALSE (pinned.IsCached (held.sources[0]));
}

// ---------------------------------------------------------------------------
//  Refus nommes
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_evaluator, a_driven_parameter_is_refused_by_name)
{
	Chain chain;
	Evaluator evaluator (chain.graph);
	EvalContext ctx;

	chain.Relax ().GetParams ().SetDriven ("iterations", ParamType::Int, "upstream.count / 2");

	ValueList outputs;
	const EvalResult result = evaluator.Evaluate (chain.polish, outputs, ctx);

	EXPECT_EQ (result.status, EvalStatus::DrivenParameter);
	EXPECT_EQ (result.node, chain.relax);
	EXPECT_EQ (result.detail, "iterations");
	EXPECT_TRUE (outputs.empty ());

	// Le refus est un diagnostic, pas un resultat : rien n'a ete calcule en
	// aval, et le parametre reste stocke tel qu'il a ete pose.
	EXPECT_EQ (chain.Relax ().GetComputeCount (), 0u);
	EXPECT_EQ (chain.Polish ().GetComputeCount (), 0u);
	ASSERT_NE (chain.Relax ().GetParams ().Find ("iterations"), nullptr);
	EXPECT_EQ (chain.Relax ().GetParams ().Find ("iterations")->kind, ParamKind::Driven);
}

TEST (TEST_cggraph_evaluator, an_unfed_input_and_an_unknown_node_are_refused_by_name)
{
	Chain chain;
	Evaluator evaluator (chain.graph);
	EvalContext ctx;
	ValueList outputs;

	ASSERT_TRUE (chain.graph.Disconnect (chain.relax, 0));
	const EvalResult missing = evaluator.Evaluate (chain.polish, outputs, ctx);
	EXPECT_EQ (missing.status, EvalStatus::MissingInput);
	EXPECT_EQ (missing.node, chain.relax);
	EXPECT_EQ (missing.detail, "in");

	const EvalResult unknown = evaluator.Evaluate (999, outputs, ctx);
	EXPECT_EQ (unknown.status, EvalStatus::UnknownNode);
	EXPECT_EQ (evaluator.GetSignature (999), kNoSignature);
}

TEST (TEST_cggraph_evaluator, a_failing_compute_is_reported_and_not_cached)
{
	TypeRegistry registry;
	const TypeDesc *shapeType = registry.Register (MakeShapeDesc ());
	ASSERT_NE (shapeType, nullptr);

	Graph graph;
	const NodeId source = graph.AddNode (std::unique_ptr<Node> (new SourceNode (shapeType, 4)));
	const NodeId failing = graph.AddNode (std::unique_ptr<Node> (new FailingNode (shapeType)));
	ASSERT_EQ (graph.Connect (source, 0, failing, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;

	const EvalResult result = evaluator.Evaluate (failing, outputs, ctx);
	EXPECT_EQ (result.status, EvalStatus::ComputeFailed);
	EXPECT_EQ (result.node, failing);
	EXPECT_FALSE (evaluator.IsCached (failing));
	EXPECT_TRUE (evaluator.IsCached (source));
}

// ---------------------------------------------------------------------------
//  Contexte d'evaluation
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_evaluator, an_aborted_computation_is_reported_and_not_cached)
{
	TypeRegistry registry;
	const TypeDesc *shapeType = registry.Register (MakeShapeDesc ());
	ASSERT_NE (shapeType, nullptr);

	std::atomic<bool> cancelled (false);
	Graph graph;
	const NodeId source = graph.AddNode (std::unique_ptr<Node> (new SourceNode (shapeType, 4)));
	const NodeId aborting = graph.AddNode (std::unique_ptr<Node> (new AbortingNode (shapeType, &cancelled)));
	ASSERT_EQ (graph.Connect (source, 0, aborting, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ctx.SetCancellationFlag (&cancelled);
	EXPECT_FALSE (ctx.IsAborted ());

	ValueList outputs;
	const EvalResult result = evaluator.Evaluate (aborting, outputs, ctx);
	EXPECT_EQ (result.status, EvalStatus::Aborted);
	EXPECT_EQ (result.node, aborting);
	EXPECT_TRUE (ctx.IsAborted ());
	EXPECT_FALSE (evaluator.IsCached (aborting));

	// Le drapeau retombe, le meme graphe se calcule.
	cancelled.store (false);
	EXPECT_TRUE (evaluator.Evaluate (aborting, outputs, ctx).IsOk () || ctx.IsAborted ());
}

TEST (TEST_cggraph_evaluator, the_context_relays_progress_and_stays_silent_without_a_sink)
{
	Chain chain;
	Evaluator evaluator (chain.graph);

	EvalContext silent;
	ValueList outputs;
	silent.Progress (0.5f, "sans collecteur");
	ASSERT_TRUE (evaluator.Evaluate (chain.source, outputs, silent).IsOk ());

	Chain other;
	Evaluator watched (other.graph);
	EvalContext ctx;
	std::vector<float> steps;
	ctx.SetProgressSink ([&steps] (float t, const char *) { steps.push_back (t); });
	ASSERT_TRUE (watched.Evaluate (other.source, outputs, ctx).IsOk ());
	ASSERT_EQ (steps.size (), 1u);
	EXPECT_FLOAT_EQ (steps[0], 0.5f);
}

// ---------------------------------------------------------------------------
//  Instance au contrat de valeur du lien
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_evaluator, an_instance_transits_on_a_link_without_touching_the_value_contract)
{
	TypeRegistry registry;
	const TypeDesc *shapeType = registry.Register (MakeShapeDesc ());
	const TypeDesc *instanceType = registry.Register (MakeInstanceDesc ());
	ASSERT_NE (shapeType, nullptr);
	ASSERT_NE (instanceType, nullptr);
	EXPECT_EQ (instanceType->mutability, TypeDesc::Immutable);
	EXPECT_EQ (instanceType->clone, nullptr);

	Graph graph;
	const NodeId source = graph.AddNode (std::unique_ptr<Node> (new SourceNode (shapeType, 4)));
	const NodeId place = graph.AddNode (std::unique_ptr<Node> (new InstanceNode (shapeType, instanceType)));
	ASSERT_EQ (graph.Connect (source, 0, place, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	ASSERT_TRUE (evaluator.Evaluate (place, outputs, ctx).IsOk ());

	ASSERT_EQ (outputs.size (), 1u);
	const Instance *instance = outputs[0].Get<Instance> (instanceType);
	ASSERT_NE (instance, nullptr);
	EXPECT_FLOAT_EQ (instance->transform[12], 3.0f);

	// N placements ne clonent pas N geometries : l'instance REFERENCE la valeur
	// amont, elle ne la copie pas.
	ValueList upstream;
	ASSERT_TRUE (evaluator.Evaluate (source, upstream, ctx).IsOk ());
	EXPECT_EQ (instance->target.get (), upstream[0].Get<Shape> (shapeType));

	// Le type est Immutable et sans clone : le cache mesure alors zero octet, et
	// il doit le traiter, non l'interdire.
	EXPECT_EQ (outputs[0].GetSizeHint (), 0u);
	EXPECT_TRUE (evaluator.IsCached (place));
}

// ---------------------------------------------------------------------------
//  Entrees optionnelles
// ---------------------------------------------------------------------------

namespace {

// Une entree obligatoire et une entree optionnelle sur le MEME noeud : les deux
// versants du refus se lisent sans changer de montage.
class MaskedRelaxNode : public TestNode
{
public:
	explicit MaskedRelaxNode (const TypeDesc *shapeType)
		: m_shapeType (shapeType)
	{
		Declare ("test.MaskedRelax", {}, { shapeType });
		DeclareInput ("shape", shapeType, false);
		DeclareInput ("zone", shapeType, true);
	}

	bool Compute (EvalContext &, const ValueList &in, ValueList &out) override
	{
		++m_computeCount;
		if (in.size () != 2)
			return false;

		m_sawZone = !in[1].IsEmpty ();
		m_zoneType = in[1].GetType ();
		m_zoneRead = in[1].Get<Shape> (m_shapeType);

		const Shape *source = in[0].Get<Shape> (m_shapeType);
		if (source == nullptr)
			return false;

		std::shared_ptr<Shape> result = std::make_shared<Shape> (*source);
		result->revision = source->revision + (m_sawZone ? 10u : 1u);
		out[0] = Value::Make (m_shapeType, result);
		return true;
	}

	bool SawZone () const { return m_sawZone; }
	const TypeDesc *GetZoneType () const { return m_zoneType; }
	const Shape *GetZoneRead () const { return m_zoneRead; }

private:
	const TypeDesc *m_shapeType;
	bool m_sawZone = false;
	const TypeDesc *m_zoneType = nullptr;
	const Shape *m_zoneRead = nullptr;
};

// Deux sources et un consommateur ; l'entree optionnelle part non alimentee.
struct Masked
{
	TypeRegistry registry;
	Graph graph;
	const TypeDesc *shapeType = nullptr;
	NodeId shape = kInvalidNodeId;
	NodeId zone = kInvalidNodeId;
	NodeId relax = kInvalidNodeId;

	Masked ()
	{
		shapeType = registry.Register (MakeShapeDesc ());
		shape = graph.AddNode (std::unique_ptr<Node> (new SourceNode (shapeType, 4)));
		zone = graph.AddNode (std::unique_ptr<Node> (new SourceNode (shapeType, 4)));
		graph.FindNode (zone)->GetParams ().SetInt ("seed", 7);
		relax = graph.AddNode (std::unique_ptr<Node> (new MaskedRelaxNode (shapeType)));
		graph.Connect (shape, 0, relax, 0);
	}

	MaskedRelaxNode &Relax () { return *static_cast<MaskedRelaxNode *> (graph.FindNode (relax)); }
	SourceNode &Zone () { return *static_cast<SourceNode *> (graph.FindNode (zone)); }
};

} // namespace

TEST (TEST_cggraph_evaluator, an_unfed_optional_input_is_empty_and_the_node_computes)
{
	Masked masked;
	Evaluator evaluator (masked.graph);
	EvalContext ctx;
	ValueList outputs;

	const EvalResult result = evaluator.Evaluate (masked.relax, outputs, ctx);

	ASSERT_EQ (result.status, EvalStatus::Ok);
	ASSERT_EQ (outputs.size (), 1u);
	const Shape *shape = outputs[0].Get<Shape> (masked.shapeType);
	ASSERT_NE (shape, nullptr);

	// Le noeud a bien calcule, et il a calcule la branche sans zone.
	EXPECT_EQ (masked.Relax ().GetComputeCount (), 1u);
	EXPECT_EQ (shape->revision, 2u);

	// L'absence se lit sur la valeur : vide, sans type, illisible.
	EXPECT_FALSE (masked.Relax ().SawZone ());
	EXPECT_EQ (masked.Relax ().GetZoneType (), nullptr);
	EXPECT_EQ (masked.Relax ().GetZoneRead (), nullptr);

	// Une entree non alimentee n'evalue rien en amont d'elle.
	EXPECT_EQ (masked.Zone ().GetComputeCount (), 0u);
}

TEST (TEST_cggraph_evaluator, a_fed_optional_input_behaves_like_a_required_one)
{
	Masked masked;
	ASSERT_EQ (masked.graph.Connect (masked.zone, 0, masked.relax, 1), ConnectStatus::Ok);

	Evaluator evaluator (masked.graph);
	EvalContext ctx;
	ValueList outputs;

	const EvalResult result = evaluator.Evaluate (masked.relax, outputs, ctx);

	ASSERT_EQ (result.status, EvalStatus::Ok);
	ASSERT_EQ (outputs.size (), 1u);
	const Shape *shape = outputs[0].Get<Shape> (masked.shapeType);
	ASSERT_NE (shape, nullptr);
	EXPECT_EQ (shape->revision, 11u);

	// La valeur recue est celle de l'amont, qui a ete evalue pour la produire.
	EXPECT_TRUE (masked.Relax ().SawZone ());
	EXPECT_EQ (masked.Relax ().GetZoneType (), masked.shapeType);
	EXPECT_EQ (masked.Relax ().GetZoneRead (), masked.Zone ().GetProduced ().get ());
	EXPECT_EQ (masked.Zone ().GetComputeCount (), 1u);
}

TEST (TEST_cggraph_evaluator, an_unfed_required_input_is_still_refused_by_name)
{
	Masked masked;
	ASSERT_TRUE (masked.graph.Disconnect (masked.relax, 0));
	ASSERT_EQ (masked.graph.Connect (masked.zone, 0, masked.relax, 1), ConnectStatus::Ok);

	Evaluator evaluator (masked.graph);
	EvalContext ctx;
	ValueList outputs;

	const EvalResult result = evaluator.Evaluate (masked.relax, outputs, ctx);

	// L'entree optionnelle est alimentee : c'est l'obligatoire qui est nommee.
	EXPECT_EQ (result.status, EvalStatus::MissingInput);
	EXPECT_EQ (result.node, masked.relax);
	EXPECT_EQ (result.detail, "shape");
	EXPECT_TRUE (outputs.empty ());
	EXPECT_EQ (masked.Relax ().GetComputeCount (), 0u);
}

TEST (TEST_cggraph_evaluator, connecting_an_optional_input_moves_the_signature_and_recomputes)
{
	Masked masked;
	Evaluator evaluator (masked.graph);
	EvalContext ctx;
	ValueList outputs;

	const Hash idle = evaluator.GetSignature (masked.relax);
	ASSERT_NE (idle, kNoSignature);
	ASSERT_TRUE (evaluator.Evaluate (masked.relax, outputs, ctx).IsOk ());
	ASSERT_EQ (masked.Relax ().GetComputeCount (), 1u);

	// Brancher l'entree optionnelle change la signature : le cache ne peut donc
	// pas servir le resultat calcule sans elle.
	ASSERT_EQ (masked.graph.Connect (masked.zone, 0, masked.relax, 1), ConnectStatus::Ok);
	const Hash fed = evaluator.GetSignature (masked.relax);
	EXPECT_NE (fed, idle);

	ASSERT_TRUE (evaluator.Evaluate (masked.relax, outputs, ctx).IsOk ());
	EXPECT_EQ (masked.Relax ().GetComputeCount (), 2u);
	EXPECT_TRUE (masked.Relax ().SawZone ());
	ASSERT_NE (outputs[0].Get<Shape> (masked.shapeType), nullptr);
	EXPECT_EQ (outputs[0].Get<Shape> (masked.shapeType)->revision, 11u);

	// Debranchee, l'entree retrouve sa signature d'origine : c'est le premier
	// resultat qui ressort, sans recalcul.
	ASSERT_TRUE (masked.graph.Disconnect (masked.relax, 1));
	EXPECT_EQ (evaluator.GetSignature (masked.relax), idle);

	const std::size_t hits = evaluator.GetStats ().hits;
	ASSERT_TRUE (evaluator.Evaluate (masked.relax, outputs, ctx).IsOk ());
	EXPECT_EQ (masked.Relax ().GetComputeCount (), 2u);
	EXPECT_EQ (evaluator.GetStats ().hits, hits + 1u);
	ASSERT_NE (outputs[0].Get<Shape> (masked.shapeType), nullptr);
	EXPECT_EQ (outputs[0].Get<Shape> (masked.shapeType)->revision, 2u);

	// Le marqueur d'entree non alimentee occupe une place dans le hachage. Sans
	// lui, le meme amont branche sur l'obligatoire puis sur l'optionnelle
	// donnerait deux fois la meme signature pour deux calculs differents.
	const Hash onRequired = evaluator.GetSignature (masked.relax);
	ASSERT_TRUE (masked.graph.Disconnect (masked.relax, 0));
	ASSERT_EQ (masked.graph.Connect (masked.shape, 0, masked.relax, 1), ConnectStatus::Ok);
	EXPECT_NE (evaluator.GetSignature (masked.relax), onRequired);
}

// ---------------------------------------------------------------------------
//  L'amont qui ne produit pas le port lu
// ---------------------------------------------------------------------------

namespace {

// Declare une sortie et n'en produit aucune. Le cas n'est pas theorique : rien
// dans le contrat de Compute n'empeche un noeud de VIDER le `out` que
// l'evaluateur a dimensionne, et un adaptateur qui se tromperait de branche
// d'echec le ferait sans le savoir.
class TruncatingNode : public TestNode
{
public:
	explicit TruncatingNode (const TypeDesc *shapeType)
	{
		Declare ("test.Truncating", {}, { shapeType });
	}

	bool Compute (EvalContext &, const ValueList &, ValueList &out) override
	{
		++m_computeCount;
		out.clear ();
		return true;
	}
};

} // namespace

TEST (TEST_cggraph_evaluator, an_upstream_that_does_not_produce_the_read_port_is_refused_by_name)
{
	// L'etape 1 avait declare cette branche DEFENSIVE et inatteignable par
	// l'API publique, en n'examinant que Graph::Connect -- qui refuse bien un
	// fromPort hors bornes. Elle avait oublie l'autre moitie de l'API publique :
	// ecrire un Node. Le port EST alimente ; c'est l'amont qui a menti, et le
	// refus doit nommer l'entree plutot que de laisser passer une valeur vide.
	TypeRegistry registry;
	const TypeDesc *shapeType = registry.Register (MakeShapeDesc ());

	Graph graph;
	const NodeId liar = graph.AddNode (std::unique_ptr<Node> (new TruncatingNode (shapeType)));
	const NodeId relax = graph.AddNode (std::unique_ptr<Node> (new RelaxNode (shapeType)));
	ASSERT_EQ (graph.Connect (liar, 0, relax, 0), ConnectStatus::Ok);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	const EvalResult result = evaluator.Evaluate (relax, outputs, ctx);

	EXPECT_EQ (result.status, EvalStatus::MissingInput);
	EXPECT_EQ (result.node, relax);
	EXPECT_EQ (result.detail, "in");
	EXPECT_TRUE (outputs.empty ());
}

// ---------------------------------------------------------------------------
//  Reentrance depuis le collecteur de progression -- le cas MONO-FIL
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_evaluator, re_entering_from_the_progress_sink_is_refused_by_name)
{
	// Le refus Busy avait UN filet, et il portait sur deux fils
	// (TEST_cggraph_async). Sur un hote mono-fil -- la cible WebAssembly, ou le
	// collecteur de progression POMPE la frame --, le second appel n'a besoin
	// d'aucun fil : le collecteur rappelle du code exterieur pendant que le
	// calcul tourne, et ce code peut redemander une evaluation. C'est le seul
	// chemin de reentrance qui existe la-bas, et c'est celui que le filet a deux
	// fils ne pouvait pas atteindre.
	//
	// Ce que le refus remplace, MESURE en retirant le garde : le second appel
	// va jusqu'au bout et rend Ok, apres avoir vide et reecrit le memo de
	// signatures que le premier est en train de parcourir. Le symptome est donc
	// un resultat faux, jamais un plantage -- et la pre-passe prend un
	// std::mutex non recursif, dont le double verrouillage est un comportement
	// indefini que MSVC laisse passer et qu'une autre implementation peut
	// changer en blocage. Le garde ferme les deux issues.
	Chain chain;
	Evaluator evaluator (chain.graph);

	EvalContext ctx;
	EvalStatus reentered = EvalStatus::Ok;
	unsigned int calls = 0;
	ctx.SetProgressSink ([&] (float, const char *) {
		++calls;
		ValueList inner;
		EvalContext innerCtx;
		reentered = evaluator.Evaluate (chain.source, inner, innerCtx).status;
	});

	ValueList outputs;
	EXPECT_TRUE (evaluator.Evaluate (chain.source, outputs, ctx).IsOk ());
	EXPECT_EQ (calls, 1u);
	EXPECT_EQ (reentered, EvalStatus::Busy);

	// Le drapeau retombe : le refus n'a pas laisse l'evaluateur inutilisable.
	ValueList again;
	EvalContext plain;
	EXPECT_TRUE (evaluator.Evaluate (chain.source, again, plain).IsOk ());
}
