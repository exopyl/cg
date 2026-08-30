#include <gtest/gtest.h>

#include "../src/cggraph/core/graph.h"

#include <memory>
#include <string>
#include <vector>

// ===========================================================================
//  Couche A : registre de types, valeur du lien, validation a la connexion
// ===========================================================================
// Le graphe se teste SEUL, sur des types inventes ici : aucune fixture de
// domaine n'est necessaire, et c'est la propriete que ces tests protegent.
//
// Deux descripteurs partagent la meme charge utile sous des noms differents :
// c'est ce qui permet de verifier que l'incompatibilite se lit sur le TYPE et
// non sur la representation.

using namespace cggraph;

namespace {

struct Payload
{
	int tag = 0;
};

std::shared_ptr<void> ClonePayload (const void *value)
{
	return std::make_shared<Payload> (*static_cast<const Payload *> (value));
}

std::size_t SizeOfPayload (const void *)
{
	return sizeof (Payload);
}

TypeDesc MakeImmutableDesc (const std::string &name)
{
	TypeDesc desc;
	desc.name = name;
	desc.mutability = TypeDesc::Immutable;
	return desc;   // clone reste nul : etat nominal d'un type immuable
}

TypeDesc MakeForkableDesc (const std::string &name)
{
	TypeDesc desc;
	desc.name = name;
	desc.clone = &ClonePayload;
	desc.sizeHint = &SizeOfPayload;
	desc.mutability = TypeDesc::Forkable;
	return desc;
}

class StubNode : public Node
{
public:
	StubNode (const std::string &typeName,
	          const std::vector<const TypeDesc *> &inputs,
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

	const NodeDesc &GetDesc () const override { return m_desc; }

	// L'evaluateur n'existe pas a ce stade : aucun test n'appelle Compute.
	bool Compute (EvalContext &, const ValueList &, ValueList &) override { return false; }

private:
	NodeDesc m_desc;
};

NodeId AddStub (Graph &graph,
                const std::string &typeName,
                const std::vector<const TypeDesc *> &inputs,
                const std::vector<const TypeDesc *> &outputs)
{
	return graph.AddNode (std::unique_ptr<Node> (new StubNode (typeName, inputs, outputs)));
}

} // namespace

// ---------------------------------------------------------------------------
//  Registre de types
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_type_registry, a_registered_type_is_found_by_its_name)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeForkableDesc ("test.Forkable"));
	ASSERT_NE (type, nullptr);
	EXPECT_EQ (registry.Find ("test.Forkable"), type);
	EXPECT_EQ (registry.Find ("test.Absent"), nullptr);
	EXPECT_EQ (registry.GetCount (), 1u);
}

TEST (TEST_cggraph_type_registry, a_duplicate_name_is_refused)
{
	TypeRegistry registry;
	ASSERT_NE (registry.Register (MakeImmutableDesc ("test.Font")), nullptr);
	EXPECT_EQ (registry.Register (MakeImmutableDesc ("test.Font")), nullptr);
	EXPECT_EQ (registry.GetCount (), 1u);
}

TEST (TEST_cggraph_type_registry, an_empty_name_is_refused)
{
	TypeRegistry registry;
	EXPECT_EQ (registry.Register (MakeImmutableDesc ("")), nullptr);
	EXPECT_EQ (registry.GetCount (), 0u);
}

TEST (TEST_cggraph_type_registry, an_immutable_type_without_clone_is_accepted)
{
	TypeRegistry registry;
	const TypeDesc *font = registry.Register (MakeImmutableDesc ("test.Font"));
	ASSERT_NE (font, nullptr);
	EXPECT_EQ (font->clone, nullptr);
	EXPECT_EQ (font->mutability, TypeDesc::Immutable);
}

TEST (TEST_cggraph_type_registry, a_forkable_type_without_clone_is_refused)
{
	TypeRegistry registry;
	TypeDesc desc = MakeForkableDesc ("test.Forkable");
	desc.clone = nullptr;
	EXPECT_EQ (registry.Register (desc), nullptr);
	EXPECT_EQ (registry.GetCount (), 0u);
}

