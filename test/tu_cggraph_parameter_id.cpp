#include <gtest/gtest.h>

#include <cstddef>
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "../src/cggraph/core/graph.h"
#include "../src/cggraph/core/serialize.h"
#include "../src/cggraph/nodes/catalog.h"
#include "../src/cggraph/nodes/catalog_factory.h"
#include "../src/cggraph/nodes/shapes/parametric_shape.h"
#include "../src/cggraph/nodes/shapes/shape_catalog.h"
#include "../src/cgmesh/parameterized.h"
#include "../src/cgmesh/parameterized_shapes.h"
#include <cgarchi/parameterized_gothic.h>
#include <cgarchi/parameterized_gothic_microarchitecture.h>
#include <nlohmann/json.hpp>

// ===========================================================================
//  Identifiant stable de parametre (Parameter::Id, lot Q3)
// ===========================================================================
//
//   - chaque cle d'un document ecrit avant l'identifiant est encore un Id ()
//     de son type : le document se relit ;
//   - les documents des formes du catalogue s'ecrivent octet pour octet comme
//     avant l'identifiant ;
//   - un parametre renomme qui garde son identifiant se relit depuis un
//     document ecrit sous son ancien libelle.
//
// REGLE : test/data/graphs/shape_parameters_before_q3.json est GELEE. Elle ne
// se regenere jamais. Un echec contre elle signale une cle de document perdue,
// ou un defaut change, a examiner ; jamais une reference a mettre a jour. Si un
// defaut change legitimement, c'est le test a l'octet qu'on retire ; le test
// d'inclusion des cles, lui, reste.

using namespace cggraph;
using namespace cggraph_nodes;

namespace
{

const char *kBeforeQ3 = "./test/data/graphs/shape_parameters_before_q3.json";

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

std::string WithoutCarriageReturns (const std::string &text)
{
	std::string out;
	out.reserve (text.size ());
	for (char c : text)
		if (c != '\r')
			out += c;
	return out;
}

// Forme de test a deux parametres. Le libelle et l'identifiant de la hauteur
// sont choisis a la construction : deux instances jouent la meme forme avant et
// apres le renommage de son libelle.
class LabelledShape : public IParameterized
{
public:
	LabelledShape (std::string heightLabel, std::string heightId)
		: m_heightLabel (std::move (heightLabel)), m_heightId (std::move (heightId))
	{
	}

	std::vector<Parameter> GetParameters () override
	{
		Parameter height = Parameter::MakeFloat (m_heightLabel, &m_height, 0.f, 100.f);
		if (!m_heightId.empty ())
			height = height.WithId (m_heightId);
		return { height, Parameter::MakeInt ("Count", &m_count, 1, 10) };
	}

	void Regenerate () override {}
	std::string GetName () const override { return "Labelled"; }

	float m_height = 10.f;
	int m_count = 3;

private:
	std::string m_heightLabel;
	std::string m_heightId;
};

const char *kLabelledType = "test.labelled";

// Le noeud de la forme de test, construit comme ParametricShapeNode : valeurs
// par defaut posees par DeclareShapeParams.
class LabelledNode : public Node
{
public:
	explicit LabelledNode (LabelledShape &shape)
	{
		DeclareShapeParams (shape, GetParams ());
	}

	const NodeDesc &GetDesc () const override
	{
		static const NodeDesc desc = [] {
			NodeDesc d;
			d.typeName = kLabelledType;
			return d;
		}();
		return desc;
	}

	bool Compute (EvalContext &, const ValueList &, ValueList &) override { return false; }
};

// Libelle « Height » sans identifiant : la forme telle qu'elle etait publiee.
LabelledShape ShapeBefore () { return LabelledShape ("Height", ""); }

// Libelle renomme, identifiant pose a la valeur de l'ancien libelle.
LabelledShape ShapeAfter () { return LabelledShape ("Height (mm)", "Height"); }

class AfterFactory : public NodeFactory
{
public:
	std::unique_ptr<Node> Create (const std::string &typeName) const override
	{
		if (typeName != kLabelledType)
			return nullptr;
		LabelledShape shape = ShapeAfter ();
		return std::make_unique<LabelledNode> (shape);
	}
};

std::set<std::string> IdsOf (IParameterized &shape)
{
	std::set<std::string> ids;
	for (const Parameter &p : shape.GetParameters ())
		ids.insert (p.Id ());
	return ids;
}

// Les Id () des parametres de la forme que le type de noeud emballe. Pour la
// fenetre gothique, s'y ajoutent les parametres que le noeud declare lui-meme
// (controle d'impression), qui ne viennent d'aucun Parameter.
bool AcceptedKeys (const std::string &typeName, std::set<std::string> &keys)
{
	if (const ShapeBinding *binding = FindShapeBinding (typeName))
	{
		std::unique_ptr<IParameterized> shape = binding->shape->make ();
		if (shape == nullptr)
			return false;
		keys = IdsOf (*shape);
		return true;
	}
	if (typeName == "shape.gothic.window")
	{
		cgarchi::ParameterizedGothicWindow window;
		keys = IdsOf (window);
		std::unique_ptr<Node> node = MakeNode (typeName);
		if (node == nullptr)
			return false;
		for (const ParamEntry &entry : node->GetParams ().GetEntries ())
			keys.insert (entry.name);
		return true;
	}
	return false;
}

// Un noeud par liaison du catalogue, dans l'ordre de ShapeBindings (), puis la
// fenetre gothique, qui a son propre descripteur. Valeurs par defaut.
std::string CatalogueDocument ()
{
	Graph graph;
	for (const ShapeBinding &binding : ShapeBindings ())
		graph.AddNode (MakeNode (binding.typeName));
	graph.AddNode (MakeNode ("shape.gothic.window"));
	return SaveGraph (graph);
}

} // namespace

