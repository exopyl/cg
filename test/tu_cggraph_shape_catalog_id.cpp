#include <gtest/gtest.h>

#include <cstddef>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "../src/cggraph/core/graph.h"
#include "../src/cggraph/core/serialize.h"
#include "../src/cggraph/nodes/archi/gothic_window.h"
#include "../src/cggraph/nodes/catalog.h"
#include "../src/cggraph/nodes/catalog_factory.h"
#include "../src/cggraph/nodes/shapes/parametric_shape.h"
#include "../src/cggraph/nodes/shapes/shape_catalog.h"
#include "../src/cgmesh/parametric_catalog.h"
#include <cgarchi/classical_parametric_catalog.h>
#include <cgarchi/gothic_parametric_catalog.h>
#include <nlohmann/json.hpp>

// ===========================================================================
//  Identifiant et categorie de forme portes par le catalogue (lot D1)
// ===========================================================================
//
//   - chaque identifiant du catalogue reuni est non vide et unique ;
//   - les identifiants sont exactement les types de noeud de la reference
//     gelee test/data/graphs/shape_parameters_before_q3.json, qui se relit ;
//   - chaque categorie est connue, et suit la liste proprietaire ;
//   - le type de chaque noeud de forme est l'identifiant de sa forme ;
//   - la recherche par identifiant trouve chaque forme, et elle seule.
//
// La reference gelee ne se regenere jamais (tu_cggraph_parameter_id.cpp).

using namespace cggraph;
using namespace cggraph_nodes;

namespace
{

const char *kBeforeQ3 = "./test/data/graphs/shape_parameters_before_q3.json";

// Les pages de maker (maker/web/js/pages : shapes.js, gothic.js, orders.js).
const std::set<std::string> kKnownCategories = { "parametric", "gothic", "classical" };

std::string ReadText (const char *path)
{
	std::string text;
	std::FILE *f = std::fopen (path, "rb");
	if (!f)
		return text;
	std::fseek (f, 0, SEEK_END);
	const long size = std::ftell (f);
	std::fseek (f, 0, SEEK_SET);
	if (size > 0)
	{
		text.resize (static_cast<std::size_t> (size));
		if (std::fread (&text[0], 1, text.size (), f) != text.size ())
			text.clear ();
	}
	std::fclose (f);
	return text;
}

} // namespace

TEST (TEST_cggraph_shape_catalog_id, every_id_of_the_joined_catalogue_is_non_empty_and_unique)
{
	std::set<std::string> ids;
	for (const ParametricShapeEntry &entry : CatalogShapes ())
	{
		ASSERT_NE (entry.id, nullptr) << entry.name;
		EXPECT_STRNE (entry.id, "") << entry.name;
		EXPECT_TRUE (ids.insert (entry.id).second) << "identifiant en double : " << entry.id;
	}
	EXPECT_EQ (ids.size (), 29u);
}

TEST (TEST_cggraph_shape_catalog_id, the_ids_are_the_node_types_of_the_frozen_document)
{
	const std::string before = ReadText (kBeforeQ3);
	ASSERT_FALSE (before.empty ()) << kBeforeQ3;
	const nlohmann::json document = nlohmann::json::parse (before);

	std::multiset<std::string> types;
	for (const nlohmann::json &node : document.at ("nodes"))
		types.insert (node.at ("type").get<std::string> ());

	std::multiset<std::string> ids;
	for (const ParametricShapeEntry &entry : CatalogShapes ())
		ids.insert (entry.id);

	EXPECT_EQ (ids, types);
	EXPECT_EQ (types.size (), 29u);
}

