#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "../src/cggraph/core/evaluator.h"
#include "../src/cggraph/core/graph.h"
#include "../src/cggraph/core/serialize.h"
#include "../src/cggraph/core/signature.h"
#include "../src/cggraph/nodes/catalog.h"
#include "../src/cggraph/nodes/catalog_factory.h"
#include "../src/cggraph/nodes/flow/boundary.h"
#include "../src/cggraph/nodes/flow/foreach.h"
#include "../src/cggraph/nodes/flow/host.h"
#include "../src/cggraph/nodes/flow/repeat.h"
#include "../src/cggraph/nodes/flow/subgraph.h"
#include "../src/cggraph/nodes/flow/subgraph_support.h"
#include "../src/cggraph/nodes/runner.h"
#include "../src/cggraph/nodes/value_types.h"
#include "../src/cgmesh/mesh.h"

// ===========================================================================
//  Etape 9 -- ForEach / Repeat / Subgraph
// ===========================================================================
// Les trois constructions qui « cassent le DAG simple ». Ce fichier tient trois
// choses, et la deuxieme est celle qui ne se voit pas :
//
//  1. qu'elles CALCULENT ce qu'elles annoncent ;
//  2. que le CACHE ne sert pas le resultat d'un element a un autre. Deux passes
//     d'un ForEach ont la meme topologie et les memes parametres : sans la cle
//     de liaison, elles ont la meme signature, et le cache interne rend le
//     resultat du premier element pour tous. Aucun plantage, aucun message ;
//  3. qu'un document delegue qui CHANGE change la signature de son hote. La
//     reference est hachee depuis l'etape 3 -- mais elle ne hache qu'un NOM.
//
// Les documents de sous-graphe ne peuvent contenir que des types du CATALOGUE :
// la relecture passe par CatalogFactory, et il n'y en a pas d'autre. Les
// montages ci-dessous emploient donc de vrais noeuds, jamais des noeuds de test
// -- contrainte, et propriete : un sous-graphe est fait de ce que le produit
// publie.

using namespace cggraph;
using namespace cggraph_nodes;

namespace {

// Grille reguliere avec une bosse au centre. La HAUTEUR de la bosse distingue
// deux elements qui ont par ailleurs la meme taille et la meme topologie : c'est
// ce qui fait qu'une collision de cache se voit sur les COORDONNEES et non
// seulement sur un compte de sommets, lequel se serait accorde par hasard.
std::shared_ptr<Mesh> MakeBumpGrid (unsigned int side, float bump)
{
	std::vector<float> vertices;
	for (unsigned int j = 0; j < side; ++j)
		for (unsigned int i = 0; i < side; ++i)
		{
			vertices.push_back (static_cast<float> (i));
			vertices.push_back (static_cast<float> (j));
			const bool inner = i > 0 && j > 0 && i + 1 < side && j + 1 < side;
			vertices.push_back (inner ? bump : 0.0f);
		}

	std::vector<unsigned int> faces;
	for (unsigned int j = 0; j + 1 < side; ++j)
		for (unsigned int i = 0; i + 1 < side; ++i)
		{
			const unsigned int a = j * side + i;
			faces.push_back (a);
			faces.push_back (a + 1);
			faces.push_back (a + side);
			faces.push_back (a + 1);
			faces.push_back (a + side + 1);
			faces.push_back (a + side);
		}

	std::shared_ptr<Mesh> mesh = std::make_shared<Mesh> ();
	mesh->SetVertices (side * side, vertices.data ());
	mesh->SetFaces (static_cast<unsigned int> (faces.size () / 3), 3, faces.data ());
	return mesh;
}

// Somme des z : une empreinte scalaire suffisante pour distinguer deux resultats
// de lissage, et insensible a l'ordre des sommets qu'aucun noeud ne change ici.
double SumZ (const Mesh &mesh)
{
	const std::vector<float> &v = mesh.GetVertices ();
	double total = 0.0;
	for (std::size_t i = 2; i < v.size (); i += 3)
		total += v[i];
	return total;
}

// Source de test dans le graphe EXTERIEUR. Le graphe exterieur se construit en
// code, donc n'importe quel Node y est admis -- contrairement aux documents.
class ValueSourceNode : public Node
{
public:
	ValueSourceNode (const char *typeName, const TypeDesc *type, Value value)
		: m_value (std::move (value))
	{
		m_desc.typeName = typeName;
		m_desc.outputs.push_back ({ "sortie", type, false });
	}

	const NodeDesc &GetDesc () const override { return m_desc; }
	bool Compute (EvalContext &, const ValueList &, ValueList &out) override
	{
		out[0] = m_value;
		return true;
	}

private:
	NodeDesc m_desc;
	Value m_value;
};

// Consommateur d'un maillage : il ne sert qu'a porter un port d'entree que
// l'evaluateur devra alimenter, donc a faire remonter un refus NOMME.
class MeshProbeNode : public Node
{
public:
	MeshProbeNode ()
	{
		m_desc.typeName = "test.probe";
		m_desc.inputs.push_back ({ "attendu", Types ().mesh, false });
		m_desc.outputs.push_back ({ "vu", Types ().mesh, false });
	}