TEST (TEST_cggraph_parameter_id, the_catalogue_document_is_written_byte_for_byte_as_before)
{
	const std::string before = WithoutCarriageReturns (ReadText (kBeforeQ3));
	ASSERT_FALSE (before.empty ()) << kBeforeQ3;
	EXPECT_EQ (CatalogueDocument (), before);
}

TEST (TEST_cggraph_parameter_id, every_key_of_a_document_written_before_is_still_an_id_of_its_type)
{
	const std::string before = ReadText (kBeforeQ3);
	ASSERT_FALSE (before.empty ()) << kBeforeQ3;
	const nlohmann::json document = nlohmann::json::parse (before);

	std::size_t checked = 0;
	for (const nlohmann::json &node : document.at ("nodes"))
	{
		const std::string type = node.at ("type").get<std::string> ();
		std::set<std::string> keys;
		if (!AcceptedKeys (type, keys))
		{
			ADD_FAILURE () << "type inconnu : " << type;
			continue;
		}
		for (const nlohmann::json &param : node.at ("params"))
		{
			const std::string name = param.at ("name").get<std::string> ();
			EXPECT_EQ (keys.count (name), 1u) << type << " : la cle « " << name
			                                  << " » n'est plus un Id ()";
			++checked;
		}
	}
	EXPECT_EQ (checked, 155u);
}

TEST (TEST_cggraph_parameter_id, the_id_defaults_to_the_name)
{
	float value = 0.f;
	const Parameter plain = Parameter::MakeFloat ("Height (mm)", &value, 0.f, 1.f);
	EXPECT_EQ (plain.Id (), "Height (mm)");

	const Parameter tagged = plain.WithId ("Height");
	EXPECT_EQ (tagged.Id (), "Height");
	EXPECT_EQ (tagged.GetName (), "Height (mm)");
	EXPECT_EQ (plain.Id (), "Height (mm)");

	EXPECT_EQ (tagged.WithId ("").Id (), "Height (mm)");
}

TEST (TEST_cggraph_parameter_id, a_relabelled_parameter_that_keeps_its_id_reads_a_document_written_before)
{
	// Document ecrit par la forme d'avant, valeurs differentes des defauts.
	LabelledShape before = ShapeBefore ();
	before.m_height = 42.f;
	before.m_count = 7;
	Graph written;
	written.AddNode (std::make_unique<LabelledNode> (before));
	const std::string document = SaveGraph (written);
	ASSERT_NE (document.find ("\"Height\""), std::string::npos);

	Graph graph;
	const LoadResult loaded = LoadGraph (document, AfterFactory (), graph);
	ASSERT_EQ (loaded.status, SerializeStatus::Ok) << loaded.detail;
	ASSERT_EQ (graph.GetNodeCount (), 1u);
	const Node *node = graph.FindNode (graph.GetNodeIds ().front ());
	ASSERT_NE (node, nullptr);

	LabelledShape after = ShapeAfter ();
	ApplyShapeParams (after, node->GetParams ());
	EXPECT_EQ (after.m_height, 42.f);
	EXPECT_EQ (after.m_count, 7);
}

TEST (TEST_cggraph_parameter_id, a_relabelled_parameter_is_written_under_its_id)
{
	LabelledShape after = ShapeAfter ();
	Graph graph;
	graph.AddNode (std::make_unique<LabelledNode> (after));
	const std::string document = SaveGraph (graph);
	EXPECT_NE (document.find ("\"name\": \"Height\""), std::string::npos) << document;
	EXPECT_EQ (document.find ("Height (mm)"), std::string::npos) << document;
}

TEST (TEST_cggraph_parameter_id, the_ids_of_every_shape_maker_sets_are_unique_and_non_empty)
{
	std::vector<std::pair<std::string, std::vector<Parameter>>> shapes;
	for (const ParametricShapeEntry &entry : CatalogShapes ())
	{
		std::unique_ptr<IParameterized> shape = entry.make ();
		ASSERT_NE (shape, nullptr) << entry.name;
		shapes.emplace_back (entry.name, shape->GetParameters ());
	}
	cgarchi::ParameterizedGothicWindow window;
	shapes.emplace_back (window.GetName (), window.GetParameters ());

	// Formes que maker regle hors catalogue. Leur liste de parametres ne depend
	// pas de la ressource : un fichier absent suffit a les construire.
	ParameterizedSvgExtrusion svg ("./tu_parameter_id_does_not_exist.svg");
	shapes.emplace_back (svg.GetName (), svg.GetParameters ());
	ParameterizedImageRelief relief ("./tu_parameter_id_does_not_exist.ppm");
	shapes.emplace_back (relief.GetName (), relief.GetParameters ());
	ParameterizedImagePixelBlocks pixels ("./tu_parameter_id_does_not_exist.ppm");
	shapes.emplace_back (pixels.GetName (), pixels.GetParameters ());
	ParameterizedText3D text ("./tu_parameter_id_does_not_exist.ttf");
	shapes.emplace_back (text.GetName (), text.GetParameters ());
	cgarchi::ParameterizedGothicMicroarchitecture micro;
	shapes.emplace_back (micro.GetName (), micro.GetParameters ());

	std::size_t count = 0;
	for (const auto &shape : shapes)
	{
		std::set<std::string> ids;
		for (const Parameter &p : shape.second)
		{
			EXPECT_FALSE (p.Id ().empty ()) << shape.first;
			EXPECT_TRUE (ids.insert (p.Id ()).second) << shape.first << ": " << p.Id ();
			++count;
		}
	}
	EXPECT_GT (count, 0u);
}
