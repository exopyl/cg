#include <gtest/gtest.h>

#include "../src/cggraph/core/evaluator.h"
#include "../src/cggraph/core/serialize.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// ===========================================================================
//  Couche A : le document du graphe
// ===========================================================================
// Comme pour l'evaluateur, les types manipules ici sont INVENTES : le moteur ne
// connait aucune bibliotheque de domaine, et ces tests le prouvent en n'en
// incluant aucune. Ce qui se verifie ici est le DOCUMENT -- ce qu'il porte, ce
// qu'il rend a la relecture, et ce qu'il refuse.

using namespace cggraph;

namespace {

struct Shape
{
	std::vector<float> positions;
};

std::shared_ptr<void> CloneShape (const void *value)
{
	return std::make_shared<Shape> (*static_cast<const Shape *> (value));
}

std::size_t SizeOfShape (const void *value)
{
	return static_cast<const Shape *> (value)->positions.size () * sizeof (float);
}

// Registre unique du fichier : l'identite d'un type est une identite de
// POINTEUR, donc deux registres produiraient deux "test.Shape" incompatibles
// entre un graphe sauve et un graphe relu.
const TypeDesc *ShapeType ()
{
	static TypeRegistry registry;
	static const TypeDesc *type = [] {
		TypeDesc desc;
		desc.name = "test.Shape";
		desc.clone = &CloneShape;
		desc.sizeHint = &SizeOfShape;
		desc.mutability = TypeDesc::Forkable;
		return registry.Register (desc);
	}();
	return type;
}

class DocNode : public Node
{
public:
	const NodeDesc &GetDesc () const override { return m_desc; }
	bool Compute (EvalContext &, const ValueList &, ValueList &) override { return true; }

protected:
	NodeDesc m_desc;
};

// Source portant les QUATRE types de parametres, les deux roles, les deux
// natures et les deux visibilites : ce qui ne traverse pas le document se voit
// ici ou nulle part.
class SourceNode : public DocNode
{
public:
	SourceNode ()
	{
		m_desc.typeName = "test.source";
		PortDesc out;
		out.name = "forme";
		out.type = ShapeType ();
		m_desc.outputs.push_back (out);

		GetParams ().SetInt ("seed", 7);
		GetParams ().SetFloat ("scale", 0.25f);
		GetParams ().SetBool ("closed", true);
		GetParams ().SetString ("label", "modele");
		GetParams ().SetString ("logLabel", "muet", ParamRole::NonSemantic);
		GetParams ().SetDriven ("width", ParamType::Float, "hauteur * 2");

		// Semantique ET interne, comme `source.identity` des deux noeuds
		// sources : il entre dans la signature et ne se regle pas a la main.
		GetParams ().SetString ("cache.key", "abc", ParamRole::Semantic,
		                        ParamVisibility::Internal);
	}

	bool Compute (EvalContext &, const ValueList &, ValueList &out) override
	{
		const ParamValue *seed = GetParams ().Find ("seed");
		std::shared_ptr<Shape> shape = std::make_shared<Shape> ();
		shape->positions.push_back (seed != nullptr ? static_cast<float> (seed->intValue) : 0.0f);
		out[0] = Value::Make (ShapeType (), shape);
		return true;
	}
};

// Meme forme, sans parametre Driven : evaluable, contrairement a SourceNode.
class PlainSourceNode : public DocNode
{
public:
	PlainSourceNode ()
	{
		m_desc.typeName = "test.plainSource";
		PortDesc out;
		out.name = "forme";
		out.type = ShapeType ();
		m_desc.outputs.push_back (out);
		GetParams ().SetInt ("seed", 3);
	}

	bool Compute (EvalContext &, const ValueList &, ValueList &out) override
	{
		const ParamValue *seed = GetParams ().Find ("seed");
		std::shared_ptr<Shape> shape = std::make_shared<Shape> ();
		shape->positions.push_back (seed != nullptr ? static_cast<float> (seed->intValue) : 0.0f);
		out[0] = Value::Make (ShapeType (), shape);
		return true;
	}
};

class RelaxNode : public DocNode
{
public:
	RelaxNode ()
	{
		m_desc.typeName = "test.relax";
		PortDesc in;
		in.name = "forme";
		in.type = ShapeType ();
		m_desc.inputs.push_back (in);
		PortDesc out;
		out.name = "forme";
		out.type = ShapeType ();
		m_desc.outputs.push_back (out);
		GetParams ().SetInt ("iterations", 2);
	}