TEST (TEST_cggraph_shape_catalog_id, the_frozen_document_loads_one_catalogue_shape_per_node)
{
	const std::string before = ReadText (kBeforeQ3);
	ASSERT_FALSE (before.empty ()) << kBeforeQ3;

	Graph graph;
	const LoadResult loaded = LoadGraph (before, CatalogFactory (), graph);
	ASSERT_EQ (loaded.status, SerializeStatus::Ok) << ToString (loaded.status) << " : " << loaded.detail;
	ASSERT_EQ (graph.GetNodeCount (), 29u);

	std::set<std::string> loadedTypes;
	for (NodeId id : graph.GetNodeIds ())
	{
		const Node *node = graph.FindNode (id);
		ASSERT_NE (node, nullptr);
		loadedTypes.insert (node->GetDesc ().typeName);
	}
	std::set<std::string> ids;
	for (const ParametricShapeEntry &entry : CatalogShapes ())
		ids.insert (entry.id);
	EXPECT_EQ (loadedTypes, ids);
}

TEST (TEST_cggraph_shape_catalog_id, every_category_is_known_and_follows_the_owner_list)
{
	struct Owner
	{
		const char *label;
		const std::vector<ParametricShapeEntry> *shapes;
		const char *category;
	};
	const Owner owners[] = {
		{ "cgmesh", &ParametricShapes (), "parametric" },
		{ "gothique", &cgarchi::GothicParametricShapes (), "gothic" },
		{ "classique", &cgarchi::ClassicalParametricShapes (), "classical" },
	};

	std::map<std::string, std::size_t> perCategory;
	for (const Owner &owner : owners)
	{
		ASSERT_FALSE (owner.shapes->empty ()) << owner.label;
		for (const ParametricShapeEntry &entry : *owner.shapes)
		{
			ASSERT_NE (entry.category, nullptr) << entry.name;
			EXPECT_STREQ (entry.category, owner.category) << owner.label << " : " << entry.name;
		}
	}
	for (const ParametricShapeEntry &entry : CatalogShapes ())
	{
		ASSERT_NE (entry.category, nullptr) << entry.name;
		EXPECT_EQ (kKnownCategories.count (entry.category), 1u)
			<< entry.name << " : categorie inconnue « " << entry.category << " »";
		++perCategory[entry.category];
	}

	// Les cardinalites que maker publie (listShapes) et que la CI attend
	// (MAKER_EXPECTED_SHAPES, MAKER_EXPECTED_GOTHIC).
	EXPECT_EQ (perCategory["parametric"], 25u);
	EXPECT_EQ (perCategory["gothic"], 2u);
	EXPECT_EQ (perCategory["classical"], 2u);
}

TEST (TEST_cggraph_shape_catalog_id, the_node_type_is_the_catalogue_id)
{
	// Controle de coherence entre liaison, descripteur et catalogue. Il ne
	// prouve pas l'absence de copie : c'est le role de la recherche consignee
	// au plan (plan_implementation_02_revision.md, section 6.7).
	ASSERT_FALSE (ShapeBindings ().empty ());
	for (const ShapeBinding &binding : ShapeBindings ())
	{
		ASSERT_NE (binding.shape, nullptr);
		EXPECT_STREQ (binding.typeName, binding.shape->id);

		std::unique_ptr<Node> node = MakeNode (binding.typeName);
		ASSERT_NE (node, nullptr) << binding.typeName;
		EXPECT_EQ (node->GetDesc ().typeName, binding.shape->id);
	}

	const ParametricShapeEntry *window = FindCatalogShapeById ("shape.gothic.window");
	ASSERT_NE (window, nullptr);
	EXPECT_STREQ (GothicWindowTypeName (), window->id);
	std::unique_ptr<Node> node = MakeNode (window->id);
	ASSERT_NE (node, nullptr);
	EXPECT_EQ (node->GetDesc ().typeName, window->id);
}

TEST (TEST_cggraph_shape_catalog_id, a_shape_is_found_by_its_id)
{
	for (const ParametricShapeEntry &entry : CatalogShapes ())
		EXPECT_EQ (FindCatalogShapeById (entry.id), &entry) << entry.id;
	EXPECT_EQ (FindCatalogShapeById ("Cube"), nullptr);
	EXPECT_EQ (FindCatalogShapeById (""), nullptr);
	EXPECT_EQ (FindCatalogShapeById ("shape.no_such_shape"), nullptr);
}
