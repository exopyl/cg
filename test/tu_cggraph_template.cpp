#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "../src/cggraph/core/evaluator.h"
#include "../src/cggraph/core/serialize.h"
#include "../src/cggraph/nodes/catalog_factory.h"
#include "../src/cggraph/nodes/io/file_ref.h"
#include "../src/cggraph/nodes/node_support.h"
#include "../src/cggraph/nodes/value_types.h"
#include "../src/cgmesh/mesh.h"

// ===========================================================================
//  Gabarits de pages maker (test/data/templates/*.json)
// ===========================================================================
//
// Un gabarit decrit une page entiere de maker : le graphe qu'elle evalue, les
// parametres qu'elle expose, ses sources de fichier. Le reste -- js/template.js
// -- ne nomme aucun noeud.
//
// CE QUE CES CAS PROTEGENT, et pourquoi ils existent. Un gabarit contient un
// DOCUMENT DE GRAPHE, c'est-a-dire quelque chose qui peut etre faux sans que
// rien ne le dise : un type de noeud renomme, un port qui change d'indice, un
// parametre disparu, une version de descripteur incrementee. Rien de tout cela
// n'est visible a la compilation, et la page ne le dirait qu'a l'execution, dans
// un navigateur, sous les yeux de l'utilisateur.
//
// Le gabarit vit donc sous test/data/ -- SOURCE UNIQUE -- et le build de maker
// en depose une copie dans web/data/templates/. C'est la source qui est testee
// ici ; la copie servie au navigateur ne se teste pas.
//
using namespace cggraph;
using namespace cggraph_nodes;

namespace {

std::string ReadTextFile (const char *path)
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
		text.resize ((std::size_t)size);
		if (std::fread (&text[0], 1, text.size (), f) != text.size ())
			text.clear ();
	}
	std::fclose (f);
	return text;
}

// Extrait la valeur d'une cle d'objet de premier niveau, par appariement
// d'accolades. Volontairement rudimentaire : ces cas n'ont pas besoin d'un
// analyseur JSON, et en tirer un dans la cible de test pour lire un champ
// ajouterait une dependance a ce qu'ils protegent. Les chaines sont traversees
// en tenant compte de l'echappement, sans quoi une accolade dans un commentaire
// du gabarit fausserait le compte -- et il y en a.
std::string ExtractObject (const std::string &json, const std::string &key)
{
	const std::string needle = "\"" + key + "\"";
	std::size_t k = json.find (needle);
	if (k == std::string::npos)
		return std::string ();
	k = json.find ('{', k);
	if (k == std::string::npos)
		return std::string ();

	int depth = 0;
	bool inString = false;
	bool escaped = false;
	for (std::size_t i = k; i < json.size (); ++i)
	{
		const char c = json[i];
		if (inString)
		{
			if (escaped)            escaped = false;
			else if (c == '\\')     escaped = true;
			else if (c == '"')      inString = false;
			continue;
		}
		if (c == '"')      inString = true;
		else if (c == '{') ++depth;
		else if (c == '}' && --depth == 0)
			return json.substr (k, i - k + 1);
	}
	return std::string ();
}

const char *kText3d = "./test/data/templates/text3d.json";
const char *kFont   = "./test/data/fonts/DejaVuSans.ttf";

// TOUS les gabarits livres. Enumeres ici et non decouverts par balayage de
// repertoire : un gabarit qu'on oublierait d'ajouter a cette liste ne serait
// jamais teste, et l'oubli serait silencieux -- alors qu'un gabarit RETIRE fait
// echouer bruyamment la lecture de son fichier.
const char *kAllTemplates[] = {
	"./test/data/templates/text3d.json",
	"./test/data/templates/relief.json",
	"./test/data/templates/pixels.json"
};

std::vector<unsigned char> ReadBytes (const char *path)
{
	const std::string text = ReadTextFile (path);
	return std::vector<unsigned char> (text.begin (), text.end ());
}

} // namespace