TEST (TEST_cggraph_type_registry, descriptors_keep_their_address_when_the_registry_grows)
{
	TypeRegistry registry;
	const TypeDesc *first = registry.Register (MakeImmutableDesc ("test.First"));
	ASSERT_NE (first, nullptr);
	for (int i = 0; i < 512; ++i)
		ASSERT_NE (registry.Register (MakeImmutableDesc ("test.Filler" + std::to_string (i))), nullptr);
	EXPECT_EQ (registry.Find ("test.First"), first);
	EXPECT_EQ (first->name, "test.First");
}

// ---------------------------------------------------------------------------
//  Valeur du lien
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_value, a_default_value_is_empty)
{
	Value value;
	EXPECT_TRUE (value.IsEmpty ());
	EXPECT_EQ (value.GetType (), nullptr);
	EXPECT_EQ (value.GetSizeHint (), 0u);
}

TEST (TEST_cggraph_value, a_value_reads_back_under_its_own_type)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeForkableDesc ("test.Forkable"));
	ASSERT_NE (type, nullptr);

	Payload payload;
	payload.tag = 42;
	// Charge non const a la construction : elle devient lisible seule.
	const Value value = Value::Make (type, std::make_shared<Payload> (payload));

	ASSERT_FALSE (value.IsEmpty ());
	const Payload *read = value.Get<Payload> (type);
	ASSERT_NE (read, nullptr);
	EXPECT_EQ (read->tag, 42);
	EXPECT_EQ (value.GetSizeHint (), sizeof (Payload));
}

TEST (TEST_cggraph_value, a_value_read_under_another_type_yields_nothing)
{
	TypeRegistry registry;
	const TypeDesc *typeA = registry.Register (MakeImmutableDesc ("test.A"));
	const TypeDesc *typeB = registry.Register (MakeImmutableDesc ("test.B"));
	ASSERT_NE (typeA, nullptr);
	ASSERT_NE (typeB, nullptr);

	const Value value = Value::Make (typeA, std::make_shared<const Payload> (Payload ()));
	EXPECT_EQ (value.Get<Payload> (typeB), nullptr);
	EXPECT_EQ (value.Share<Payload> (typeB), nullptr);
	EXPECT_NE (value.Share<Payload> (typeA), nullptr);
}

TEST (TEST_cggraph_value, a_type_that_cannot_measure_itself_hints_zero)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeImmutableDesc ("test.Font"));
	ASSERT_NE (type, nullptr);

	const Value value = Value::Make (type, std::make_shared<const Payload> (Payload ()));
	ASSERT_FALSE (value.IsEmpty ());
	EXPECT_EQ (value.GetSizeHint (), 0u);
}

TEST (TEST_cggraph_value, an_immutable_value_without_clone_transits_on_a_link)
{
	TypeRegistry registry;
	const TypeDesc *font = registry.Register (MakeImmutableDesc ("test.Font"));
	ASSERT_NE (font, nullptr);
	ASSERT_EQ (font->clone, nullptr);

	Graph graph;
	const NodeId source = AddStub (graph, "test.LoadFont", {}, { font });
	const NodeId sink = AddStub (graph, "test.UseFont", { font }, {});
	ASSERT_EQ (graph.Connect (source, 0, sink, 0), ConnectStatus::Ok);

	ValueList produced (1);
	Payload payload;
	payload.tag = 7;
	produced[0] = Value::Make (font, std::make_shared<const Payload> (payload));

	const Link *link = graph.FindInputLink (sink, 0);
	ASSERT_NE (link, nullptr);
	ValueList consumed (1);
	consumed[link->toPort] = produced[link->fromPort];

	const Payload *read = consumed[0].Get<Payload> (font);
	ASSERT_NE (read, nullptr);
	EXPECT_EQ (read->tag, 7);
	EXPECT_EQ (consumed[0].GetType ()->clone, nullptr);
}