	bool Compute (EvalContext &, const ValueList &in, ValueList &out) override
	{
		const Shape *source = in[0].Get<Shape> (ShapeType ());
		if (source == nullptr)
			return false;
		std::shared_ptr<Shape> result = std::make_shared<Shape> (*source);
		const ParamValue *iterations = GetParams ().Find ("iterations");
		for (float &value : result->positions)
			value *= (iterations != nullptr ? static_cast<float> (iterations->intValue) : 1.0f);
		out[0] = Value::Make (ShapeType (), result);
		return true;
	}
};

// Type de noeud dont la VERSION courante est 2 : un document qui le declare en
// version 1 est un document qu'aucune migration n'a traverse.
class SecondVersionNode : public DocNode
{
public:
	SecondVersionNode ()
	{
		m_desc.typeName = "test.v2";
		m_desc.version = 2;
		PortDesc out;
		out.name = "forme";
		out.type = ShapeType ();
		m_desc.outputs.push_back (out);
		GetParams ().SetInt ("seed", 1);
	}

	bool Compute (EvalContext &, const ValueList &, ValueList &out) override
	{
		out[0] = Value::Make (ShapeType (), std::make_shared<Shape> ());
		return true;
	}
};

class TestFactory : public NodeFactory
{
public:
	std::unique_ptr<Node> Create (const std::string &typeName) const override
	{
		if (typeName == "test.source")
			return std::unique_ptr<Node> (new SourceNode ());
		if (typeName == "test.plainSource")
			return std::unique_ptr<Node> (new PlainSourceNode ());
		if (typeName == "test.relax")
			return std::unique_ptr<Node> (new RelaxNode ());
		if (typeName == "test.v2")
			return std::unique_ptr<Node> (new SecondVersionNode ());
		return nullptr;
	}
};

// Graphe de reference : une source riche en parametres, deux etages de relax,
// des positions d'ecran distinctes et une reference de sous-graphe.
struct Document
{
	Graph graph;
	NodeId source = kInvalidNodeId;
	NodeId relax = kInvalidNodeId;
	NodeId polish = kInvalidNodeId;

	Document ()
	{
		source = graph.AddNode (std::unique_ptr<Node> (new SourceNode ()));
		relax = graph.AddNode (std::unique_ptr<Node> (new RelaxNode ()));
		polish = graph.AddNode (std::unique_ptr<Node> (new RelaxNode ()));
		graph.Connect (source, 0, relax, 0);
		graph.Connect (relax, 0, polish, 0);
		graph.SetNodePosition (source, -120.5f, 40.0f);
		graph.SetNodePosition (relax, 0.25f, -8.75f);
		graph.SetNodePosition (polish, 320.0f, 12.5f);
		graph.SetNodeSubgraph (polish, "finition.json");
	}
};

std::string TempPath (const char *name)
{
	return std::string ("./") + name;
}

} // namespace

// ---------------------------------------------------------------------------
//  Aller-retour
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_serialize, a_round_trip_is_byte_for_byte_identical)
{
	Document document;
	const std::string first = SaveGraph (document.graph);

	Graph reloaded;
	const TestFactory factory;
	const LoadResult result = LoadGraph (first, factory, reloaded);
	ASSERT_TRUE (result.IsOk ()) << result.detail;
	EXPECT_TRUE (result.incompatible.empty ());

	const std::string second = SaveGraph (reloaded);
	EXPECT_EQ (first, second);

	// Le graphe relu porte la meme topologie, pas seulement le meme texte.
	EXPECT_EQ (reloaded.GetNodeCount (), 3u);
	ASSERT_EQ (reloaded.GetLinks ().size (), 2u);
	EXPECT_EQ (reloaded.GetLinks ()[0].from, document.source);
	EXPECT_EQ (reloaded.GetLinks ()[0].to, document.relax);
	EXPECT_EQ (reloaded.GetLinks ()[1].from, document.relax);
	EXPECT_EQ (reloaded.GetLinks ()[1].to, document.polish);
}

TEST (TEST_cggraph_serialize, the_document_carries_the_screen_positions)
{
	Document document;
	Graph reloaded;
	const TestFactory factory;
	ASSERT_TRUE (LoadGraph (SaveGraph (document.graph), factory, reloaded).IsOk ());

	float x = 0.0f;
	float y = 0.0f;
	ASSERT_TRUE (reloaded.GetNodePosition (document.source, x, y));
	EXPECT_FLOAT_EQ (x, -120.5f);
	EXPECT_FLOAT_EQ (y, 40.0f);
	ASSERT_TRUE (reloaded.GetNodePosition (document.relax, x, y));
	EXPECT_FLOAT_EQ (x, 0.25f);
	EXPECT_FLOAT_EQ (y, -8.75f);

	// Versant negatif : deplacer une boite CHANGE le document. Sans lui, un
	// document qui ne porterait aucune position passerait le versant positif
	// tant que le graphe relu garderait ses valeurs par defaut.
	const std::string before = SaveGraph (document.graph);
	document.graph.SetNodePosition (document.relax, 999.0f, -999.0f);
	EXPECT_NE (before, SaveGraph (document.graph));
}