TEST (TEST_cggraph_template, the_text3d_template_document_loads_against_the_live_catalog)
{
	// Le cas le plus utile du fichier, et le moins spectaculaire : il prend en
	// defaut un gabarit dont un type de noeud aurait ete renomme, ou dont une
	// version de descripteur aurait ete incrementee. LoadGraph nomme lui-meme la
	// cause ; on la fait remonter telle quelle.
	const std::string tpl = ReadTextFile (kText3d);
	ASSERT_FALSE (tpl.empty ()) << kText3d << " introuvable";

	const std::string document = ExtractObject (tpl, "graph");
	ASSERT_FALSE (document.empty ()) << "le gabarit n'a pas de section \"graph\"";

	Graph graph;
	const CatalogFactory factory;
	const LoadResult result = LoadGraph (document, factory, graph);
	ASSERT_TRUE (result.IsOk ())
		<< ToString (result.status) << " : " << result.detail;

	// TREIZE. Les quatre du contour 2D -- fichier, police, contours, extrusion --
	// plus les six du SOCLE (deux formes de plaque, le selecteur qui les arbitre,
	// l'extrusion sur sa propre plage de Z, la fusion, et le selecteur « avec ou
	// sans socle »), plus les trois de l'ARETE (chanfrein, cavet, et le selecteur
	// de profil). Ce compte est un FILET : il rougit des qu'une branche est
	// ajoutee ou retiree, ce qui force a relire le graphe plutot qu'a le supposer.
	// Le quatorzieme est mesh.color : la couleur de la piece appartient au
	// DOCUMENT, pas aux reglages d'affichage de la page. Les deux derniers sont
	// la FIXATION MURALE et le selecteur qui la rend facultative -- places AVANT
	// la couleur, sans quoi le bandeau sortirait gris pendant que le reste est
	// peint.
	EXPECT_EQ (graph.GetNodeCount (), 16u);
}

TEST (TEST_cggraph_template, the_text3d_template_evaluates_to_a_mesh_once_its_source_is_fed)
{
	// Ce que la page fait, dans l'ordre ou elle le fait : relire le document,
	// alimenter la source par des octets, evaluer le noeud de sortie.
	//
	// La source est alimentee par l'interface ByteSource -- pas par le type
	// concret -- parce que c'est exactement ce que fait graphSetBytes cote maker.
	// Un noeud source qui cesserait de l'implementer casserait la page sans que
	// rien d'autre ne le dise.
	const std::string tpl = ReadTextFile (kText3d);
	ASSERT_FALSE (tpl.empty ());
	const std::string document = ExtractObject (tpl, "graph");
	ASSERT_FALSE (document.empty ());

	Graph graph;
	const CatalogFactory factory;
	ASSERT_TRUE (LoadGraph (document, factory, graph).IsOk ());

	// Le gabarit designe sa ressource par un file.ref, dont le CHEMIN est
	// serialise. C'est exactement ce que fait l'hote : il pose le chemin, il ne
	// verse pas d'octets. On ne devine pas l'identifiant du noeud -- on le
	// cherche par son type.
	FileRefNode *source = nullptr;
	for (NodeId id = 1; id <= 8 && source == nullptr; ++id)
		source = dynamic_cast<FileRefNode *> (graph.FindNode (id));
	ASSERT_NE (source, nullptr) << "le gabarit n'a pas de noeud file.ref";
	source->SetPath (kFont);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	// Le noeud de sortie est l'extrudeur, en bout de chaine -- id 4 dans le
	// gabarit, et c'est aussi ce que sa cle "output" annonce.
	const EvalResult result = evaluator.Evaluate (4, outputs, ctx);
	ASSERT_EQ (result.status, EvalStatus::Ok) << result.detail;

	ASSERT_FALSE (outputs.empty ());
	const Mesh *mesh = outputs[0].Get<Mesh> (Types ().mesh);
	ASSERT_NE (mesh, nullptr) << "le noeud de sortie ne rend pas un maillage";
	EXPECT_GT (mesh->GetNVertices (), 0u);
	EXPECT_GT (mesh->GetNFaces (), 0u);
}