// ---------------------------------------------------------------------------
//  Validation a la connexion
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_graph, a_link_between_matching_ports_is_accepted)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeForkableDesc ("test.Forkable"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId source = AddStub (graph, "test.Source", {}, { type });
	const NodeId sink = AddStub (graph, "test.Sink", { type }, {});
	ASSERT_NE (source, kInvalidNodeId);
	ASSERT_NE (sink, kInvalidNodeId);
	EXPECT_EQ (graph.GetNodeCount (), 2u);

	EXPECT_EQ (graph.Connect (source, 0, sink, 0), ConnectStatus::Ok);
	EXPECT_EQ (graph.GetLinks ().size (), 1u);
	EXPECT_NE (graph.FindInputLink (sink, 0), nullptr);
}

TEST (TEST_cggraph_graph, a_cycle_is_refused)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeForkableDesc ("test.Forkable"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId a = AddStub (graph, "test.A", { type }, { type });
	const NodeId b = AddStub (graph, "test.B", { type }, { type });
	const NodeId c = AddStub (graph, "test.C", { type }, { type });

	ASSERT_EQ (graph.Connect (a, 0, b, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (b, 0, c, 0), ConnectStatus::Ok);

	EXPECT_EQ (graph.Connect (c, 0, a, 0), ConnectStatus::Cycle);
	EXPECT_EQ (graph.GetLinks ().size (), 2u);
}

TEST (TEST_cggraph_graph, a_node_cannot_feed_itself)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeForkableDesc ("test.Forkable"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId a = AddStub (graph, "test.A", { type }, { type });
	EXPECT_EQ (graph.Connect (a, 0, a, 0), ConnectStatus::Cycle);
	EXPECT_TRUE (graph.GetLinks ().empty ());
}

TEST (TEST_cggraph_graph, incompatible_types_are_refused)
{
	TypeRegistry registry;
	const TypeDesc *typeA = registry.Register (MakeImmutableDesc ("test.A"));
	const TypeDesc *typeB = registry.Register (MakeImmutableDesc ("test.B"));
	ASSERT_NE (typeA, nullptr);
	ASSERT_NE (typeB, nullptr);

	Graph graph;
	const NodeId source = AddStub (graph, "test.Source", {}, { typeA });
	const NodeId sink = AddStub (graph, "test.Sink", { typeB }, {});

	EXPECT_EQ (graph.Connect (source, 0, sink, 0), ConnectStatus::TypeMismatch);
	EXPECT_TRUE (graph.GetLinks ().empty ());
}

TEST (TEST_cggraph_graph, a_second_source_on_a_fed_input_is_refused)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeForkableDesc ("test.Forkable"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId first = AddStub (graph, "test.First", {}, { type });
	const NodeId second = AddStub (graph, "test.Second", {}, { type });
	const NodeId sink = AddStub (graph, "test.Sink", { type }, {});

	ASSERT_EQ (graph.Connect (first, 0, sink, 0), ConnectStatus::Ok);
	EXPECT_EQ (graph.Connect (second, 0, sink, 0), ConnectStatus::InputAlreadyConnected);
	EXPECT_EQ (graph.GetLinks ().size (), 1u);
	EXPECT_EQ (graph.FindInputLink (sink, 0)->from, first);
}

TEST (TEST_cggraph_graph, an_output_feeds_several_inputs)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeForkableDesc ("test.Forkable"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId source = AddStub (graph, "test.Source", {}, { type });
	const NodeId left = AddStub (graph, "test.Left", { type }, {});
	const NodeId right = AddStub (graph, "test.Right", { type }, {});

	EXPECT_EQ (graph.Connect (source, 0, left, 0), ConnectStatus::Ok);
	EXPECT_EQ (graph.Connect (source, 0, right, 0), ConnectStatus::Ok);
	EXPECT_EQ (graph.GetLinks ().size (), 2u);
}

TEST (TEST_cggraph_graph, unknown_nodes_and_ports_are_refused)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeForkableDesc ("test.Forkable"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId source = AddStub (graph, "test.Source", {}, { type });
	const NodeId sink = AddStub (graph, "test.Sink", { type }, {});

	EXPECT_EQ (graph.Connect (kInvalidNodeId, 0, sink, 0), ConnectStatus::UnknownNode);
	EXPECT_EQ (graph.Connect (source, 0, 999, 0), ConnectStatus::UnknownNode);
	EXPECT_EQ (graph.Connect (source, 1, sink, 0), ConnectStatus::UnknownPort);
	EXPECT_EQ (graph.Connect (source, 0, sink, 1), ConnectStatus::UnknownPort);
	EXPECT_TRUE (graph.GetLinks ().empty ());
}

TEST (TEST_cggraph_graph, disconnecting_frees_the_input)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeForkableDesc ("test.Forkable"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId first = AddStub (graph, "test.First", {}, { type });
	const NodeId second = AddStub (graph, "test.Second", {}, { type });
	const NodeId sink = AddStub (graph, "test.Sink", { type }, {});

	ASSERT_EQ (graph.Connect (first, 0, sink, 0), ConnectStatus::Ok);
	EXPECT_TRUE (graph.Disconnect (sink, 0));
	EXPECT_FALSE (graph.Disconnect (sink, 0));
	EXPECT_EQ (graph.FindInputLink (sink, 0), nullptr);
	EXPECT_EQ (graph.Connect (second, 0, sink, 0), ConnectStatus::Ok);
}

TEST (TEST_cggraph_graph, upstream_reports_sources_in_port_order)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeForkableDesc ("test.Forkable"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId left = AddStub (graph, "test.Left", {}, { type });
	const NodeId right = AddStub (graph, "test.Right", {}, { type });
	const NodeId merge = AddStub (graph, "test.Merge", { type, type, type }, { type });

	ASSERT_EQ (graph.Connect (right, 0, merge, 1), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (left, 0, merge, 0), ConnectStatus::Ok);

	const std::vector<NodeId> upstream = graph.GetUpstream (merge);
	ASSERT_EQ (upstream.size (), 3u);
	EXPECT_EQ (upstream[0], left);
	EXPECT_EQ (upstream[1], right);
	EXPECT_EQ (upstream[2], kInvalidNodeId);

	EXPECT_TRUE (graph.GetUpstream (999).empty ());
}

TEST (TEST_cggraph_graph, a_null_node_is_refused_and_ids_stay_unique)
{
	Graph graph;
	EXPECT_EQ (graph.AddNode (nullptr), kInvalidNodeId);

	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeForkableDesc ("test.Forkable"));
	ASSERT_NE (type, nullptr);

	const NodeId a = AddStub (graph, "test.A", {}, { type });
	const NodeId b = AddStub (graph, "test.B", {}, { type });
	EXPECT_NE (a, b);
	EXPECT_NE (a, kInvalidNodeId);
	EXPECT_NE (graph.FindNode (a), nullptr);
	EXPECT_EQ (graph.FindNode (999), nullptr);
	EXPECT_EQ (graph.GetNodeIds ().size (), 2u);
}