TEST (TEST_cggraph_serialize, the_document_carries_a_subgraph_reference)
{
	Document document;
	const std::string text = SaveGraph (document.graph);
	EXPECT_NE (text.find ("\"subgraph\""), std::string::npos);
	EXPECT_NE (text.find ("finition.json"), std::string::npos);

	Graph reloaded;
	const TestFactory factory;
	ASSERT_TRUE (LoadGraph (text, factory, reloaded).IsOk ());
	EXPECT_EQ (reloaded.GetNodeSubgraph (document.polish), "finition.json");

	// Un noeud qui ne delegue rien n'ecrit pas le champ, et le relit vide.
	EXPECT_EQ (reloaded.GetNodeSubgraph (document.source), "");
	EXPECT_EQ (text.find ("\"subgraph\""), text.rfind ("\"subgraph\""));
}

TEST (TEST_cggraph_serialize, a_subgraph_reference_moves_the_signature)
{
	// La reference designe le CALCUL : deux delegations differentes n'ont
	// aucune raison de partager une entree de cache. Ce cas est ce qui empeche
	// le champ d'etre decoratif -- un document peut le porter sans que rien ne
	// l'utilise, une signature non.
	Graph graph;
	const NodeId node = graph.AddNode (std::unique_ptr<Node> (new PlainSourceNode ()));

	const Hash none = Signature (graph, node);
	ASSERT_TRUE (graph.SetNodeSubgraph (node, "a.json"));
	const Hash first = Signature (graph, node);
	ASSERT_TRUE (graph.SetNodeSubgraph (node, "b.json"));
	const Hash second = Signature (graph, node);

	EXPECT_NE (none, first);
	EXPECT_NE (first, second);
}

TEST (TEST_cggraph_serialize, every_parameter_kind_survives_the_round_trip)
{
	Document document;
	Graph reloaded;
	const TestFactory factory;
	ASSERT_TRUE (LoadGraph (SaveGraph (document.graph), factory, reloaded).IsOk ());

	const Node *source = reloaded.FindNode (document.source);
	ASSERT_NE (source, nullptr);
	const ParamSet &params = source->GetParams ();
	ASSERT_EQ (params.GetCount (), 7u);

	// L'ordre de declaration est celui du document : il compte, la signature
	// hachant les entrees dans cet ordre.
	const std::deque<ParamEntry> &entries = params.GetEntries ();
	EXPECT_EQ (entries[0].name, "seed");
	EXPECT_EQ (entries[5].name, "width");

	const ParamValue *seed = params.Find ("seed");
	ASSERT_NE (seed, nullptr);
	EXPECT_EQ (seed->kind, ParamKind::Literal);
	EXPECT_EQ (seed->type, ParamType::Int);
	EXPECT_EQ (seed->intValue, 7);

	const ParamValue *scale = params.Find ("scale");
	ASSERT_NE (scale, nullptr);
	EXPECT_FLOAT_EQ (scale->floatValue, 0.25f);

	const ParamValue *closed = params.Find ("closed");
	ASSERT_NE (closed, nullptr);
	EXPECT_TRUE (closed->boolValue);

	const ParamValue *label = params.Find ("label");
	ASSERT_NE (label, nullptr);
	EXPECT_EQ (label->stringValue, "modele");

	// Le role traverse : un parametre non semantique relu comme semantique
	// invaliderait des calculs que rien ne devrait invalider.
	const ParamEntry *quiet = nullptr;
	for (const ParamEntry &entry : entries)
		if (entry.name == "logLabel")
			quiet = &entry;
	ASSERT_NE (quiet, nullptr);
	EXPECT_EQ (quiet->role, ParamRole::NonSemantic);

	// La variante Driven se sauve, se relit et se hache sans que personne sache
	// l'evaluer -- c'est tout ce que D13 demandait de tenir ici.
	const ParamValue *width = params.Find ("width");
	ASSERT_NE (width, nullptr);
	EXPECT_EQ (width->kind, ParamKind::Driven);
	EXPECT_EQ (width->type, ParamType::Float);
	EXPECT_EQ (width->expression, "hauteur * 2");

	// La visibilite traverse elle aussi, et les deux valeurs sont verifiees :
	// « tout est interne » et « tout est public » satisferaient chacun la
	// moitie du cas.
	const ParamEntry *internalEntry = params.FindEntry ("cache.key");
	ASSERT_NE (internalEntry, nullptr);
	EXPECT_EQ (internalEntry->visibility, ParamVisibility::Internal);
	EXPECT_EQ (internalEntry->role, ParamRole::Semantic);
	const ParamEntry *publicEntry = params.FindEntry ("seed");
	ASSERT_NE (publicEntry, nullptr);
	EXPECT_EQ (publicEntry->visibility, ParamVisibility::Public);

	// Et la signature du noeud relu est celle du noeud sauve : ce qui traverse
	// le document est exactement ce que le cache indexe.
	EXPECT_EQ (Signature (reloaded, document.source), Signature (document.graph, document.source));
}