TEST (TEST_cggraph_template, every_exposed_parameter_of_the_text3d_template_exists_in_its_graph)
{
	// Un widget qui n'ecrit nulle part est le pire des deux mondes : la page
	// s'affiche, le curseur bouge, et rien ne change. template.js le signale a
	// l'execution ; ce cas l'attrape avant.
	//
	// Les noms attendus sont ecrits ICI, en dur, plutot que relus du gabarit :
	// relire les deux cotes du meme fichier ne prouverait que sa coherence avec
	// lui-meme. C'est au CODE qu'on les confronte.
	const std::string tpl = ReadTextFile (kText3d);
	ASSERT_FALSE (tpl.empty ());
	const std::string document = ExtractObject (tpl, "graph");
	ASSERT_FALSE (document.empty ());

	Graph graph;
	const CatalogFactory factory;
	ASSERT_TRUE (LoadGraph (document, factory, graph).IsOk ());

	// Les reglages sont maintenant repartis sur DEUX noeuds, et la repartition
	// est elle-meme ce qu'on verifie : tout ce qui touche a la mise en page reste
	// sur text.contours (id 2), et `depth` a suivi l'extrusion sur shape.extrude
	// (id 4). `unionOverlaps` a disparu des deux -- l'union est desormais
	// inconditionnelle -- et le gabarit ne doit donc plus l'exposer.
	const Node *contours = graph.FindNode (2);
	ASSERT_NE (contours, nullptr);
	const Node *extrude = graph.FindNode (4);
	ASSERT_NE (extrude, nullptr);

	const char *onContours[] = { "text", "align", "size", "letterSpacing",
	                             "lineSpacing", "kerning", "flattenTol",
	                             "centerOnOrigin", "support", "supportMargin",
	                             "supportCornerRadius" };
	const char *onExtrude[] = { "depth" };

	for (const char *name : onContours)
	{
		EXPECT_NE (contours->GetParams ().Find (name), nullptr)
			<< "le gabarit expose \"" << name << "\", que text.contours n'a pas";
		// Et il doit etre PUBLIC : graphNodeInfo omet les parametres internes,
		// donc un widget pose sur un interne lirait null et n'afficherait rien.
		const ParamEntry *entry = contours->GetParams ().FindEntry (name);
		ASSERT_NE (entry, nullptr) << name;
		EXPECT_EQ (entry->visibility, ParamVisibility::Public) << name;
	}
	for (const char *name : onExtrude)
	{
		EXPECT_NE (extrude->GetParams ().Find (name), nullptr)
			<< "le gabarit expose \"" << name << "\", que shape.extrude n'a pas";
		const ParamEntry *entry = extrude->GetParams ().FindEntry (name);
		ASSERT_NE (entry, nullptr) << name;
		EXPECT_EQ (entry->visibility, ParamVisibility::Public) << name;
	}

	// Et le controle qui donne des dents au retrait : `unionOverlaps` n'est plus
	// nulle part. Sans lui, le tableau ci-dessus se contenterait de ne pas le
	// mentionner.
	EXPECT_EQ (contours->GetParams ().Find ("unionOverlaps"), nullptr);
	EXPECT_EQ (extrude->GetParams ().Find ("unionOverlaps"), nullptr);
}