// ---------------------------------------------------------------------------
//  Suppression d'un noeud
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_graph, removing_a_node_takes_all_its_links_with_it)
{
	// Les DEUX sens comptent : un noeud du milieu porte un lien entrant et deux
	// liens sortants. Une suppression qui n'emporterait que l'un des deux sens
	// laisserait un lien vers un noeud absent -- que Connect ne peut plus
	// refuser, puisqu'il est deja pose.
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeForkableDesc ("test.Forkable"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId source = AddStub (graph, "test.Source", {}, { type });
	const NodeId middle = AddStub (graph, "test.Middle", { type }, { type });
	const NodeId left = AddStub (graph, "test.Left", { type }, {});
	const NodeId right = AddStub (graph, "test.Right", { type }, {});
	const NodeId spare = AddStub (graph, "test.Spare", { type }, {});

	ASSERT_EQ (graph.Connect (source, 0, middle, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (middle, 0, left, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (middle, 0, right, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (source, 0, spare, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.GetLinks ().size (), 4u);

	EXPECT_EQ (graph.RemoveNode (middle), RemoveStatus::Ok);
	EXPECT_EQ (graph.GetNodeCount (), 4u);
	EXPECT_EQ (graph.FindNode (middle), nullptr);

	// Seul le lien etranger au noeud supprime survit.
	ASSERT_EQ (graph.GetLinks ().size (), 1u);
	EXPECT_EQ (graph.GetLinks ()[0].from, source);
	EXPECT_EQ (graph.GetLinks ()[0].to, spare);
	EXPECT_EQ (graph.FindInputLink (left, 0), nullptr);
	EXPECT_EQ (graph.FindInputLink (right, 0), nullptr);

	// Et l'entree ainsi liberee se recable : le lien est parti du graphe, pas
	// seulement de la liste que GetLinks rend.
	EXPECT_EQ (graph.Connect (source, 0, left, 0), ConnectStatus::Ok);
}

TEST (TEST_cggraph_graph, removing_a_node_leaves_a_hole_in_the_identifiers)
{
	// Le trou est la propriete, pas un effet de bord tolere : renumeroter
	// rendrait fausse toute reference exterieure au graphe, et le compteur qui
	// reculerait ferait reattribuer un identifiant qu'un document porte deja.
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeForkableDesc ("test.Forkable"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	std::vector<NodeId> created;
	for (int i = 0; i < 5; ++i)
		created.push_back (AddStub (graph, "test.N" + std::to_string (i), { type }, { type }));

	ASSERT_EQ (graph.RemoveNode (created[2]), RemoveStatus::Ok);

	const std::vector<NodeId> ids = graph.GetNodeIds ();
	ASSERT_EQ (ids.size (), 4u);
	EXPECT_EQ (ids[0], created[0]);
	EXPECT_EQ (ids[1], created[1]);
	EXPECT_EQ (ids[2], created[3]);
	EXPECT_EQ (ids[3], created[4]);

	// Le suivant ne comble pas le trou.
	const NodeId fresh = AddStub (graph, "test.Fresh", { type }, { type });
	EXPECT_GT (fresh, created[4]);
	EXPECT_NE (fresh, created[2]);
	EXPECT_EQ (graph.FindNode (created[2]), nullptr);
}

TEST (TEST_cggraph_graph, removing_an_unknown_node_is_named_and_changes_nothing)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeForkableDesc ("test.Forkable"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId source = AddStub (graph, "test.Source", {}, { type });
	const NodeId sink = AddStub (graph, "test.Sink", { type }, {});
	ASSERT_EQ (graph.Connect (source, 0, sink, 0), ConnectStatus::Ok);

	EXPECT_EQ (graph.RemoveNode (999), RemoveStatus::UnknownNode);
	EXPECT_EQ (graph.RemoveNode (kInvalidNodeId), RemoveStatus::UnknownNode);

	// Deux fois le meme noeud : la seconde ne trouve plus rien.
	EXPECT_EQ (graph.RemoveNode (sink), RemoveStatus::Ok);
	EXPECT_EQ (graph.RemoveNode (sink), RemoveStatus::UnknownNode);

	EXPECT_EQ (graph.GetNodeCount (), 1u);
	EXPECT_TRUE (graph.GetLinks ().empty ());
}

TEST (TEST_cggraph_graph, removing_a_node_frees_the_cycle_it_was_closing)
{
	// Versant negatif de la topologie : la suppression ne doit pas laisser dans
	// IsDownstream un chemin qui n'existe plus, sans quoi une connexion
	// parfaitement licite serait refusee comme cycle.
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeForkableDesc ("test.Forkable"));
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId a = AddStub (graph, "test.A", { type }, { type });
	const NodeId b = AddStub (graph, "test.B", { type }, { type });
	const NodeId c = AddStub (graph, "test.C", { type }, { type });

	ASSERT_EQ (graph.Connect (a, 0, b, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (b, 0, c, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (c, 0, a, 0), ConnectStatus::Cycle);

	ASSERT_EQ (graph.RemoveNode (b), RemoveStatus::Ok);
	EXPECT_EQ (graph.Connect (c, 0, a, 0), ConnectStatus::Ok);
}