TEST (TEST_cggraph_serialize, the_document_is_authoritative_on_parameters)
{
	// Un document ecrit sans des parametres que le constructeur du noeud pose :
	// ceux-la ne doivent pas REAPPARAITRE a la relecture, sans quoi la
	// re-sauvegarde n'est pas fidele et le noeud relu ne se hache pas comme le
	// noeud dont le document vient.
	//
	// Le noeud choisi en pose SEPT et le document n'en declare que DEUX : un
	// noeud a parametre unique rendrait ce cas muet, la mise a jour en place
	// suffisant alors a produire le bon resultat.
	const std::string text =
		"{ \"format\": \"cggraph\", \"formatVersion\": 1, \"links\": [],"
		" \"nodes\": [ { \"id\": 1, \"type\": \"test.source\", \"version\": 1,"
		" \"x\": 0.0, \"y\": 0.0, \"params\": ["
		" { \"kind\": \"literal\", \"name\": \"seed\", \"role\": \"semantic\","
		" \"type\": \"int\", \"value\": 42 },"
		" { \"kind\": \"literal\", \"name\": \"label\", \"role\": \"semantic\","
		" \"type\": \"string\", \"value\": \"relu\" } ] } ] }";

	Graph graph;
	const TestFactory factory;
	ASSERT_TRUE (LoadGraph (text, factory, graph).IsOk ());

	const Node *node = graph.FindNode (1);
	ASSERT_NE (node, nullptr);
	EXPECT_EQ (node->GetParams ().GetCount (), 2u);
	ASSERT_NE (node->GetParams ().Find ("seed"), nullptr);
	EXPECT_EQ (node->GetParams ().Find ("seed")->intValue, 42);
	ASSERT_NE (node->GetParams ().Find ("label"), nullptr);
	EXPECT_EQ (node->GetParams ().Find ("label")->stringValue, "relu");

	// Les cinq que le constructeur pose et que le document tait ont disparu.
	EXPECT_EQ (node->GetParams ().Find ("scale"), nullptr);
	EXPECT_EQ (node->GetParams ().Find ("closed"), nullptr);
	EXPECT_EQ (node->GetParams ().Find ("logLabel"), nullptr);
	EXPECT_EQ (node->GetParams ().Find ("width"), nullptr);
	EXPECT_EQ (node->GetParams ().Find ("cache.key"), nullptr);

	// Et la re-sauvegarde ne les ressuscite pas.
	const std::string resaved = SaveGraph (graph);
	EXPECT_EQ (resaved.find ("closed"), std::string::npos);
	EXPECT_EQ (resaved.find ("hauteur * 2"), std::string::npos);

	Graph again;
	ASSERT_TRUE (LoadGraph (resaved, factory, again).IsOk ());
	EXPECT_EQ (SaveGraph (again), resaved);
}

TEST (TEST_cggraph_serialize, identifiers_are_preserved_and_the_next_one_does_not_collide)
{
	// Un identifiant designe un noeud AILLEURS que dans le graphe : selection
	// d'un editeur, argument d'un pilote. Le renumeroter a la relecture rendrait
	// ces references fausses en silence.
	//
	// Le document est ecrit A LA MAIN avec des identifiants NON CONTIGUS et dans
	// le desordre, et c'est ce qui donne au cas sa valeur : un graphe construit
	// en memoire porte toujours 1..N dans l'ordre, si bien qu'une relecture qui
	// renumeroterait rendrait exactement les memes identifiants et passerait
	// inapercue.
	const std::string text =
		"{ \"format\": \"cggraph\", \"formatVersion\": 1,"
		" \"nodes\": [ { \"id\": 10, \"type\": \"test.plainSource\", \"version\": 1,"
		" \"params\": [] },"
		" { \"id\": 4, \"type\": \"test.relax\", \"version\": 1, \"params\": [] } ],"
		" \"links\": [ { \"from\": 10, \"fromPort\": 0, \"to\": 4, \"toPort\": 0 } ] }";

	Graph graph;
	const TestFactory factory;
	ASSERT_TRUE (LoadGraph (text, factory, graph).IsOk ());

	const std::vector<NodeId> ids = graph.GetNodeIds ();
	ASSERT_EQ (ids.size (), 2u);
	EXPECT_EQ (ids[0], 10u);
	EXPECT_EQ (ids[1], 4u);
	ASSERT_EQ (graph.GetLinks ().size (), 1u);
	EXPECT_EQ (graph.GetLinks ()[0].from, 10u);
	EXPECT_EQ (graph.GetLinks ()[0].to, 4u);

	// Le compteur repart AU-DELA du plus grand identifiant repris : un noeud
	// ajoute apres la relecture ne doit pas recevoir un identifiant deja pose.
	const NodeId fresh = graph.AddNode (std::unique_ptr<Node> (new RelaxNode ()));
	EXPECT_GT (fresh, 10u);
	EXPECT_NE (graph.FindNode (10), nullptr);
	EXPECT_NE (graph.FindNode (4), nullptr);

	// Et l'aller-retour reste fidele sur des identifiants qu'aucun AddNode
	// n'aurait attribues.
	Graph again;
	const std::string saved = SaveGraph (graph);
	ASSERT_TRUE (LoadGraph (saved, factory, again).IsOk ());
	EXPECT_EQ (SaveGraph (again), saved);
}