// ---------------------------------------------------------------------------
//  Tous les gabarits, sans exception
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_template, every_shipped_template_loads_against_the_live_catalog)
{
	for (const char *path : kAllTemplates)
	{
		const std::string tpl = ReadTextFile (path);
		ASSERT_FALSE (tpl.empty ()) << path << " introuvable";

		const std::string document = ExtractObject (tpl, "graph");
		ASSERT_FALSE (document.empty ()) << path << " : pas de section \"graph\"";

		Graph graph;
		const CatalogFactory factory;
		const LoadResult result = LoadGraph (document, factory, graph);
		EXPECT_TRUE (result.IsOk ())
			<< path << " : " << ToString (result.status) << " — " << result.detail;
	}
}

TEST (TEST_cggraph_template, every_exposed_parameter_matches_the_type_its_node_declares)
{
	// LE cas qui protege du defaut le plus vicieux de cette mecanique. Le type
	// declare par le gabarit decide du SETTER employe cote JS ; s'il ne
	// correspond pas a celui du noeud, l'ecriture RETYPE le parametre et
	// l'adaptateur -- qui lit par type exact, GetInt exigeant ParamType::Int --
	// retombe sur son defaut. Le curseur bouge, et rien ne change.
	//
	// La panne est MUETTE et facile a ecrire : il suffit de declarer « float » un
	// entier parce qu'on lui a mis un curseur. C'est arrive en ecrivant
	// relief.json.
	const auto typeOf = [] (const std::string &declared) -> ParamType {
		if (declared == "int" || declared == "enum") return ParamType::Int;
		if (declared == "float")                     return ParamType::Float;
		if (declared == "bool")                      return ParamType::Bool;
		return ParamType::String;
	};

	for (const char *path : kAllTemplates)
	{
		const std::string tpl = ReadTextFile (path);
		ASSERT_FALSE (tpl.empty ()) << path;
		const std::string document = ExtractObject (tpl, "graph");
		ASSERT_FALSE (document.empty ()) << path;

		Graph graph;
		const CatalogFactory factory;
		ASSERT_TRUE (LoadGraph (document, factory, graph).IsOk ()) << path;

		// Parcours rudimentaire de la liste "expose" : chaque entree porte un
		// "node", un "param" et un "type". Meme parti pris que ExtractObject --
		// pas d'analyseur JSON dans la cible de test.
		std::size_t cursor = tpl.find ("\"expose\"");
		ASSERT_NE (cursor, std::string::npos) << path << " : pas de section \"expose\"";
		const std::size_t end = tpl.find ("\"graph\"", cursor);

		std::size_t checked = 0;
		while (true)
		{
			const std::size_t at = tpl.find ("\"node\"", cursor);
			if (at == std::string::npos || at >= end) break;
			cursor = at + 6;

			const auto field = [&tpl] (const char *key, std::size_t from, std::size_t stop)
				-> std::string {
				const std::size_t k = tpl.find (key, from);
				if (k == std::string::npos || k >= stop) return std::string ();
				const std::size_t a = tpl.find ('"', tpl.find (':', k)) + 1;
				return tpl.substr (a, tpl.find ('"', a) - a);
			};

			const std::size_t stop = std::min (tpl.find ("\"node\"", cursor), end);
			const int id = std::atoi (tpl.c_str () + tpl.find_first_of ("0123456789", cursor));
			const std::string name = field ("\"param\"", cursor, stop);
			const std::string declared = field ("\"type\"", cursor, stop);
			if (name.empty () || declared.empty ()) continue;

			const Node *node = graph.FindNode ((NodeId)id);
			ASSERT_NE (node, nullptr) << path << " : noeud " << id << " absent";
			const ParamValue *value = node->GetParams ().Find (name);
			ASSERT_NE (value, nullptr)
				<< path << " : le gabarit expose \"" << name << "\", que le noeud n'a pas";
			EXPECT_EQ (value->type, typeOf (declared))
				<< path << " : \"" << name << "\" est declare \"" << declared
				<< "\" mais le noeud le porte d'un autre type -- le reglage serait sans effet";
			++checked;
		}
		EXPECT_GT (checked, 0u) << path << " : aucun parametre expose n'a ete verifie";
	}
}