	const NodeDesc &GetDesc () const override { return m_desc; }
	bool Compute (EvalContext &, const ValueList &in, ValueList &out) override
	{
		out[0] = in[0];
		return true;
	}

private:
	NodeDesc m_desc;
};

Value MakeArray (const std::vector<std::shared_ptr<const Mesh>> &items)
{
	std::shared_ptr<MeshArray> array = std::make_shared<MeshArray> ();
	array->items = items;
	return Value::Make (Types ().meshArray, array);
}

std::string GetStringParam (const Node &node, const char *name)
{
	const ParamValue *value = node.GetParams ().Find (name);
	return value == nullptr ? std::string ("<absent>") : value->stringValue;
}

void WriteDocument (const std::string &path, const Graph &graph)
{
	ASSERT_EQ (SaveGraphToFile (graph, path), SerializeStatus::Ok) << path;
}

// Document : flow.in(nomIn) -> lissage -> flow.out(nomOut). Le coeur de tous
// les montages -- une transformation qui DEPEND de son entree, donc dont une
// collision de cache se voit.
void WriteSmoothDocument (const std::string &path, const char *nomIn, const char *nomOut,
                          int iterations = 1)
{
	Graph graph;
	const NodeId in = graph.AddNode (MakeNode (flow::kInputTypeName));
	const NodeId smooth = graph.AddNode (MakeNode ("mesh.smooth.laplacian"));
	const NodeId out = graph.AddNode (MakeNode (flow::kOutputTypeName));
	ASSERT_NE (in, kInvalidNodeId);
	ASSERT_NE (smooth, kInvalidNodeId);
	ASSERT_NE (out, kInvalidNodeId);

	graph.FindNode (in)->GetParams ().SetString ("nom", nomIn);
	graph.FindNode (out)->GetParams ().SetString ("nom", nomOut);
	graph.FindNode (smooth)->GetParams ().SetInt ("iterations", iterations);

	ASSERT_EQ (graph.Connect (in, 0, smooth, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (smooth, 0, out, 0), ConnectStatus::Ok);
	WriteDocument (path, graph);
}

// Le meme, plus une SECONDE sortie alimentee par un generateur qui ne depend
// d'AUCUNE entree. C'est ce montage qui rend le cache interne observable : la
// branche constante doit se calculer une fois et servir n fois, ce que
// l'egalite des ADRESSES des valeurs rendues etablit.
void WriteSmoothAndConstantDocument (const std::string &path)
{
	Graph graph;
	const NodeId in = graph.AddNode (MakeNode (flow::kInputTypeName));
	const NodeId smooth = graph.AddNode (MakeNode ("mesh.smooth.laplacian"));
	const NodeId out0 = graph.AddNode (MakeNode (flow::kOutputTypeName));
	const NodeId cube = graph.AddNode (MakeNode ("shape.cube"));
	const NodeId out1 = graph.AddNode (MakeNode (flow::kOutputTypeName));
	ASSERT_NE (cube, kInvalidNodeId);

	graph.FindNode (in)->GetParams ().SetString ("nom", "piece");
	graph.FindNode (out0)->GetParams ().SetInt ("slot", 0);
	graph.FindNode (out0)->GetParams ().SetString ("nom", "lisse");
	graph.FindNode (out1)->GetParams ().SetInt ("slot", 1);
	graph.FindNode (out1)->GetParams ().SetString ("nom", "constante");

	ASSERT_EQ (graph.Connect (in, 0, smooth, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (smooth, 0, out0, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (cube, 0, out1, 0), ConnectStatus::Ok);
	WriteDocument (path, graph);
}

// Document a PUITS : le lissage part aussi vers un enregistrement. Le nom du
// fichier porte le jeton {hash}, donc il derive du CONTENU ecrit.
void WriteSinkDocument (const std::string &path, const std::string &target)
{
	Graph graph;
	const NodeId in = graph.AddNode (MakeNode (flow::kInputTypeName));
	const NodeId smooth = graph.AddNode (MakeNode ("mesh.smooth.laplacian"));
	const NodeId out = graph.AddNode (MakeNode (flow::kOutputTypeName));
	const NodeId save = graph.AddNode (MakeNode ("mesh.io.save"));
	graph.FindNode (in)->GetParams ().SetString ("nom", "piece");
	graph.FindNode (out)->GetParams ().SetString ("nom", "lisse");
	graph.FindNode (save)->GetParams ().SetString ("path", target);

	ASSERT_EQ (graph.Connect (in, 0, smooth, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (smooth, 0, out, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (smooth, 0, save, 0), ConnectStatus::Ok);
	WriteDocument (path, graph);
}

std::vector<std::string> FilesWithPrefix (const std::string &prefix)
{
	std::vector<std::string> found;
	for (const std::filesystem::directory_entry &entry :
	     std::filesystem::directory_iterator (std::filesystem::current_path ()))
	{
		const std::string name = entry.path ().filename ().string ();
		if (name.rfind (prefix, 0) == 0)
			found.push_back (name);
	}
	std::sort (found.begin (), found.end ());
	return found;
}

void RemoveFilesWithPrefix (const std::string &prefix)
{
	for (const std::string &name : FilesWithPrefix (prefix))
		std::filesystem::remove (name);
}

// Un OBJ multi-objets : `count` tetraedres, chacun dans son propre objet. C'est
// la seule source de SUITE du catalogue, et c'est le cas d'usage que le corpus
// nomme -- une decomposition dont les pieces divergent.
void WriteMultiObjectObj (const std::string &path, int count)
{
	std::ofstream file (path);
	ASSERT_TRUE (file.is_open ());
	int base = 1;
	for (int k = 0; k < count; ++k)
	{
		const float s = static_cast<float> (k + 1);
		const float x = 10.0f * static_cast<float> (k);
		file << "o piece_" << k << "\n";
		file << "v " << x << " 0 0\n";
		file << "v " << x + s << " 0 0\n";
		file << "v " << x << " " << s << " 0\n";
		file << "v " << x << " 0 " << s << "\n";
		file << "f " << base << " " << base + 2 << " " << base + 1 << "\n";
		file << "f " << base << " " << base + 1 << " " << base + 3 << "\n";
		file << "f " << base + 1 << " " << base + 2 << " " << base + 3 << "\n";
		file << "f " << base << " " << base + 3 << " " << base + 2 << "\n";
		base += 4;
	}
}

} // namespace

// ===========================================================================
//  flow.subgraph -- la delegation, et ses ports
// ===========================================================================

TEST (TEST_cggraph_flow_subgraph, the_ports_of_the_host_are_those_the_document_declares)
{
	WriteSmoothDocument ("flow_ports.json", "piece", "lisse");

	flow::SubgraphNode node;
	EXPECT_EQ (node.GetDesc ().inputs.size (), 0u);
	EXPECT_EQ (node.GetDesc ().outputs.size (), 0u);

	node.SetSubgraphReference ("flow_ports.json");
	ASSERT_EQ (node.GetDesc ().inputs.size (), 1u);
	ASSERT_EQ (node.GetDesc ().outputs.size (), 1u);
	// Le NOM vient du document, pas du type de noeud. Un hote qui publierait un
	// port fixe passerait le compte et echouerait ici.
	EXPECT_EQ (node.GetDesc ().inputs[0].name, "piece");
	EXPECT_EQ (node.GetDesc ().outputs[0].name, "lisse");
	EXPECT_EQ (node.GetDesc ().inputs[0].type, Types ().mesh);
}

TEST (TEST_cggraph_flow_subgraph, the_slot_parameter_orders_the_ports_and_not_the_node_order)
{
	// Les deux frontieres sont ajoutees dans l'ordre 0 puis 1, mais leurs slots
	// disent l'inverse. Si l'hote publiait dans l'ordre du graphe, ce cas
	// passerait sans rien etablir : le montage est IRREGULIER la ou le mecanisme
	// porte sur une regularite.
	Graph graph;
	const NodeId first = graph.AddNode (MakeNode (flow::kOutputTypeName));
	const NodeId second = graph.AddNode (MakeNode (flow::kOutputTypeName));
	const NodeId cubeA = graph.AddNode (MakeNode ("shape.cube"));
	const NodeId cubeB = graph.AddNode (MakeNode ("shape.sphere"));
	graph.FindNode (first)->GetParams ().SetInt ("slot", 7);
	graph.FindNode (first)->GetParams ().SetString ("nom", "tard");
	graph.FindNode (second)->GetParams ().SetInt ("slot", 2);
	graph.FindNode (second)->GetParams ().SetString ("nom", "tot");
	ASSERT_EQ (graph.Connect (cubeA, 0, first, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (cubeB, 0, second, 0), ConnectStatus::Ok);
	WriteDocument ("flow_slots.json", graph);

	flow::SubgraphNode node;
	node.SetSubgraphReference ("flow_slots.json");
	ASSERT_EQ (node.GetDesc ().outputs.size (), 2u);
	EXPECT_EQ (node.GetDesc ().outputs[0].name, "tot");
	EXPECT_EQ (node.GetDesc ().outputs[1].name, "tard");
}

TEST (TEST_cggraph_flow_subgraph, a_reference_that_leads_nowhere_leaves_a_visible_node_without_ports)
{
	flow::SubgraphNode node;
	node.SetSubgraphReference ("flow_does_not_exist.json");
	EXPECT_EQ (node.GetDesc ().inputs.size (), 0u);
	EXPECT_EQ (node.GetDesc ().outputs.size (), 0u);
	// Le refus est NOMME, comme partout ailleurs dans ce moteur.
	EXPECT_NE (node.GetLastInnerDetail ().find ("fileNotReadable"), std::string::npos)
		<< node.GetLastInnerDetail ();
}

TEST (TEST_cggraph_flow_subgraph, the_subgraph_computes_what_the_same_chain_computes_inline)
{
	WriteSmoothDocument ("flow_equiv.json", "piece", "lisse", 2);

	const std::shared_ptr<Mesh> source = MakeBumpGrid (5, 3.0f);

	// Reference : la meme chaine, a plat.
	Graph flat;
	const NodeId flatSrc = flat.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.mesh", Types ().mesh, Value::Make (Types ().mesh, source))));
	const NodeId flatSmooth = flat.AddNode (MakeNode ("mesh.smooth.laplacian"));
	flat.FindNode (flatSmooth)->GetParams ().SetInt ("iterations", 2);
	ASSERT_EQ (flat.Connect (flatSrc, 0, flatSmooth, 0), ConnectStatus::Ok);

	Evaluator flatEval (flat);
	EvalContext ctx;
	ValueList flatOut;
	ASSERT_EQ (flatEval.Evaluate (flatSmooth, flatOut, ctx).status, EvalStatus::Ok);
	ASSERT_EQ (flatOut.size (), 1u);
	const Mesh *expected = flatOut[0].Get<Mesh> (Types ().mesh);
	ASSERT_NE (expected, nullptr);

	// Le meme calcul, delegue.
	Graph outer;
	const NodeId src = outer.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.mesh", Types ().mesh, Value::Make (Types ().mesh, source))));
	const NodeId host = outer.AddNode (std::unique_ptr<Node> (new flow::SubgraphNode ()));
	ASSERT_TRUE (outer.SetNodeSubgraph (host, "flow_equiv.json"));
	ASSERT_EQ (outer.Connect (src, 0, host, 0), ConnectStatus::Ok);

	Evaluator eval (outer);
	ValueList out;
	ASSERT_EQ (eval.Evaluate (host, out, ctx).status, EvalStatus::Ok);
	ASSERT_EQ (out.size (), 1u);
	const Mesh *got = out[0].Get<Mesh> (Types ().mesh);
	ASSERT_NE (got, nullptr);

	EXPECT_EQ (got->GetNVertices (), expected->GetNVertices ());
	EXPECT_NEAR (SumZ (*got), SumZ (*expected), 1e-4);
	// Et le lissage a REELLEMENT eu lieu : sans cela les deux cotes seraient
	// egaux a l'entree, et l'egalite ci-dessus ne dirait rien.
	EXPECT_GT (std::abs (SumZ (*got) - SumZ (*source)), 1e-3);
}

// ===========================================================================
//  Le champ "subgraph" de l'etape 3 -- critere 9.6
// ===========================================================================

TEST (TEST_cggraph_flow_subgraph, the_document_field_of_step_three_drives_the_node_without_migration)
{
	WriteSmoothDocument ("flow_ref.json", "piece", "lisse");

	Graph graph;
	const NodeId host = graph.AddNode (std::unique_ptr<Node> (new flow::SubgraphNode ()));
	ASSERT_TRUE (graph.SetNodeSubgraph (host, "flow_ref.json"));
	const std::string document = SaveGraph (graph);

	// Le champ existe DANS LE FORMAT depuis l'etape 3, et il n'a pas fallu le
	// migrer : la version de format n'a pas bouge.
	EXPECT_NE (document.find ("\"subgraph\""), std::string::npos);
	EXPECT_NE (document.find ("\"formatVersion\": 1"), std::string::npos);

	Graph reloaded;
	const CatalogFactory factory;
	const LoadResult result = LoadGraph (document, factory, reloaded);
	ASSERT_TRUE (result.IsOk ());
	EXPECT_EQ (reloaded.GetNodeSubgraph (host), "flow_ref.json");

	// Et la relecture a REELLEMENT arme le noeud : ses ports viennent du
	// document delegue. Sans la transmission au noeud, la reference serait
	// relue, re-sauvee, et decorative.
	const Node *node = reloaded.FindNode (host);
	ASSERT_NE (node, nullptr);
	ASSERT_EQ (node->GetDesc ().inputs.size (), 1u);
	EXPECT_EQ (node->GetDesc ().inputs[0].name, "piece");
}

TEST (TEST_cggraph_flow_subgraph, the_reference_and_the_content_both_move_the_signature)
{
	WriteSmoothDocument ("flow_sigA.json", "piece", "lisse", 1);

	Graph graph;
	const NodeId host = graph.AddNode (std::unique_ptr<Node> (new flow::SubgraphNode ()));
	Evaluator eval (graph);

	const Hash bare = eval.GetSignature (host);
	ASSERT_TRUE (graph.SetNodeSubgraph (host, "flow_sigA.json"));
	// Volet acquis a l'etape 3 : la reference elle-meme est hachee.
	const Hash named = eval.GetSignature (host);
	EXPECT_NE (bare, named);

	// Volet de CETTE etape, et c'est le risque de gravite elevee revenu par une
	// porte neuve : le NOM ne bouge pas, le CONTENU change. Sans releve de
	// l'etat exterieur, le cache resservirait l'ancien resultat sans planter.
	graph.FindNode (host)->RefreshExternalState ();
	const Hash before = eval.GetSignature (host);

	WriteSmoothDocument ("flow_sigA.json", "piece", "lisse", 5);
	graph.FindNode (host)->RefreshExternalState ();
	const Hash after = eval.GetSignature (host);
	EXPECT_NE (before, after);
}

TEST (TEST_cggraph_flow_subgraph, an_edited_document_recomputes_instead_of_being_served_by_the_cache)
{
	WriteSmoothDocument ("flow_edit.json", "piece", "lisse", 1);

	const std::shared_ptr<Mesh> source = MakeBumpGrid (5, 4.0f);
	Graph graph;
	const NodeId src = graph.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.mesh", Types ().mesh, Value::Make (Types ().mesh, source))));
	const NodeId host = graph.AddNode (std::unique_ptr<Node> (new flow::SubgraphNode ()));
	ASSERT_TRUE (graph.SetNodeSubgraph (host, "flow_edit.json"));
	ASSERT_EQ (graph.Connect (src, 0, host, 0), ConnectStatus::Ok);

	Evaluator eval (graph);
	EvalContext ctx;
	ValueList first;
	ASSERT_EQ (eval.Evaluate (host, first, ctx).status, EvalStatus::Ok);
	const double one = SumZ (*first[0].Get<Mesh> (Types ().mesh));

	// Meme nom de fichier, contenu different. mtime + taille suffisent : le
	// document passe de 1 a 12 iterations, donc sa taille change.
	WriteSmoothDocument ("flow_edit.json", "piece", "lisse", 12);

	ValueList second;
	ASSERT_EQ (eval.Evaluate (host, second, ctx).status, EvalStatus::Ok);
	const double twelve = SumZ (*second[0].Get<Mesh> (Types ().mesh));

	EXPECT_GT (std::abs (one - twelve), 1e-3)
		<< "le cache a resservi le resultat de l'ancien document";
}

// ===========================================================================
//  Le second espace de cycles -- celui que Connect ne voit pas
// ===========================================================================

TEST (TEST_cggraph_flow_subgraph, a_document_that_references_itself_is_refused_by_name)
{
	// Il n'y a AUCUNE arete ici : Graph::Connect n'a rien a refuser. Le cycle est
	// dans l'espace des DOCUMENTS, ouvert par cette etape. Il s'effondrerait des
	// le CHARGEMENT -- charger construit le noeud, qui apprend sa reference, qui
	// charge --, donc la garde est autour du chargement.
	Graph graph;
	const NodeId host = graph.AddNode (std::unique_ptr<Node> (new flow::SubgraphNode ()));
	ASSERT_TRUE (graph.SetNodeSubgraph (host, "flow_self.json"));
	WriteDocument ("flow_self.json", graph);

	flow::SubgraphInstance instance;
	std::string detail;
	// Que ce cas se TERMINE est la moitie de ce qu'il etablit.
	ASSERT_EQ (flow::LoadSubgraph ("flow_self.json", instance, detail), flow::LoadStatus::Ok);

	const std::vector<NodeId> ids = instance.graph.GetNodeIds ();
	ASSERT_EQ (ids.size (), 1u);
	const flow::SubgraphHostNode *inner =
		dynamic_cast<const flow::SubgraphHostNode *> (instance.graph.FindNode (ids[0]));
	ASSERT_NE (inner, nullptr);
	EXPECT_NE (inner->GetLastInnerDetail ().find ("recursive"), std::string::npos)
		<< inner->GetLastInnerDetail ();
	EXPECT_EQ (inner->GetDesc ().outputs.size (), 0u);
}

TEST (TEST_cggraph_flow_subgraph, two_documents_that_reference_each_other_make_the_pair_unloadable)
{
	// Un cycle a DEUX documents ne se refuse pas au meme endroit qu'un cycle a
	// un seul, et c'est ce qui a corrige la premiere redaction de ce cas : la
	// garde mord DEUX crans sous l'appel, dans une instance intermediaire que
	// personne ne conserve. Ce qui est observable est la CONSEQUENCE, et elle
	// est la bonne -- celle qu'un utilisateur voit : le noeud intermediaire
	// n'ayant plus de port, le lien que son document porte n'est plus posable,
	// et le document entier est refuse par la meme validation que Connect.
	//
	// Le montage exige un AMORCAGE : un document ne peut se referer qu'a un
	// document qui existe deja, puisque le lien interne ne se pose que si le
	// port existe. Le cycle se ferme donc par REECRITURE du premier.
	{
		Graph leaf;
		const NodeId in = leaf.AddNode (MakeNode (flow::kInputTypeName));
		const NodeId out = leaf.AddNode (MakeNode (flow::kOutputTypeName));
		ASSERT_EQ (leaf.Connect (in, 0, out, 0), ConnectStatus::Ok);
		WriteDocument ("flow_cyc_leaf.json", leaf);
	}
	// A, premiere version : pointe sur la feuille.
	{
		Graph a;
		const NodeId host = a.AddNode (std::unique_ptr<Node> (new flow::SubgraphNode ()));
		const NodeId out = a.AddNode (MakeNode (flow::kOutputTypeName));
		ASSERT_TRUE (a.SetNodeSubgraph (host, "flow_cyc_leaf.json"));
		ASSERT_EQ (a.Connect (host, 0, out, 0), ConnectStatus::Ok);
		WriteDocument ("flow_cyc_a.json", a);
	}
	// B pointe sur A.
	{
		Graph b;
		const NodeId host = b.AddNode (std::unique_ptr<Node> (new flow::SubgraphNode ()));
		const NodeId out = b.AddNode (MakeNode (flow::kOutputTypeName));
		ASSERT_TRUE (b.SetNodeSubgraph (host, "flow_cyc_a.json"));
		ASSERT_EQ (b.Connect (host, 0, out, 0), ConnectStatus::Ok);
		WriteDocument ("flow_cyc_b.json", b);
	}
	// A est REECRIT et pointe desormais sur B : le cycle est ferme sur disque.
	{
		Graph a;
		const NodeId host = a.AddNode (std::unique_ptr<Node> (new flow::SubgraphNode ()));
		const NodeId out = a.AddNode (MakeNode (flow::kOutputTypeName));
		ASSERT_TRUE (a.SetNodeSubgraph (host, "flow_cyc_b.json"));
		ASSERT_EQ (a.Connect (host, 0, out, 0), ConnectStatus::Ok);
		WriteDocument ("flow_cyc_a.json", a);
	}

	flow::SubgraphInstance instance;
	std::string detail;
	// Que ce cas se TERMINE est la moitie de ce qu'il etablit.
	EXPECT_EQ (flow::LoadSubgraph ("flow_cyc_a.json", instance, detail),
	           flow::LoadStatus::DocumentInvalid);
	EXPECT_NE (detail.find ("unknownPort"), std::string::npos) << detail;
}

TEST (TEST_cggraph_flow_subgraph, the_depth_cap_refuses_exactly_one_level_beyond_the_maximum)
{
	// La garde exacte -- un chemin canonique deja sur la pile -- ne reconnait pas
	// deux ORTHOGRAPHES du meme fichier. Le plafond de profondeur est le second
	// filet, et il se mesure : une chaine de kMaxSubgraphDepth documents
	// imbriques se charge, une de plus ne se charge pas.
	//
	// Le montage est construit du BAS vers le HAUT, et il ne peut pas l'etre
	// autrement : le lien interne d'un niveau ne se pose que si le niveau
	// inferieur a publie son port, donc si le niveau inferieur existe deja.
	const std::string prefix = "flow_chain_";
	RemoveFilesWithPrefix (prefix);

	{
		Graph leaf;
		const NodeId in = leaf.AddNode (MakeNode (flow::kInputTypeName));
		const NodeId out = leaf.AddNode (MakeNode (flow::kOutputTypeName));
		ASSERT_EQ (leaf.Connect (in, 0, out, 0), ConnectStatus::Ok);
		WriteDocument (prefix + "0.json", leaf);
	}

	// ⚠ LA BORNE DE CETTE BOUCLE EST UN LITTERAL, et ce n'est pas un detail. Sa
	// premiere redaction bouclait jusqu'a kMaxSubgraphDepth + 2 et comparait a
	// kMaxSubgraphDepth : elle etait INVARIANTE par tout changement de la
	// constante -- porter le plafond a 64 la laissait VERTE. Un filet ecrit dans
	// les termes de ce qu'il surveille ne surveille rien (sabotage S9).
	const std::size_t probe = 20;
	std::size_t deepest = 0;
	for (std::size_t level = 1; level <= probe; ++level)
	{
		Graph graph;
		const NodeId host = graph.AddNode (std::unique_ptr<Node> (new flow::SubgraphNode ()));
		const NodeId out = graph.AddNode (MakeNode (flow::kOutputTypeName));
		ASSERT_TRUE (
			graph.SetNodeSubgraph (host, prefix + std::to_string (level - 1) + ".json"));

		// Le port n'existe que si le niveau inferieur s'est charge. Des que le
		// plafond mord, il n'y a plus rien a connecter.
		if (graph.FindNode (host)->GetDesc ().outputs.empty ())
			break;
		ASSERT_EQ (graph.Connect (host, 0, out, 0), ConnectStatus::Ok) << level;
		WriteDocument (prefix + std::to_string (level) + ".json", graph);
		deepest = level;
	}

	// Le plafond compte les APPELS empiles : charger le niveau `deepest` depuis
	// une pile vide en empile exactement `deepest`. Les deux assertions sont
	// necessaires -- la premiere fige la valeur DECLAREE, la seconde mesure la
	// valeur EFFECTIVE, et un plafond qui ne mordrait pas les separerait.
	EXPECT_EQ (flow::kMaxSubgraphDepth, 16u);
	EXPECT_EQ (deepest, 16u);
	ASSERT_LT (deepest, probe) << "le plafond n'a pas mordu";
}

// ===========================================================================
//  La branche defensive de l'evaluateur -- la porte que le plan lui predisait
// ===========================================================================

TEST (TEST_cggraph_flow_subgraph, a_link_to_a_port_the_shrunk_document_no_longer_publishes_is_refused_by_name)
{
	// Le paragraphe 6-C de l'etape 1 annoncait cette branche « inatteignable par
	// l'API publique », et lui predisait deux portes : un descripteur relu d'une
	// version anterieure -- porte FERMEE a l'etape 3 -- et un sous-graphe. Voici
	// la seconde, et elle est ouverte : les ports d'un hote viennent d'un
	// DOCUMENT, donc ils peuvent RETRECIR apres que les liens ont ete poses.
	{
		Graph two;
		const NodeId cubeA = two.AddNode (MakeNode ("shape.cube"));
		const NodeId cubeB = two.AddNode (MakeNode ("shape.sphere"));
		const NodeId out0 = two.AddNode (MakeNode (flow::kOutputTypeName));
		const NodeId out1 = two.AddNode (MakeNode (flow::kOutputTypeName));
		two.FindNode (out0)->GetParams ().SetInt ("slot", 0);
		two.FindNode (out1)->GetParams ().SetInt ("slot", 1);
		ASSERT_EQ (two.Connect (cubeA, 0, out0, 0), ConnectStatus::Ok);
		ASSERT_EQ (two.Connect (cubeB, 0, out1, 0), ConnectStatus::Ok);
		WriteDocument ("flow_two_outputs.json", two);
	}
	{
		Graph one;
		const NodeId cube = one.AddNode (MakeNode ("shape.cube"));
		const NodeId out0 = one.AddNode (MakeNode (flow::kOutputTypeName));
		ASSERT_EQ (one.Connect (cube, 0, out0, 0), ConnectStatus::Ok);
		WriteDocument ("flow_one_output.json", one);
	}

	Graph graph;
	const NodeId host = graph.AddNode (std::unique_ptr<Node> (new flow::SubgraphNode ()));
	const NodeId probe = graph.AddNode (std::unique_ptr<Node> (new MeshProbeNode ()));
	ASSERT_TRUE (graph.SetNodeSubgraph (host, "flow_two_outputs.json"));
	ASSERT_EQ (graph.FindNode (host)->GetDesc ().outputs.size (), 2u);
	ASSERT_EQ (graph.Connect (host, 1, probe, 0), ConnectStatus::Ok);

	Evaluator eval (graph);
	EvalContext ctx;
	ValueList out;
	ASSERT_EQ (eval.Evaluate (probe, out, ctx).status, EvalStatus::Ok);

	// Le document RETRECIT sous le lien deja pose.
	ASSERT_TRUE (graph.SetNodeSubgraph (host, "flow_one_output.json"));
	ASSERT_EQ (graph.FindNode (host)->GetDesc ().outputs.size (), 1u);

	const EvalResult result = eval.Evaluate (probe, out, ctx);
	EXPECT_EQ (result.status, EvalStatus::MissingInput);
	EXPECT_EQ (result.node, probe);
	EXPECT_EQ (result.detail, "attendu");
}

// ===========================================================================
//  L'annulation traverse la frontiere
// ===========================================================================

TEST (TEST_cggraph_flow_subgraph, an_abort_traverses_the_subgraph_boundary)
{
	WriteSmoothDocument ("flow_abort.json", "piece", "lisse");

	flow::SubgraphNode node;
	node.SetSubgraphReference ("flow_abort.json");
	ASSERT_EQ (node.GetDesc ().inputs.size (), 1u);

	std::atomic<bool> cancelled{ true };
	EvalContext ctx;
	ctx.SetCancellationFlag (&cancelled);

	// Compute est appele DIRECTEMENT : passer par un evaluateur exterieur ferait
	// refuser le noeud avant meme d'entrer dans le nid, et le cas n'etablirait
	// rien sur la traversee.
	ValueList in (1);
	in[0] = Value::Make (Types ().mesh, MakeBumpGrid (4, 1.0f));
	ValueList out (1);
	EXPECT_FALSE (node.Compute (ctx, in, out));
	// Le statut NOMME de l'evaluateur interne est republie : sans ce canal,
	// Node::Compute rendant un booleen, l'exterieur ne verrait qu'un echec muet.
	EXPECT_EQ (node.GetLastInnerStatus (), EvalStatus::Aborted);
	EXPECT_EQ (node.GetPassCount (), 1u);
}

// ===========================================================================
//  flow.foreach -- la divergence
// ===========================================================================

TEST (TEST_cggraph_flow_foreach, every_element_gets_its_own_result)
{
	// LE cas de cette etape. Les trois elements ont la MEME topologie et le MEME
	// nombre de sommets : seules leurs coordonnees different. Une collision de
	// cache -- le resultat du premier element servi aux trois -- ne se verrait
	// donc PAS sur un compte de sommets, elle se voit ici.
	WriteSmoothDocument ("flow_each.json", "pieces", "lissees", 2);

	std::vector<std::shared_ptr<const Mesh>> items;
	std::vector<double> expected;
	for (int k = 0; k < 3; ++k)
	{
		const std::shared_ptr<Mesh> grid = MakeBumpGrid (5, 1.0f + static_cast<float> (k));
		items.push_back (grid);

		// Reference : le meme lissage, a plat.
		Graph flat;
		const NodeId src = flat.AddNode (std::unique_ptr<Node> (
			new ValueSourceNode ("test.mesh", Types ().mesh, Value::Make (Types ().mesh, grid))));
		const NodeId smooth = flat.AddNode (MakeNode ("mesh.smooth.laplacian"));
		flat.FindNode (smooth)->GetParams ().SetInt ("iterations", 2);
		ASSERT_EQ (flat.Connect (src, 0, smooth, 0), ConnectStatus::Ok);
		Evaluator flatEval (flat);
		EvalContext flatCtx;
		ValueList flatOut;
		ASSERT_EQ (flatEval.Evaluate (smooth, flatOut, flatCtx).status, EvalStatus::Ok);
		expected.push_back (SumZ (*flatOut[0].Get<Mesh> (Types ().mesh)));
	}
	// Les trois references sont distinctes : sans cela, l'egalite ci-dessous
	// serait satisfaite par un ForEach qui recopierait le premier resultat.
	ASSERT_GT (std::abs (expected[0] - expected[1]), 1e-3);
	ASSERT_GT (std::abs (expected[1] - expected[2]), 1e-3);

	Graph graph;
	const NodeId src = graph.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.array", Types ().meshArray, MakeArray (items))));
	const NodeId host = graph.AddNode (std::unique_ptr<Node> (new flow::ForEachNode ()));
	ASSERT_TRUE (graph.SetNodeSubgraph (host, "flow_each.json"));
	ASSERT_EQ (graph.FindNode (host)->GetDesc ().inputs.size (), 1u);
	EXPECT_EQ (graph.FindNode (host)->GetDesc ().inputs[0].type, Types ().meshArray);
	ASSERT_EQ (graph.Connect (src, 0, host, 0), ConnectStatus::Ok);

	Evaluator eval (graph);
	EvalContext ctx;
	ValueList out;
	ASSERT_EQ (eval.Evaluate (host, out, ctx).status, EvalStatus::Ok);
	ASSERT_EQ (out.size (), 1u);
	const MeshArray *result = out[0].Get<MeshArray> (Types ().meshArray);
	ASSERT_NE (result, nullptr);
	ASSERT_EQ (result->items.size (), 3u);
	for (std::size_t k = 0; k < 3; ++k)
		EXPECT_NEAR (SumZ (*result->items[k]), expected[k], 1e-4) << "element " << k;
}

TEST (TEST_cggraph_flow_foreach, each_element_costs_one_pass)
{
	// La moitie MESURABLE du critere 9.5. Un ForEach sur n elements coute n
	// evaluations au regard de la signature ; c'est ce cout qui interdit de s'en
	// servir pour repeter un meme sous-calcul -- la boucle reste alors DANS le
	// noeud, comme la memoisation par glyphe de l'extrusion de texte.
	WriteSmoothDocument ("flow_cost.json", "pieces", "lissees");

	flow::ForEachNode node;
	node.SetSubgraphReference ("flow_cost.json");
	EXPECT_EQ (node.GetPassCount (), 0u);

	std::vector<std::shared_ptr<const Mesh>> items;
	for (int k = 0; k < 4; ++k)
		items.push_back (MakeBumpGrid (4, 1.0f + static_cast<float> (k)));

	EvalContext ctx;
	ValueList in (1);
	in[0] = MakeArray (items);
	ValueList out (1);
	ASSERT_TRUE (node.Compute (ctx, in, out));
	EXPECT_EQ (node.GetPassCount (), 4u);
}

TEST (TEST_cggraph_flow_foreach, two_identical_elements_still_cost_two_passes)
{
	// Le versant qui rend le critere 9.5 falsifiable plutot qu'exhortatif : deux
	// elements IDENTIQUES -- le meme objet, partage -- coutent quand meme deux
	// passes, parce que la cle de liaison est l'INDICE. C'est exactement
	// l'economie que le corpus dit qu'un ForEach detruit, et la voici, mesuree.
	// La recuperer exigerait un cache indexe par CONTENU d'element, c'est-a-dire
	// un autre moteur.
	WriteSmoothDocument ("flow_twin.json", "pieces", "lissees");

	flow::ForEachNode node;
	node.SetSubgraphReference ("flow_twin.json");

	const std::shared_ptr<const Mesh> shared = MakeBumpGrid (4, 2.0f);
	const std::vector<std::shared_ptr<const Mesh>> items = { shared, shared };

	EvalContext ctx;
	ValueList in (1);
	in[0] = MakeArray (items);
	ValueList out (1);
	ASSERT_TRUE (node.Compute (ctx, in, out));
	EXPECT_EQ (node.GetPassCount (), 2u);

	const MeshArray *result = out[0].Get<MeshArray> (Types ().meshArray);
	ASSERT_NE (result, nullptr);
	ASSERT_EQ (result->items.size (), 2u);
	// Deux calculs, deux objets DISTINCTS au meme contenu. Une egalite d'adresse
	// signalerait que le cache a servi, donc que la cle de liaison ne mord pas.
	EXPECT_NE (result->items[0].get (), result->items[1].get ());
	EXPECT_NEAR (SumZ (*result->items[0]), SumZ (*result->items[1]), 1e-6);
}

TEST (TEST_cggraph_flow_foreach, a_branch_that_ignores_the_element_is_computed_once_for_all_of_them)
{
	// L'autre moitie du cache interne, et celle qui justifie qu'il existe : une
	// branche du document qui ne depend PAS de l'element garde la meme signature
	// d'une passe a l'autre, donc se calcule une fois. L'instrument est
	// l'ADRESSE : trois entrees egales ne prouveraient rien -- un recalcul rend
	// le meme contenu --, une meme adresse prouve que rien n'a ete recalcule.
	WriteSmoothAndConstantDocument ("flow_shared.json");

	flow::ForEachNode node;
	node.SetSubgraphReference ("flow_shared.json");
	ASSERT_EQ (node.GetDesc ().outputs.size (), 2u);
	EXPECT_EQ (node.GetDesc ().outputs[0].name, "lisse");
	EXPECT_EQ (node.GetDesc ().outputs[1].name, "constante");

	std::vector<std::shared_ptr<const Mesh>> items;
	for (int k = 0; k < 3; ++k)
		items.push_back (MakeBumpGrid (4, 1.0f + static_cast<float> (k)));

	EvalContext ctx;
	ValueList in (1);
	in[0] = MakeArray (items);
	ValueList out (2);
	ASSERT_TRUE (node.Compute (ctx, in, out));

	const MeshArray *variable = out[0].Get<MeshArray> (Types ().meshArray);
	const MeshArray *constant = out[1].Get<MeshArray> (Types ().meshArray);
	ASSERT_NE (variable, nullptr);
	ASSERT_NE (constant, nullptr);
	ASSERT_EQ (variable->items.size (), 3u);
	ASSERT_EQ (constant->items.size (), 3u);

	// La branche constante : UNE seule instance, servie trois fois.
	EXPECT_EQ (constant->items[0].get (), constant->items[1].get ());
	EXPECT_EQ (constant->items[1].get (), constant->items[2].get ());
	// La branche dependante : trois instances distinctes, aux contenus distincts.
	EXPECT_NE (variable->items[0].get (), variable->items[1].get ());
	EXPECT_GT (std::abs (SumZ (*variable->items[0]) - SumZ (*variable->items[1])), 1e-3);
	EXPECT_GT (std::abs (SumZ (*variable->items[1]) - SumZ (*variable->items[2])), 1e-3);
}

TEST (TEST_cggraph_flow_foreach, an_empty_array_yields_an_empty_array_and_no_pass)
{
	WriteSmoothDocument ("flow_empty.json", "pieces", "lissees");

	flow::ForEachNode node;
	node.SetSubgraphReference ("flow_empty.json");

	EvalContext ctx;
	ValueList in (1);
	in[0] = MakeArray (std::vector<std::shared_ptr<const Mesh>> ());
	ValueList out (1);
	ASSERT_TRUE (node.Compute (ctx, in, out));
	const MeshArray *result = out[0].Get<MeshArray> (Types ().meshArray);
	ASSERT_NE (result, nullptr);
	EXPECT_TRUE (result->items.empty ());
	EXPECT_EQ (node.GetPassCount (), 0u);
}

TEST (TEST_cggraph_flow_foreach, arrays_of_different_lengths_are_refused_by_name)
{
	// Deux frontieres d'entree, deux suites de longueurs differentes. Prendre la
	// plus courte serait un choix silencieux, la plus longue un acces hors
	// bornes : c'est un refus, et il nomme le port.
	Graph doc;
	const NodeId inA = doc.AddNode (MakeNode (flow::kInputTypeName));
	const NodeId inB = doc.AddNode (MakeNode (flow::kInputTypeName));
	const NodeId out = doc.AddNode (MakeNode (flow::kOutputTypeName));
	doc.FindNode (inA)->GetParams ().SetInt ("slot", 0);
	doc.FindNode (inA)->GetParams ().SetString ("nom", "gauche");
	doc.FindNode (inB)->GetParams ().SetInt ("slot", 1);
	doc.FindNode (inB)->GetParams ().SetString ("nom", "droite");
	ASSERT_EQ (doc.Connect (inA, 0, out, 0), ConnectStatus::Ok);
	WriteDocument ("flow_pair.json", doc);

	flow::ForEachNode node;
	node.SetSubgraphReference ("flow_pair.json");
	ASSERT_EQ (node.GetDesc ().inputs.size (), 2u);

	EvalContext ctx;
	ValueList in (2);
	in[0] = MakeArray ({ MakeBumpGrid (4, 1.0f), MakeBumpGrid (4, 2.0f) });
	in[1] = MakeArray ({ MakeBumpGrid (4, 3.0f) });
	ValueList out2 (1);
	EXPECT_FALSE (node.Compute (ctx, in, out2));
	EXPECT_EQ (node.GetLastInnerStatus (), EvalStatus::MissingInput);
	EXPECT_EQ (node.GetLastInnerDetail (), "droite");
}

TEST (TEST_cggraph_flow_foreach, a_decomposed_file_diverges_piece_by_piece)
{
	// Le cas d'usage que le corpus nomme : une decomposition rend N pieces,
	// chacune traitee separement. C'est la SEULE source de suite du catalogue.
	WriteMultiObjectObj ("flow_parts.obj", 3);
	WriteSmoothDocument ("flow_parts.json", "pieces", "lissees");

	Graph graph;
	const NodeId load = graph.AddNode (MakeNode ("mesh.io.load_parts"));
	ASSERT_NE (load, kInvalidNodeId);
	graph.FindNode (load)->GetParams ().SetString ("path", "flow_parts.obj");
	const NodeId host = graph.AddNode (std::unique_ptr<Node> (new flow::ForEachNode ()));
	ASSERT_TRUE (graph.SetNodeSubgraph (host, "flow_parts.json"));
	ASSERT_EQ (graph.Connect (load, 0, host, 0), ConnectStatus::Ok);

	Evaluator eval (graph);
	EvalContext ctx;
	ValueList out;
	ASSERT_EQ (eval.Evaluate (host, out, ctx).status, EvalStatus::Ok);
	const MeshArray *result = out[0].Get<MeshArray> (Types ().meshArray);
	ASSERT_NE (result, nullptr);
	ASSERT_EQ (result->items.size (), 3u);
	// Les trois pieces restent a des abscisses differentes : elles ne se sont pas
	// confondues en chemin. L'abscisse MOYENNE, et non celle du premier sommet :
	// le lissage deplace les sommets, et un test qui figerait une coordonnee
	// exacte mesurerait le lissage plutot que la divergence.
	double previous = -1e9;
	for (std::size_t k = 0; k < 3; ++k)
	{
		ASSERT_EQ (result->items[k]->GetNVertices (), 4u);
		const std::vector<float> &v = result->items[k]->GetVertices ();
		double mean = 0.0;
		for (std::size_t i = 0; i < v.size (); i += 3)
			mean += v[i];
		mean /= 4.0;
		EXPECT_GT (mean, previous + 5.0) << "piece " << k;
		previous = mean;
	}
}

// ===========================================================================
//  flow.repeat -- la boucle bornee sans cycle
// ===========================================================================

TEST (TEST_cggraph_flow_repeat, n_enters_the_signature)
{
	// Critere 9.1. Rien n'a ete ecrit pour cela : `n` est un parametre Semantic,
	// donc HashParamSet le hache comme les autres. Ce cas verifie le mecanisme de
	// l'etape 1 sur un parametre de l'etape 9 -- et c'est bien le sujet, car un
	// `n` pose NonSemantic compilerait et rendrait faux.
	//
	// ⚠ LA VALEUR EST MUTEE PAR SON ADRESSE, jamais par SetInt, et c'est ce qui
	// donne des dents a ce cas. La premiere redaction appelait SetInt : or les
	// mutateurs REPOSENT le role a chaque appel, avec Semantic pour defaut. Le
	// montage reparait donc lui-meme le defaut qu'il pretendait detecter -- poser
	// `n` NonSemantic au constructeur le laissait VERT (sabotage S13). Muter par
	// l'adresse est aussi le chemin REEL d'un panneau (P4, D26).
	Graph graph;
	const NodeId host = graph.AddNode (std::unique_ptr<Node> (new flow::RepeatNode ()));
	Evaluator eval (graph);

	// Volet declaratif : le role est celui qui entre dans la signature.
	const ParamEntry *entry = graph.FindNode (host)->GetParams ().FindEntry ("n");
	ASSERT_NE (entry, nullptr);
	EXPECT_EQ (entry->role, ParamRole::Semantic);

	ParamValue *value = graph.FindNode (host)->GetParams ().Find ("n");
	ASSERT_NE (value, nullptr);
	value->intValue = 3;
	const Hash three = eval.GetSignature (host);
	value->intValue = 4;
	const Hash four = eval.GetSignature (host);
	value->intValue = 3;
	const Hash again = eval.GetSignature (host);

	EXPECT_NE (three, four);
	EXPECT_EQ (three, again);
}

TEST (TEST_cggraph_flow_repeat, repeating_n_times_equals_n_chained_applications)
{
	WriteSmoothDocument ("flow_rep.json", "entree", "sortie", 1);

	const std::shared_ptr<Mesh> source = MakeBumpGrid (6, 5.0f);

	// Reference : trois lissages a la file, a plat.
	Graph flat;
	NodeId previous = flat.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.mesh", Types ().mesh, Value::Make (Types ().mesh, source))));
	for (int k = 0; k < 3; ++k)
	{
		const NodeId smooth = flat.AddNode (MakeNode ("mesh.smooth.laplacian"));
		flat.FindNode (smooth)->GetParams ().SetInt ("iterations", 1);
		ASSERT_EQ (flat.Connect (previous, 0, smooth, 0), ConnectStatus::Ok);
		previous = smooth;
	}
	Evaluator flatEval (flat);
	EvalContext ctx;
	ValueList flatOut;
	ASSERT_EQ (flatEval.Evaluate (previous, flatOut, ctx).status, EvalStatus::Ok);
	const double expected = SumZ (*flatOut[0].Get<Mesh> (Types ().mesh));

	Graph graph;
	const NodeId src = graph.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.mesh", Types ().mesh, Value::Make (Types ().mesh, source))));
	const NodeId host = graph.AddNode (std::unique_ptr<Node> (new flow::RepeatNode ()));
	ASSERT_TRUE (graph.SetNodeSubgraph (host, "flow_rep.json"));
	graph.FindNode (host)->GetParams ().SetInt ("n", 3);
	ASSERT_EQ (graph.Connect (src, 0, host, 0), ConnectStatus::Ok);

	Evaluator eval (graph);
	ValueList out;
	ASSERT_EQ (eval.Evaluate (host, out, ctx).status, EvalStatus::Ok);
	EXPECT_NEAR (SumZ (*out[0].Get<Mesh> (Types ().mesh)), expected, 1e-4);
	// Et le chainage a bien eu lieu : trois passes, pas une.
	const flow::RepeatNode *node = dynamic_cast<const flow::RepeatNode *> (graph.FindNode (host));
	ASSERT_NE (node, nullptr);
	EXPECT_EQ (node->GetPassCount (), 3u);
	// Trois lissages ne rendent pas ce que l'entree portait : la reference n'est
	// pas satisfaite par accident.
	EXPECT_GT (std::abs (expected - SumZ (*source)), 1e-3);
}

TEST (TEST_cggraph_flow_repeat, repeating_zero_times_is_the_identity_and_runs_no_pass)
{
	WriteSmoothDocument ("flow_rep0.json", "entree", "sortie");

	flow::RepeatNode node;
	node.SetSubgraphReference ("flow_rep0.json");
	node.GetParams ().SetInt ("n", 0);

	const std::shared_ptr<Mesh> source = MakeBumpGrid (4, 7.0f);
	EvalContext ctx;
	ValueList in (1);
	in[0] = Value::Make (Types ().mesh, source);
	ValueList out (1);
	ASSERT_TRUE (node.Compute (ctx, in, out));
	// L'identite au sens fort : la MEME valeur, pas une copie egale.
	EXPECT_EQ (out[0].Get<Mesh> (Types ().mesh), source.get ());
	EXPECT_EQ (node.GetPassCount (), 0u);
}

TEST (TEST_cggraph_flow_repeat, a_negative_n_is_refused_rather_than_quietly_clamped)
{
	WriteSmoothDocument ("flow_repneg.json", "entree", "sortie");

	flow::RepeatNode node;
	node.SetSubgraphReference ("flow_repneg.json");
	node.GetParams ().SetInt ("n", -2);

	EvalContext ctx;
	ValueList in (1);
	in[0] = Value::Make (Types ().mesh, MakeBumpGrid (4, 1.0f));
	ValueList out (1);
	EXPECT_FALSE (node.Compute (ctx, in, out));
	EXPECT_EQ (node.GetLastInnerDetail (), "n");
	EXPECT_EQ (node.GetPassCount (), 0u);
}

TEST (TEST_cggraph_flow_repeat, a_document_without_a_one_to_one_boundary_is_refused)
{
	// La regle du gabarit : un descripteur ne declare que ce que le corps sait
	// tenir. Repeat CHAINE, donc il exige une correspondance un pour un ; un
	// document a deux sorties ne se repete pas, et le refus le dit plutot que de
	// choisir la premiere.
	Graph doc;
	const NodeId in = doc.AddNode (MakeNode (flow::kInputTypeName));
	const NodeId out0 = doc.AddNode (MakeNode (flow::kOutputTypeName));
	const NodeId out1 = doc.AddNode (MakeNode (flow::kOutputTypeName));
	doc.FindNode (out1)->GetParams ().SetInt ("slot", 1);
	const NodeId cube = doc.AddNode (MakeNode ("shape.cube"));
	ASSERT_EQ (doc.Connect (in, 0, out0, 0), ConnectStatus::Ok);
	ASSERT_EQ (doc.Connect (cube, 0, out1, 0), ConnectStatus::Ok);
	WriteDocument ("flow_rep2.json", doc);

	flow::RepeatNode node;
	node.SetSubgraphReference ("flow_rep2.json");
	// Les ports de Repeat sont FIXES : ils ne suivent pas le document.
	EXPECT_EQ (node.GetDesc ().inputs.size (), 1u);
	EXPECT_EQ (node.GetDesc ().outputs.size (), 1u);

	EvalContext ctx;
	ValueList in2 (1);
	in2[0] = Value::Make (Types ().mesh, MakeBumpGrid (4, 1.0f));
	ValueList out (1);
	EXPECT_FALSE (node.Compute (ctx, in2, out));
	EXPECT_EQ (node.GetLastInnerDetail (), "frontiere");
}

TEST (TEST_cggraph_flow_repeat, the_loop_lives_in_the_node_and_the_graph_stays_a_dag)
{
	// Critere 9.2, reconduit de l'etape 0 SUR LE MONTAGE QUI AURAIT PU LE
	// CASSER : une boucle. Elle n'ajoute aucune arete de retour -- le retour
	// arriere vit DANS le noeud --, et Connect refuse toujours un cycle, y
	// compris celui qu'on essaierait de fermer autour d'un noeud de boucle.
	WriteSmoothDocument ("flow_dag.json", "entree", "sortie");

	Graph graph;
	const NodeId src = graph.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.mesh", Types ().mesh,
		                     Value::Make (Types ().mesh, MakeBumpGrid (4, 1.0f)))));
	const NodeId host = graph.AddNode (std::unique_ptr<Node> (new flow::RepeatNode ()));
	const NodeId probe = graph.AddNode (std::unique_ptr<Node> (new MeshProbeNode ()));
	ASSERT_TRUE (graph.SetNodeSubgraph (host, "flow_dag.json"));
	ASSERT_EQ (graph.Connect (src, 0, host, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (host, 0, probe, 0), ConnectStatus::Ok);

	// Refermer la boucle a la main : refuse, et le noeud de boucle n'y change
	// rien. Il n'existe AUCUNE exception au refus de cycle.
	graph.Disconnect (host, 0);
	EXPECT_EQ (graph.Connect (probe, 0, host, 0), ConnectStatus::Cycle);
}

// ===========================================================================
//  Effets de bord au travers d'un sous-graphe -- criteres 9.3 et 9.4
// ===========================================================================

TEST (TEST_cggraph_flow_sinks, a_sink_inside_a_subgraph_writes_nothing_without_an_explicit_demand)
{
	const std::string prefix = "flow_quiet_";
	RemoveFilesWithPrefix (prefix);
	WriteSinkDocument ("flow_quiet.json", prefix + "{hash}.obj");

	flow::ForEachNode node;
	node.SetSubgraphReference ("flow_quiet.json");
	// `sinks` est FAUX par defaut : le defaut penche du cote qui n'ecrit rien.
	EXPECT_FALSE (node.GetDesc ().sideEffect);

	EvalContext ctx;
	ValueList in (1);
	in[0] = MakeArray ({ MakeBumpGrid (4, 1.0f), MakeBumpGrid (4, 2.0f) });
	ValueList out (1);
	ASSERT_TRUE (node.Compute (ctx, in, out));
	EXPECT_TRUE (FilesWithPrefix (prefix).empty ());
}

TEST (TEST_cggraph_flow_sinks, naming_the_sinks_makes_the_host_a_side_effect_node)
{
	const std::string prefix = "flow_loud_";
	RemoveFilesWithPrefix (prefix);
	WriteSinkDocument ("flow_loud.json", prefix + "{hash}.obj");

	flow::ForEachNode node;
	node.SetSubgraphReference ("flow_loud.json");
	ASSERT_FALSE (node.GetDesc ().sideEffect);

	// La demande fait de l'hote un noeud a effet de bord. Sans cette propagation,
	// le cache exterieur servirait la seconde evaluation et la seconde ecriture
	// n'aurait pas lieu -- faux, et muet.
	node.GetParams ().SetBool ("sinks", true);
	node.RefreshExternalState ();
	EXPECT_TRUE (node.GetDesc ().sideEffect);

	// Et retirer la demande le rend a nouveau cacheable : la propagation depend
	// des DEUX conditions, pas d'une seule.
	node.GetParams ().SetBool ("sinks", false);
	node.RefreshExternalState ();
	EXPECT_FALSE (node.GetDesc ().sideEffect);
}

TEST (TEST_cggraph_flow_sinks, a_document_without_any_sink_never_becomes_a_side_effect_node)
{
	// Le versant symetrique : sans lui, « l'hote herite de l'effet de bord »
	// serait satisfait par un hote qui se declare toujours a effet de bord, donc
	// qui ne serait jamais mis en cache.
	WriteSmoothDocument ("flow_nosink.json", "pieces", "lissees");

	flow::ForEachNode node;
	node.SetSubgraphReference ("flow_nosink.json");
	node.GetParams ().SetBool ("sinks", true);
	node.RefreshExternalState ();
	EXPECT_FALSE (node.GetDesc ().sideEffect);
}

TEST (TEST_cggraph_flow_sinks, two_evaluations_of_the_same_graph_write_the_same_files_twice)
{
	// Criteres 9.3 et 9.4 reunis, sur N elements. Le montage est celui que
	// l'etape 3 avait du trouver : UN graphe charge une fois et tire DEUX fois.
	// Deux executions distinctes reconstruiraient les noeuds et ne pourraient
	// pas distinguer un nom derive d'un compteur d'un nom derive du contenu.
	const std::string prefix = "flow_twice_";
	RemoveFilesWithPrefix (prefix);
	WriteSinkDocument ("flow_twice.json", prefix + "{hash}.obj");

	Graph graph;
	const NodeId src = graph.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.array", Types ().meshArray,
		                     MakeArray ({ MakeBumpGrid (4, 1.0f), MakeBumpGrid (4, 2.0f),
		                                  MakeBumpGrid (4, 3.0f) }))));
	const NodeId host = graph.AddNode (std::unique_ptr<Node> (new flow::ForEachNode ()));
	ASSERT_TRUE (graph.SetNodeSubgraph (host, "flow_twice.json"));
	graph.FindNode (host)->GetParams ().SetBool ("sinks", true);
	ASSERT_EQ (graph.Connect (src, 0, host, 0), ConnectStatus::Ok);

	Evaluator eval (graph);
	EvalContext ctx;
	ValueList out;
	ASSERT_EQ (eval.Evaluate (host, out, ctx).status, EvalStatus::Ok);

	const std::vector<std::string> first = FilesWithPrefix (prefix);
	// Trois elements distincts, trois noms distincts : les noms derivent du
	// CONTENU, donc ils divergent avec lui.
	ASSERT_EQ (first.size (), 3u);

	// Effacer HORS du graphe : si le cache servait, ils ne reviendraient pas.
	RemoveFilesWithPrefix (prefix);
	ASSERT_TRUE (FilesWithPrefix (prefix).empty ());

	ASSERT_EQ (eval.Evaluate (host, out, ctx).status, EvalStatus::Ok);
	const std::vector<std::string> second = FilesWithPrefix (prefix);
	EXPECT_EQ (first, second);
}

TEST (TEST_cggraph_flow_sinks, identical_elements_collapse_onto_one_name_and_the_content_says_so)
{
	// RESERVE, figee plutot que decouverte plus tard. Le nom derive du CONTENU
	// -- l'un des deux regimes que le corpus autorise --, si bien que deux
	// elements identiques ecrivent le MEME fichier. Trois passes, deux noms.
	// C'est le comportement voulu : deux fichiers de memes octets n'ont pas a
	// exister deux fois. Le figer ici evite qu'on le decouvre comme un defaut.
	const std::string prefix = "flow_collide_";
	RemoveFilesWithPrefix (prefix);
	WriteSinkDocument ("flow_collide.json", prefix + "{hash}.obj");

	const std::shared_ptr<const Mesh> twin = MakeBumpGrid (4, 2.0f);

	flow::ForEachNode node;
	node.SetSubgraphReference ("flow_collide.json");
	node.GetParams ().SetBool ("sinks", true);
	node.RefreshExternalState ();

	EvalContext ctx;
	ValueList in (1);
	in[0] = MakeArray ({ twin, MakeBumpGrid (4, 9.0f), twin });
	ValueList out (1);
	ASSERT_TRUE (node.Compute (ctx, in, out));
	EXPECT_EQ (node.GetPassCount (), 3u);
	EXPECT_EQ (FilesWithPrefix (prefix).size (), 2u);
}

// ===========================================================================
//  Relecture de fixtures -- les branches qu'aucun montage n'atteignait
// ===========================================================================
// Ces cinq cas ne viennent pas des criteres : ils viennent de la RELECTURE de la
// campagne de sabotage. Trois branches ecrites plus haut n'etaient couvertes par
// aucun montage, et un sabotage vert (S20) l'a montre pour la premiere d'entre
// elles.

TEST (TEST_cggraph_flow_boundary, an_unbound_input_refuses_instead_of_yielding_an_empty_value)
{
	// flow.in est un noeud du CATALOGUE : rien n'empeche de le poser dans un
	// document ordinaire et de le tirer. Personne ne lui lie alors de valeur.
	// Rendre un succes vide propagerait le vide jusqu'a un consommateur qui le
	// diagnostiquerait a sa place, et le refus nommerait le mauvais noeud.
	//
	// Aucun montage n'atteignait cette branche avant ce cas : le sabotage S20 --
	// « rendre un succes vide » -- restait VERT.
	Graph graph;
	const NodeId in = graph.AddNode (MakeNode (flow::kInputTypeName));
	ASSERT_NE (in, kInvalidNodeId);

	Evaluator eval (graph);
	EvalContext ctx;
	ValueList out;
	const EvalResult result = eval.Evaluate (in, out, ctx);
	EXPECT_EQ (result.status, EvalStatus::ComputeFailed);
	EXPECT_EQ (result.node, in);
}

TEST (TEST_cggraph_flow_foreach, a_null_element_stops_the_loop_and_the_inner_refusal_is_republished)
{
	// Un element nul produit une valeur VIDE a la frontiere : la passe echoue, la
	// boucle s'arrete, et le compte de passes dit OU. C'est le versant « une
	// passe echoue » de la boucle, qu'aucun autre montage n'atteignait.
	WriteSmoothDocument ("flow_null.json", "pieces", "lissees");

	flow::ForEachNode node;
	node.SetSubgraphReference ("flow_null.json");

	std::vector<std::shared_ptr<const Mesh>> items;
	items.push_back (MakeBumpGrid (4, 1.0f));
	items.push_back (std::shared_ptr<const Mesh> ());
	items.push_back (MakeBumpGrid (4, 3.0f));

	EvalContext ctx;
	ValueList in (1);
	in[0] = MakeArray (items);
	ValueList out (1);
	EXPECT_FALSE (node.Compute (ctx, in, out));
	EXPECT_EQ (node.GetLastInnerStatus (), EvalStatus::ComputeFailed);
	// Deux passes, pas trois : la boucle s'arrete au premier refus plutot que de
	// produire une suite trouee dont personne ne saurait dire ce qui manque.
	EXPECT_EQ (node.GetPassCount (), 2u);
}

TEST (TEST_cggraph_flow_foreach, a_value_of_the_wrong_type_is_refused_by_port_name)
{
	WriteSmoothDocument ("flow_type.json", "pieces", "lissees");

	flow::ForEachNode node;
	node.SetSubgraphReference ("flow_type.json");

	EvalContext ctx;
	ValueList in (1);   // vide : ni suite, ni maillage
	ValueList out (1);
	EXPECT_FALSE (node.Compute (ctx, in, out));
	EXPECT_EQ (node.GetLastInnerStatus (), EvalStatus::MissingInput);
	EXPECT_EQ (node.GetLastInnerDetail (), "pieces");
}

TEST (TEST_cggraph_flow_boundary, the_three_hosts_refuse_by_name_when_no_document_is_referenced)
{
	// LoadStatus::NoReference n'etait atteint par aucun montage : les cas
	// precedents referencent tous un document, existant ou non. Un hote neuf,
	// lui, n'en reference aucun -- c'est son etat a la sortie de la fabrique.
	EvalContext ctx;
	ValueList in (1);
	in[0] = Value::Make (Types ().mesh, MakeBumpGrid (4, 1.0f));
	ValueList out (1);

	flow::SubgraphNode subgraph;
	EXPECT_FALSE (subgraph.Compute (ctx, in, out));
	EXPECT_NE (subgraph.GetLastInnerDetail ().find ("noReference"), std::string::npos)
		<< subgraph.GetLastInnerDetail ();

	flow::RepeatNode repeat;
	EXPECT_FALSE (repeat.Compute (ctx, in, out));
	EXPECT_NE (repeat.GetLastInnerDetail ().find ("noReference"), std::string::npos)
		<< repeat.GetLastInnerDetail ();

	ValueList arrayIn (1);
	arrayIn[0] = MakeArray ({ MakeBumpGrid (4, 1.0f) });
	flow::ForEachNode each;
	EXPECT_FALSE (each.Compute (ctx, arrayIn, out));
	EXPECT_NE (each.GetLastInnerDetail ().find ("noReference"), std::string::npos)
		<< each.GetLastInnerDetail ();
}

TEST (TEST_cggraph_flow_boundary, an_unreadable_reference_falls_back_on_the_stat_key_and_the_document_still_wins)
{
	// Le repli du releve d'identite : quand le fichier n'est pas lisible, il n'y
	// a pas de contenu a hacher. Aucun montage ne l'atteignait -- tous les autres
	// referencent un fichier lisible, ou ne relevent pas l'etat exterieur.
	std::filesystem::remove ("flow_late.json");

	flow::SubgraphNode node;
	node.SetSubgraphReference ("flow_late.json");
	node.RefreshExternalState ();
	const std::string absent = GetStringParam (node, "subgraph.identity");
	node.RefreshExternalState ();
	// Stable tant que rien ne bouge : un releve qui varierait sans cause ferait
	// recalculer sans fin.
	EXPECT_EQ (GetStringParam (node, "subgraph.identity"), absent);
	EXPECT_EQ (node.GetDesc ().outputs.size (), 0u);

	// Le fichier APPARAIT : l'identite bouge, et le noeud publie ses ports.
	WriteSmoothDocument ("flow_late.json", "piece", "lisse");
	node.RefreshExternalState ();
	EXPECT_NE (GetStringParam (node, "subgraph.identity"), absent);
	EXPECT_EQ (node.GetDesc ().outputs.size (), 1u);
}

TEST (TEST_cggraph_flow_sinks, a_sink_that_fails_stops_the_loop_and_names_the_node)
{
	// Le tirage des puits a son propre chemin de refus, qu'aucun montage
	// n'atteignait : tous les puits precedents reussissent. Une extension que
	// Mesh::save ne deroute pas suffit -- et c'est le cas d'usage le plus banal
	// qui soit, un chemin d'export mal ecrit.
	WriteSinkDocument ("flow_badsink.json", "flow_badsink_{hash}.xyz");

	flow::ForEachNode node;
	node.SetSubgraphReference ("flow_badsink.json");
	node.GetParams ().SetBool ("sinks", true);
	node.RefreshExternalState ();

	EvalContext ctx;
	ValueList in (1);
	in[0] = MakeArray ({ MakeBumpGrid (4, 1.0f), MakeBumpGrid (4, 2.0f) });
	ValueList out (1);
	EXPECT_FALSE (node.Compute (ctx, in, out));
	EXPECT_EQ (node.GetLastInnerStatus (), EvalStatus::ComputeFailed);
	// La premiere passe suffit a refuser : la boucle ne continue pas a ecrire ce
	// qu'elle ne sait pas ecrire.
	EXPECT_EQ (node.GetPassCount (), 1u);
}

TEST (TEST_cggraph_flow_repeat, a_pass_that_fails_stops_the_chain)
{
	// Le chainage a son propre versant negatif : une passe qui echoue arrete la
	// boucle plutot que de laisser les suivantes s'executer sur une valeur qui
	// n'existe pas -- le refus nommerait alors le mauvais tour de boucle.
	//
	// ⚠ Ce cas atteint le refus par le REFUS DE LA PASSE, pas par la garde
	// « la passe a reussi mais n'a rien rendu ». Cette derniere reste declaree
	// non couverte (voir repeat.cpp).
	WriteSmoothDocument ("flow_repfail.json", "entree", "sortie");

	flow::RepeatNode node;
	node.SetSubgraphReference ("flow_repfail.json");
	node.GetParams ().SetInt ("n", 3);

	EvalContext ctx;
	ValueList in (1);   // valeur VIDE : la frontiere n'aura rien a rendre
	ValueList out (1);
	EXPECT_FALSE (node.Compute (ctx, in, out));
	EXPECT_EQ (node.GetPassCount (), 1u);
}

TEST (TEST_cggraph_flow_foreach, two_concurrent_hosts_on_one_document_produce_what_two_sequential_ones_produce)
{
	// La forme du critere 6.2, portee sur le mecanisme de cette etape. Ce qui est
	// en jeu ici n'est pas dans la couche B d'origine : les deux hotes partagent
	// le meme DOCUMENT et la meme PILE thread_local de gardes de recursion, et
	// chacun construit son graphe interne et son evaluateur interne pendant que
	// l'autre en fait autant.
	//
	// C'est ce test qui justifie que l'instance interne soit construite PAR
	// CALCUL plutot que retenue par le noeud : un graphe interne retenu serait
	// un etat mutable partage entre deux Compute, et Compute est concurrent par
	// construction depuis l'etape 6.
	WriteSmoothDocument ("flow_conc.json", "pieces", "lissees", 2);

	std::vector<std::shared_ptr<const Mesh>> left;
	std::vector<std::shared_ptr<const Mesh>> right;
	for (int k = 0; k < 3; ++k)
	{
		left.push_back (MakeBumpGrid (7, 1.0f + static_cast<float> (k)));
		right.push_back (MakeBumpGrid (6, 4.0f + static_cast<float> (k)));
	}

	// ⚠ DEUX NOMS DE TYPE DISTINCTS, et ce n'est pas cosmetique. Une source de
	// test ne porte aucun parametre : sa signature se reduit a son nom de type.
	// Deux sources de meme nom ont donc la MEME signature, leurs deux hotes
	// aussi, et le cache sert le resultat du premier au second -- la premiere
	// redaction de ce cas mesurait ainsi UNE branche en croyant en mesurer deux.
	Graph graph;
	const NodeId srcA = graph.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.arrayA", Types ().meshArray, MakeArray (left))));
	const NodeId srcB = graph.AddNode (std::unique_ptr<Node> (
		new ValueSourceNode ("test.arrayB", Types ().meshArray, MakeArray (right))));
	const NodeId hostA = graph.AddNode (std::unique_ptr<Node> (new flow::ForEachNode ()));
	const NodeId hostB = graph.AddNode (std::unique_ptr<Node> (new flow::ForEachNode ()));
	ASSERT_TRUE (graph.SetNodeSubgraph (hostA, "flow_conc.json"));
	ASSERT_TRUE (graph.SetNodeSubgraph (hostB, "flow_conc.json"));
	ASSERT_EQ (graph.Connect (srcA, 0, hostA, 0), ConnectStatus::Ok);
	ASSERT_EQ (graph.Connect (srcB, 0, hostB, 0), ConnectStatus::Ok);

	// Empreintes de reference, prises SEQUENTIELLEMENT.
	std::vector<double> refA;
	std::vector<double> refB;
	{
		Evaluator eval (graph);
		EvalContext ctx;
		ValueList out;
		ASSERT_EQ (eval.Evaluate (hostA, out, ctx).status, EvalStatus::Ok);
		for (const std::shared_ptr<const Mesh> &m : out[0].Get<MeshArray> (Types ().meshArray)->items)
			refA.push_back (SumZ (*m));
		ASSERT_EQ (eval.Evaluate (hostB, out, ctx).status, EvalStatus::Ok);
		for (const std::shared_ptr<const Mesh> &m : out[0].Get<MeshArray> (Types ().meshArray)->items)
			refB.push_back (SumZ (*m));
	}
	ASSERT_EQ (refA.size (), 3u);
	ASSERT_EQ (refB.size (), 3u);
	ASSERT_GT (std::abs (refA[0] - refB[0]), 1e-3);

	std::atomic<int> mismatches (0);
	const int rounds = 8;
	auto run = [&] (NodeId target, const std::vector<double> &reference) {
		Evaluator evaluator (graph);
		for (int round = 0; round < rounds; ++round)
		{
			EvalContext ctx;
			ValueList out;
			// Le cache est vide a chaque tour : sans cela, un seul calcul reel
			// aurait lieu et les sept autres tours ne croiseraient rien.
			evaluator.ClearCache ();
			if (evaluator.Evaluate (target, out, ctx).status != EvalStatus::Ok || out.empty ())
			{
				++mismatches;
				continue;
			}
			const MeshArray *got = out[0].Get<MeshArray> (Types ().meshArray);
			if (got == nullptr || got->items.size () != reference.size ())
			{
				++mismatches;
				continue;
			}
			for (std::size_t k = 0; k < reference.size (); ++k)
				if (std::abs (SumZ (*got->items[k]) - reference[k]) > 1e-4)
					++mismatches;
		}
	};

	std::thread worker ([&] { run (hostB, refB); });
	run (hostA, refA);
	worker.join ();
	EXPECT_EQ (mismatches.load (), 0);
}

TEST (TEST_cggraph_flow_sinks, the_headless_runner_names_the_files_written_inside_the_subgraph)
{
	// Le critere 3.5 ETENDU A N ELEMENTS, et par l'instrument que le critere
	// nomme -- la liste que le pilote republie --, non par un listage de
	// repertoire. Sans la republication a la frontiere, ce rapport serait VIDE
	// pour un graphe qui vient d'ecrire trois fichiers : le pilote ne verrait
	// pas au travers du sous-graphe.
	//
	// Montage de bout en bout : un fichier multi-objets, un ForEach, un puits
	// DANS le document delegue, et un document exterieur relu depuis le disque.
	// ⚠ Le prefixe des SORTIES ne doit prefixer aucun fichier du montage. La
	// premiere redaction nommait les sorties « flow_runner_… » et le document
	// delegue « flow_runner_sub.json » : le nettoyage effacait le document, et
	// le comptage le comptait. Un prefixe est un filtre, et un filtre trop large
	// mesure autre chose que ce qu'il annonce.
	const std::string prefix = "flow_written_";
	RemoveFilesWithPrefix (prefix);
	WriteMultiObjectObj ("flow_runner.obj", 3);
	WriteSinkDocument ("flow_subdoc_run.json", prefix + "{hash}.obj");

	{
		Graph outer;
		const NodeId load = outer.AddNode (MakeNode ("mesh.io.load_parts"));
		ASSERT_NE (load, kInvalidNodeId);
		outer.FindNode (load)->GetParams ().SetString ("path", "flow_runner.obj");
		const NodeId host = outer.AddNode (MakeNode ("flow.foreach"));
		ASSERT_NE (host, kInvalidNodeId);
		ASSERT_TRUE (outer.SetNodeSubgraph (host, "flow_subdoc_run.json"));
		outer.FindNode (host)->GetParams ().SetBool ("sinks", true);
		ASSERT_EQ (outer.Connect (load, 0, host, 0), ConnectStatus::Ok);
		WriteDocument ("flow_runner.json", outer);
	}

	RunRequest request;
	request.document = "flow_runner.json";
	request.allSinks = true;
	const RunReport report = cggraph_nodes::Run (request);
	ASSERT_EQ (report.status, RunStatus::Ok) << report.detail;
	EXPECT_EQ (report.written.size (), 3u);
	EXPECT_EQ (FilesWithPrefix (prefix).size (), 3u);

	// Sans cible nommee, rien n'est evalue et rien n'est ecrit -- la regle de
	// l'etape 3 traverse la frontiere du sous-graphe sans s'y perdre.
	RemoveFilesWithPrefix (prefix);
	RunRequest silent;
	silent.document = "flow_runner.json";
	const RunReport refused = cggraph_nodes::Run (silent);
	EXPECT_EQ (refused.status, RunStatus::NoTarget);
	EXPECT_TRUE (refused.written.empty ());
	EXPECT_TRUE (FilesWithPrefix (prefix).empty ());
}

// ===========================================================================
//  DOCUMENT EMBARQUE -- le corps DANS le parent, sans fichier a cote
// ===========================================================================
// Le champ `subgraph` accepte deux formes : une CHAINE, qui designe un fichier,
// et un OBJET, qui EST le document. La seconde rend le parent autonome -- rien
// a resoudre sur disque -- et rend son identite exacte, le contenu etant sa
// propre reference.

namespace {

// Le meme corps que WriteSmoothDocument, mais rendu en TEXTE au lieu d'etre
// ecrit. C'est ce texte qui se pose sur le noeud hote.
std::string SmoothDocumentText (int iterations = 1)
{
	Graph graph;
	const NodeId in = graph.AddNode (MakeNode (flow::kInputTypeName));
	const NodeId smooth = graph.AddNode (MakeNode ("mesh.smooth.laplacian"));
	const NodeId out = graph.AddNode (MakeNode (flow::kOutputTypeName));
	graph.FindNode (smooth)->GetParams ().SetInt ("iterations", iterations);
	graph.Connect (in, 0, smooth, 0);
	graph.Connect (smooth, 0, out, 0);
	return SaveGraph (graph);
}

} // namespace

TEST (TEST_cggraph_flow_embedded, a_body_carried_in_the_parent_computes_without_any_file)
{
	// LE CAS QUI MOTIVE TOUT : aucun fichier n'existe, aucun repertoire courant
	// n'est en jeu, et la boucle tourne quand meme. C'est ce qui manquait a
	// l'hote web, ou le corps devait etre depose a la main dans le systeme de
	// fichiers du worker avant que le document ne se calcule.
	Graph graph;
	const NodeId repeat = graph.AddNode (MakeNode ("flow.repeat"));
	ASSERT_NE (repeat, kInvalidNodeId);
	ASSERT_TRUE (graph.SetNodeSubgraphDocument (repeat, SmoothDocumentText ()));
	graph.FindNode (repeat)->GetParams ().SetInt ("n", 3);

	// Les ports sont publies des que le document est pose : la preuve qu'il a
	// bien ete lu, et non simplement range.
	ASSERT_EQ (graph.FindNode (repeat)->GetDesc ().inputs.size (), 1u);
	ASSERT_EQ (graph.FindNode (repeat)->GetDesc ().outputs.size (), 1u);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList in (1);
	in[0] = Value::Make (Types ().mesh, MakeBumpGrid (5, 3.0f));
	ValueList out (1);
	ASSERT_TRUE (graph.FindNode (repeat)->Compute (ctx, in, out));
	ASSERT_FALSE (out.empty ());
	EXPECT_NE (out[0].Get<Mesh> (Types ().mesh), nullptr);

	// Trois passes, et non une servie trois fois.
	EXPECT_EQ (static_cast<flow::SubgraphHostNode *> (graph.FindNode (repeat))->GetPassCount (), 3u);
}

TEST (TEST_cggraph_flow_embedded, the_two_forms_exclude_each_other)
{
	// Un noeud qui delegue a la fois un fichier et un texte n'aurait pas de sens,
	// et il faudrait ecrire quelque part lequel gagne -- une regle que personne
	// ne lirait au bon moment. Poser l'une efface donc l'autre, dans les deux
	// sens.
	Graph graph;
	const NodeId repeat = graph.AddNode (MakeNode ("flow.repeat"));

	ASSERT_TRUE (graph.SetNodeSubgraph (repeat, "corps.json"));
	EXPECT_EQ (graph.GetNodeSubgraph (repeat), "corps.json");
	EXPECT_TRUE (graph.GetNodeSubgraphDocument (repeat).empty ());

	ASSERT_TRUE (graph.SetNodeSubgraphDocument (repeat, SmoothDocumentText ()));
	EXPECT_TRUE (graph.GetNodeSubgraph (repeat).empty ());
	EXPECT_FALSE (graph.GetNodeSubgraphDocument (repeat).empty ());

	ASSERT_TRUE (graph.SetNodeSubgraph (repeat, "autre.json"));
	EXPECT_EQ (graph.GetNodeSubgraph (repeat), "autre.json");
	EXPECT_TRUE (graph.GetNodeSubgraphDocument (repeat).empty ());
}

TEST (TEST_cggraph_flow_embedded, editing_the_body_moves_the_signature_with_no_outside_probe)
{
	// LE GAIN LE MOINS VISIBLE ET LE PLUS IMPORTANT. Pour un FICHIER, la
	// reference ne hache qu'un NOM : editer le corps sans le renommer laisserait
	// le cache resservir l'ancien resultat, et c'est pourquoi l'hote releve
	// l'etat exterieur du fichier dans un parametre d'identite. Embarque, le
	// contenu EST la reference -- la signature bouge d'elle-meme.
	Graph graph;
	const NodeId repeat = graph.AddNode (MakeNode ("flow.repeat"));
	ASSERT_TRUE (graph.SetNodeSubgraphDocument (repeat, SmoothDocumentText (1)));

	const Hash before = Signature (graph, repeat);

	// Un seul parametre change dans le corps -- exactement le cas qui piegeait
	// le stat : meme taille, meme seconde.
	ASSERT_TRUE (graph.SetNodeSubgraphDocument (repeat, SmoothDocumentText (5)));
	EXPECT_NE (Signature (graph, repeat), before);

	// Et le releve exterieur ne s'en mele pas : il n'y a pas de fichier a
	// interroger, donc l'identite reste a son defaut. Le parametre EXISTE quand
	// meme -- il est declare au constructeur, et le retirer selon la forme ferait
	// varier le jeu de parametres d'un noeud a l'autre.
	graph.FindNode (repeat)->RefreshExternalState ();
	EXPECT_EQ (GetStringParam (*graph.FindNode (repeat), "subgraph.identity"), "absent");
}

TEST (TEST_cggraph_flow_embedded, the_document_survives_a_save_and_reload_as_an_object)
{
	// Le format porte l'objet, pas une chaine echappee : re-lire le document
	// rend un noeud qui delegue le MEME corps, et qui calcule.
	Graph graph;
	const NodeId repeat = graph.AddNode (MakeNode ("flow.repeat"));
	ASSERT_TRUE (graph.SetNodeSubgraphDocument (repeat, SmoothDocumentText ()));
    graph.FindNode (repeat)->GetParams ().SetInt ("n", 2);

	const std::string document = SaveGraph (graph);
	// L'objet embarque, et non une chaine : sans cela le document serait illisible
	// par tout autre outil, et le controle ci-dessous ne dirait rien de plus que
	// « une chaine a survecu ».
	EXPECT_NE (document.find ("\"subgraph\""), std::string::npos);
	EXPECT_EQ (document.find ("\"subgraph\": \""), std::string::npos);

	Graph reloaded;
	const CatalogFactory factory;
	ASSERT_TRUE (LoadGraph (document, factory, reloaded).IsOk ());

	NodeId found = kInvalidNodeId;
	for (NodeId id : reloaded.GetNodeIds ())
		if (reloaded.FindNode (id)->GetDesc ().typeName == "flow.repeat")
			found = id;
	ASSERT_NE (found, kInvalidNodeId);
	EXPECT_TRUE (reloaded.GetNodeSubgraph (found).empty ());
	EXPECT_FALSE (reloaded.GetNodeSubgraphDocument (found).empty ());
	EXPECT_EQ (reloaded.FindNode (found)->GetDesc ().inputs.size (), 1u);

	EvalContext ctx;
	ValueList in (1);
	in[0] = Value::Make (Types ().mesh, MakeBumpGrid (5, 2.0f));
	ValueList out (1);
	EXPECT_TRUE (reloaded.FindNode (found)->Compute (ctx, in, out));
}

TEST (TEST_cggraph_flow_embedded, an_invalid_embedded_document_is_refused_by_name)
{
	// Le corps embarque est relu par le MEME lecteur que son parent : une erreur
	// dedans se nomme comme n'importe quelle autre, au lieu de donner un noeud
	// muet sans port.
	Graph graph;
	const NodeId repeat = graph.AddNode (MakeNode ("flow.repeat"));
	ASSERT_TRUE (graph.SetNodeSubgraphDocument (repeat, "{\"format\":\"cggraph\"}"));

	// Sans frontiere lisible, l'hote reste VISIBLE et garde ses ports fixes --
	// c'est le parti pris pour toute reference qui ne se charge pas --, mais le
	// calcul refuse.
	EvalContext ctx;
	ValueList in (1);
	in[0] = Value::Make (Types ().mesh, MakeBumpGrid (4, 1.0f));
	ValueList out (1);
	EXPECT_FALSE (graph.FindNode (repeat)->Compute (ctx, in, out));
}