TEST (TEST_cggraph_serialize, a_hollow_identifier_sequence_survives_the_round_trip)
{
	// Le cas ci-dessus part d'un document ECRIT A LA MAIN ; celui-ci part d'un
	// graphe que l'editeur peut reellement produire. C'est la suppression qui
	// rend le cas possible : sans elle, un graphe construit en memoire porte
	// toujours 1..N dans l'ordre, et renumeroter serait l'identite.
	Graph graph;
	std::vector<NodeId> created;
	created.push_back (graph.AddNode (std::unique_ptr<Node> (new PlainSourceNode ())));
	for (int i = 0; i < 4; ++i)
		created.push_back (graph.AddNode (std::unique_ptr<Node> (new RelaxNode ())));
	for (std::size_t i = 1; i < created.size (); ++i)
		ASSERT_EQ (graph.Connect (created[i - 1], 0, created[i], 0), ConnectStatus::Ok);

	// Deux trous, non adjacents : un seul trou serait aussi produit par une
	// renumerotation qui se contenterait de decaler la queue.
	ASSERT_EQ (graph.RemoveNode (created[1]), RemoveStatus::Ok);
	ASSERT_EQ (graph.RemoveNode (created[3]), RemoveStatus::Ok);

	const std::vector<NodeId> ids = graph.GetNodeIds ();
	ASSERT_EQ (ids.size (), 3u);
	EXPECT_EQ (ids[0], created[0]);
	EXPECT_EQ (ids[1], created[2]);
	EXPECT_EQ (ids[2], created[4]);
	EXPECT_TRUE (graph.GetLinks ().empty ());

	// La suite ECRITE porte les trous : le document, et pas seulement le graphe
	// en memoire, est ce qui doit les conserver.
	const std::string saved = SaveGraph (graph);
	EXPECT_NE (saved.find ("\"id\": " + std::to_string (created[2])), std::string::npos);
	EXPECT_EQ (saved.find ("\"id\": " + std::to_string (created[1])), std::string::npos);
	EXPECT_EQ (saved.find ("\"id\": " + std::to_string (created[3])), std::string::npos);

	Graph reloaded;
	const TestFactory factory;
	ASSERT_TRUE (LoadGraph (saved, factory, reloaded).IsOk ());
	EXPECT_EQ (SaveGraph (reloaded), saved);

	const std::vector<NodeId> reloadedIds = reloaded.GetNodeIds ();
	ASSERT_EQ (reloadedIds.size (), 3u);
	for (std::size_t i = 0; i < reloadedIds.size (); ++i)
		EXPECT_EQ (reloadedIds[i], ids[i]);

	// Et le graphe relu ne comble pas davantage les trous que celui d'origine.
	const NodeId fresh = reloaded.AddNode (std::unique_ptr<Node> (new RelaxNode ()));
	EXPECT_GT (fresh, created[4]);
	EXPECT_EQ (reloaded.FindNode (created[1]), nullptr);
	EXPECT_EQ (reloaded.FindNode (created[3]), nullptr);
}

// ---------------------------------------------------------------------------
//  Visibilite
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_serialize, a_document_written_before_the_visibility_axis_still_reads)
{
	// Aucun champ `visibility` : c'est la forme de TOUS les documents deja
	// ecrits. Ils doivent se relire sans migration, et le defaut est Public --
	// un parametre qui ne dit rien se montre.
	const std::string text =
		"{ \"format\": \"cggraph\", \"formatVersion\": 1, \"links\": [],"
		" \"nodes\": [ { \"id\": 1, \"type\": \"test.relax\", \"version\": 1,"
		" \"x\": 0.0, \"y\": 0.0, \"params\": ["
		" { \"kind\": \"literal\", \"name\": \"iterations\", \"role\": \"semantic\","
		" \"type\": \"int\", \"value\": 4 } ] } ] }";

	Graph graph;
	const TestFactory factory;
	ASSERT_TRUE (LoadGraph (text, factory, graph).IsOk ());

	const Node *node = graph.FindNode (1);
	ASSERT_NE (node, nullptr);
	const ParamEntry *entry = node->GetParams ().FindEntry ("iterations");
	ASSERT_NE (entry, nullptr);
	EXPECT_EQ (entry->visibility, ParamVisibility::Public);

	// Et la re-sauvegarde ne l'alourdit pas : un axe qui s'ecrirait toujours
	// ferait grossir d'une ligne par parametre chaque document deja ecrit.
	const std::string resaved = SaveGraph (graph);
	EXPECT_EQ (resaved.find ("visibility"), std::string::npos);
}

TEST (TEST_cggraph_serialize, an_internal_parameter_says_so_in_the_document)
{
	Document document;
	const std::string text = SaveGraph (document.graph);

	// Present pour l'interne, ABSENT pour les six autres du meme noeud : sans le
	// second versant, un champ ecrit sur tout le monde passerait.
	EXPECT_NE (text.find ("\"visibility\": \"internal\""), std::string::npos);
	EXPECT_EQ (text.find ("\"visibility\": \"public\""), std::string::npos);
	EXPECT_EQ (text.find ("visibility"), text.rfind ("visibility"));

	Graph reloaded;
	const TestFactory factory;
	ASSERT_TRUE (LoadGraph (text, factory, reloaded).IsOk ());

	const Node *source = reloaded.FindNode (document.source);
	ASSERT_NE (source, nullptr);
	ASSERT_NE (source->GetParams ().FindEntry ("cache.key"), nullptr);
	EXPECT_EQ (source->GetParams ().FindEntry ("cache.key")->visibility, ParamVisibility::Internal);
}

TEST (TEST_cggraph_serialize, the_visibility_axis_stays_out_of_the_signature)
{
	// La visibilite dit qui REGLE le parametre, pas ce qu'il calcule. Deux jeux
	// qui n'en different pas indexent la meme entree de cache -- sans quoi
	// masquer un champ suffirait a tout recalculer.
	Graph graph;
	const NodeId node = graph.AddNode (std::unique_ptr<Node> (new PlainSourceNode ()));
	ASSERT_NE (node, kInvalidNodeId);

	const Hash shown = Signature (graph, node);
	graph.FindNode (node)->GetParams ().SetInt ("seed", 3, ParamRole::Semantic,
	                                            ParamVisibility::Internal);
	EXPECT_EQ (Signature (graph, node), shown);

	// Temoin : la VALEUR, elle, deplace bien la signature. Sans lui, une
	// signature constante satisferait le cas ci-dessus.
	graph.FindNode (node)->GetParams ().SetInt ("seed", 4, ParamRole::Semantic,
	                                            ParamVisibility::Internal);
	EXPECT_NE (Signature (graph, node), shown);
}

TEST (TEST_cggraph_serialize, an_unreadable_visibility_is_refused_like_the_other_enumerations)
{
	// Absent = Public, mais present et inconnu = refus. Deviner ferait entrer
	// dans le graphe un etat que personne n'a ecrit.
	const TestFactory factory;
	Graph graph;
	const LoadResult result =
		LoadGraph ("{ \"format\": \"cggraph\", \"formatVersion\": 1, \"links\": [],"
		           " \"nodes\": [ { \"id\": 1, \"type\": \"test.relax\", \"version\": 1,"
		           " \"params\": [ { \"name\": \"iterations\", \"role\": \"semantic\","
		           " \"visibility\": \"secret\", \"kind\": \"literal\", \"type\": \"int\","
		           " \"value\": 2 } ] } ] }",
		           factory, graph);
	EXPECT_EQ (result.status, SerializeStatus::BadNode);
	EXPECT_EQ (result.detail, "test.relax: param iterations");
}

// ---------------------------------------------------------------------------
//  Versionnement des descripteurs
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_serialize, an_older_node_version_loads_visible_and_refuses_to_compute)
{
	const std::string text =
		"{\n"
		"\t\"format\": \"cggraph\",\n"
		"\t\"formatVersion\": 1,\n"
		"\t\"links\": [ { \"from\": 1, \"fromPort\": 0, \"to\": 2, \"toPort\": 0 } ],\n"
		"\t\"nodes\": [\n"
		"\t\t{ \"id\": 1, \"params\": [], \"type\": \"test.v2\", \"version\": 1,"
		" \"x\": 5.0, \"y\": 6.0 },\n"
		"\t\t{ \"id\": 2, \"params\": [], \"type\": \"test.relax\", \"version\": 1,"
		" \"x\": 0.0, \"y\": 0.0 }\n"
		"\t]\n"
		"}\n";

	Graph graph;
	const TestFactory factory;
	const LoadResult result = LoadGraph (text, factory, graph);

	// VISIBLE : le chargement ne plante pas, le noeud est dans le graphe, il
	// porte sa position et son lien.
	ASSERT_TRUE (result.IsOk ()) << result.detail;
	ASSERT_EQ (result.incompatible.size (), 1u);
	EXPECT_EQ (result.incompatible[0], 1u);
	EXPECT_EQ (graph.GetNodeCount (), 2u);
	EXPECT_EQ (graph.GetLinks ().size (), 1u);
	float x = 0.0f;
	float y = 0.0f;
	ASSERT_TRUE (graph.GetNodePosition (1, x, y));
	EXPECT_FLOAT_EQ (x, 5.0f);

	// NON CALCULABLE : le refus est nomme, et il nomme le type en cause.
	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	EvalResult evaluated = evaluator.Evaluate (1, outputs, ctx);
	EXPECT_EQ (evaluated.status, EvalStatus::IncompatibleVersion);
	EXPECT_EQ (evaluated.detail, "test.v2");

	// Et il BLOQUE l'aval : un noeud a jour alimente par un noeud perime ne
	// calcule pas davantage.
	evaluated = evaluator.Evaluate (2, outputs, ctx);
	EXPECT_EQ (evaluated.status, EvalStatus::IncompatibleVersion);
	EXPECT_EQ (evaluated.node, 1u);

	// Re-sauver ne PROMEUT PAS le document : la version d'origine est conservee,
	// sans quoi un document non migre se declarerait a jour par le seul fait
	// qu'on l'a ouvert.
	const std::string resaved = SaveGraph (graph);
	Graph again;
	const LoadResult second = LoadGraph (resaved, factory, again);
	ASSERT_TRUE (second.IsOk ());
	EXPECT_EQ (second.incompatible.size (), 1u);
}

TEST (TEST_cggraph_serialize, a_node_at_its_own_version_is_compatible)
{
	// Versant symetrique : sans lui, « le graphe refuse de calculer » serait
	// satisfait par un moteur qui refuse TOUT document relu.
	Document document;
	Graph reloaded;
	const TestFactory factory;
	const LoadResult result = LoadGraph (SaveGraph (document.graph), factory, reloaded);
	ASSERT_TRUE (result.IsOk ());
	EXPECT_TRUE (result.incompatible.empty ());

	const Node *node = reloaded.FindNode (document.relax);
	ASSERT_NE (node, nullptr);
	EXPECT_TRUE (node->IsVersionCompatible ());
	EXPECT_EQ (node->GetDocumentVersion (), 1);
}

TEST (TEST_cggraph_serialize, a_reloaded_graph_computes_what_the_original_computed)
{
	// Le pilote en tete haute est le premier consommateur qui rejoue un graphe
	// SANS le contexte qui l'a produit : c'est donc le premier a reveler une
	// signature ou une relecture incompletes, par un resultat qui diverge.
	Graph original;
	const NodeId source = original.AddNode (std::unique_ptr<Node> (new PlainSourceNode ()));
	const NodeId relax = original.AddNode (std::unique_ptr<Node> (new RelaxNode ()));
	ASSERT_EQ (original.Connect (source, 0, relax, 0), ConnectStatus::Ok);
	original.FindNode (source)->GetParams ().SetInt ("seed", 5);
	original.FindNode (relax)->GetParams ().SetInt ("iterations", 3);

	Evaluator direct (original);
	EvalContext ctx;
	ValueList expected;
	ASSERT_TRUE (direct.Evaluate (relax, expected, ctx).IsOk ());
	ASSERT_EQ (expected.size (), 1u);
	const Shape *expectedShape = expected[0].Get<Shape> (ShapeType ());
	ASSERT_NE (expectedShape, nullptr);
	ASSERT_EQ (expectedShape->positions.size (), 1u);
	EXPECT_FLOAT_EQ (expectedShape->positions[0], 15.0f);

	Graph reloaded;
	const TestFactory factory;
	ASSERT_TRUE (LoadGraph (SaveGraph (original), factory, reloaded).IsOk ());

	Evaluator replayed (reloaded);
	ValueList produced;
	ASSERT_TRUE (replayed.Evaluate (relax, produced, ctx).IsOk ());
	ASSERT_EQ (produced.size (), 1u);
	const Shape *producedShape = produced[0].Get<Shape> (ShapeType ());
	ASSERT_NE (producedShape, nullptr);
	EXPECT_EQ (producedShape->positions, expectedShape->positions);

	// Et la cle du cache est la meme des deux cotes : deux resultats egaux
	// obtenus sous deux signatures differentes seraient une coincidence, pas une
	// fidelite.
	EXPECT_EQ (Signature (original, relax), Signature (reloaded, relax));
}

// ---------------------------------------------------------------------------
//  Les refus, un par un
// ---------------------------------------------------------------------------
// Chaque statut est atteint par un document qui le merite. Un statut qu'aucun
// document n'atteint serait du code mort ; un booleen ferait passer l'un de ces
// refus pour un autre.

TEST (TEST_cggraph_serialize, every_refusal_is_named)
{
	const TestFactory factory;

	{
		Graph graph;
		EXPECT_EQ (LoadGraph ("ceci n'est pas du JSON", factory, graph).status,
		           SerializeStatus::ParseError);
	}
	{
		Graph graph;
		EXPECT_EQ (LoadGraph ("{ \"format\": \"autre\" }", factory, graph).status,
		           SerializeStatus::NotAGraph);
	}
	{
		Graph graph;
		const LoadResult result = LoadGraph (
			"{ \"format\": \"cggraph\", \"formatVersion\": 99, \"nodes\": [], \"links\": [] }",
			factory, graph);
		EXPECT_EQ (result.status, SerializeStatus::UnsupportedFormat);
		EXPECT_EQ (result.detail, "99");
	}
	{
		// Le graphe d'accueil doit etre vide : fusionner deux jeux
		// d'identifiants produirait des liens faux en silence.
		Document document;
		EXPECT_EQ (LoadGraph (SaveGraph (document.graph), factory, document.graph).status,
		           SerializeStatus::GraphNotEmpty);
	}
	{
		Graph graph;
		EXPECT_EQ (LoadGraph ("{ \"format\": \"cggraph\", \"formatVersion\": 1, \"links\": [],"
		                      " \"nodes\": [ { \"id\": 1, \"type\": \"test.relax\" } ] }",
		                      factory, graph)
		               .status,
		           SerializeStatus::BadNode);
	}
	{
		Graph graph;
		const LoadResult result =
			LoadGraph ("{ \"format\": \"cggraph\", \"formatVersion\": 1, \"links\": [],"
			           " \"nodes\": [ { \"id\": 1, \"type\": \"test.absent\", \"version\": 1,"
			           " \"params\": [] } ] }",
			           factory, graph);
		EXPECT_EQ (result.status, SerializeStatus::UnknownNodeType);
		EXPECT_EQ (result.detail, "test.absent");
	}
	{
		Graph graph;
		EXPECT_EQ (
			LoadGraph ("{ \"format\": \"cggraph\", \"formatVersion\": 1, \"links\": [],"
			           " \"nodes\": [ { \"id\": 1, \"type\": \"test.relax\", \"version\": 1,"
			           " \"params\": [] },"
			           " { \"id\": 1, \"type\": \"test.relax\", \"version\": 1, \"params\": [] } ] }",
			           factory, graph)
				.status,
			SerializeStatus::DuplicateNodeId);
	}
	{
		// Un parametre dont le type annonce ne correspond pas a la valeur : le
		// document ment sur lui-meme, et la relecture ne devine pas.
		Graph graph;
		EXPECT_EQ (LoadGraph ("{ \"format\": \"cggraph\", \"formatVersion\": 1, \"links\": [],"
		                      " \"nodes\": [ { \"id\": 1, \"type\": \"test.relax\", \"version\": 1,"
		                      " \"params\": [ { \"name\": \"iterations\", \"role\": \"semantic\","
		                      " \"kind\": \"literal\", \"type\": \"int\", \"value\": \"trois\" } ] } ] }",
		                      factory, graph)
		               .status,
		           SerializeStatus::BadNode);
	}
	{
		Graph graph;
		EXPECT_EQ (LoadGraph ("{ \"format\": \"cggraph\", \"formatVersion\": 1,"
		                      " \"nodes\": [], \"links\": [ { \"from\": 1, \"fromPort\": 0,"
		                      " \"to\": 2, \"toPort\": 0 } ] }",
		                      factory, graph)
		               .status,
		           SerializeStatus::BadLink);
	}
}

TEST (TEST_cggraph_serialize, a_stale_port_index_is_refused_by_the_same_validation_as_connect)
{
	// Un document ecrit sous une version anterieure peut nommer un port qui
	// n'existe plus. La relecture passe par Connect, la MEME validation que
	// l'API publique : le chargeur n'est pas une porte derobee vers un etat que
	// l'editeur ne saurait pas produire.
	const std::string text =
		"{ \"format\": \"cggraph\", \"formatVersion\": 1,"
		" \"nodes\": [ { \"id\": 1, \"type\": \"test.plainSource\", \"version\": 1, \"params\": [] },"
		" { \"id\": 2, \"type\": \"test.relax\", \"version\": 1, \"params\": [] } ],"
		" \"links\": [ { \"from\": 1, \"fromPort\": 7, \"to\": 2, \"toPort\": 0 } ] }";

	Graph graph;
	const TestFactory factory;
	const LoadResult result = LoadGraph (text, factory, graph);
	EXPECT_EQ (result.status, SerializeStatus::BadLink);
	EXPECT_EQ (result.detail, "unknownPort");
}

TEST (TEST_cggraph_serialize, files_that_cannot_be_read_or_written_are_named)
{
	const TestFactory factory;
	Graph graph;
	const LoadResult missing = LoadGraphFromFile ("./no_such_document.json", factory, graph);
	EXPECT_EQ (missing.status, SerializeStatus::FileNotReadable);

	Document document;
	EXPECT_EQ (SaveGraphToFile (document.graph, "./no_such_directory/graph.json"),
	           SerializeStatus::FileNotWritable);

	const std::string path = TempPath ("cggraph_serialize_roundtrip.json");
	ASSERT_EQ (SaveGraphToFile (document.graph, path), SerializeStatus::Ok);

	Graph reloaded;
	ASSERT_TRUE (LoadGraphFromFile (path, factory, reloaded).IsOk ());
	EXPECT_EQ (SaveGraph (reloaded), SaveGraph (document.graph));
	std::remove (path.c_str ());
}
