#pragma once
//
//  Ce que les trois noeuds de flux partagent -- charger un document, reperer sa
//  frontiere, l'evaluer une fois.
//
// Les trois constructions qui « cassent le DAG simple » (nodal.md §11) sont en
// realite UN seul mecanisme vu trois fois : appliquer un document a des valeurs.
// `flow.subgraph` l'applique une fois, `flow.foreach` une fois par element d'une
// suite, `flow.repeat` n fois en chainant. Rien d'autre ne les distingue, et
// c'est pourquoi le mecanisme vit ici plutot que trois fois.
//
// ---------------------------------------------------------------------------
// COMMENT CELA COMPOSE AVEC UN EVALUATEUR MONO-FIL
// ---------------------------------------------------------------------------
// L'evaluateur refuse toute evaluation reentrante par un statut nomme (Busy) :
// son cache et son memo ne sont pas partageables. Or un noeud de flux calcule
// PENDANT que l'evaluateur exterieur l'evalue. Il ne peut donc pas s'en servir.
//
// Il n'essaie pas. Chaque calcul construit SON graphe et SON evaluateur, sur la
// pile, pour la duree du calcul. L'imbrication est une PILE d'evaluateurs, un
// par niveau, chacun mono-fil et chacun le declarant ; `Busy` n'est jamais
// observe parce qu'aucun evaluateur n'a jamais deux utilisateurs. Le contexte,
// lui, est TRANSMIS tel quel : le jeton d'annulation traverse toute la pile, et
// une annulation posee au sommet interrompt le niveau le plus profond.
//
// Consequence assumee : le graphe interne est reconstruit -- document relu,
// noeuds refabriques -- a chaque calcul du noeud hote. Cela n'arrive que sur un
// DEFAUT de cache du noeud hote ; un hote servi par le cache exterieur ne relit
// rien.
//
// ---------------------------------------------------------------------------
// COMMENT CELA COMPOSE AVEC UN CACHE INDEXE SUR LA SIGNATURE
// ---------------------------------------------------------------------------
// L'evaluateur interne garde son cache pour TOUTE la boucle -- une seule
// instance pour les n passes. C'est voulu, et c'est la moitie utile du cache
// ici : une branche du sous-graphe qui ne depend PAS de l'element (une forme
// constante, un profil, une police) se calcule une fois et sert n fois.
//
// L'autre moitie serait un desastre si rien ne la traitait : une branche qui
// depend de l'element a, d'une passe a l'autre, la MEME topologie et les MEMES
// parametres, donc la meme signature. Le cache servirait le resultat de la
// premiere passe a toutes les autres. C'est le defaut de gravite elevee du
// corpus dans sa forme la plus pure -- un resultat faux, aucun plantage.
//
// Ce qui l'empeche : la frontiere d'entree porte une CLE DE LIAISON, parametre
// semantique reecrit a chaque passe (boundary.h). Elle vaut l'INDICE de la
// passe, et cet indice suffit -- il n'a pas a etre un hachage du contenu de
// l'element, ce que nodal.md §11.1 qualifie a juste titre d'« autre moteur ».
// Il suffit parce que l'evaluateur interne ne SURVIT PAS a la boucle : dans la
// duree de vie de ce cache, il n'existe qu'une suite, et l'indice y designe un
// element sans ambiguite. Un cache interne qui survivrait d'un calcul a l'autre
// rendrait l'indice insuffisant du jour au lendemain, et c'est la seule raison
// pour laquelle il ne survit pas.
//
// ---------------------------------------------------------------------------
// RECURSION
// ---------------------------------------------------------------------------
// Un document qui se reference lui-meme n'est PAS un cycle du DAG : Graph::
// Connect ne le voit pas, parce qu'il n'y a pas d'arete. C'est un cycle dans
// l'espace des DOCUMENTS, ouvert par cette etape et par elle seule. Il
// s'effondrerait des le CHARGEMENT -- charger le document construit le noeud de
// flux, qui apprend sa reference, qui charge le document -- donc la garde est
// posee autour du chargement et non autour du calcul.
//
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../core/evaluator.h"
#include "../../core/graph.h"
#include "../../core/node.h"
#include "../../core/value.h"

namespace cggraph_nodes
{
namespace flow
{

// Une frontiere reperee dans un document charge.
struct Boundary
{
	cggraph::NodeId id = cggraph::kInvalidNodeId;
	int slot = 0;
	std::string name;
};

// Un document charge, sa frontiere ordonnee et ses puits. Non copiable et non
// deplacable, comme le Graph qu'il porte : il se construit sur place.
struct SubgraphInstance
{
	cggraph::Graph graph;
	std::vector<Boundary> inputs;
	std::vector<Boundary> outputs;

	// Noeuds dont le descripteur declare un effet de bord, a QUELQUE profondeur
	// que ce soit : un noeud de flux imbrique publie lui-meme l'effet de bord de
	// son propre document, si bien que la propriete remonte toute seule.
	std::vector<cggraph::NodeId> sinks;
};

enum class LoadStatus
{
	Ok,
	NoReference,
	FileNotReadable,
	DocumentInvalid,
	// La reference est deja en cours de chargement plus haut dans la pile.
	Recursive,
	TooDeep
};

// Profondeur d'imbrication maximale. Elle double la garde exacte par chemin :
// deux orthographes du meme fichier -- lien symbolique, chemin relatif d'un
// repertoire courant different -- ne se reconnaissent pas l'une l'autre, et la
// recursion serait alors bornee par la pile d'appel, c'est-a-dire par rien.
static const std::size_t kMaxSubgraphDepth = 16;

LoadStatus LoadSubgraph (const std::string &reference, SubgraphInstance &instance,
                         std::string &detail);

// Meme chose depuis le TEXTE du document, embarque dans le parent au lieu d'etre
// designe par un chemin.
//
// PAS DE GARDE DE RECURSION ICI, et ce n'est pas un oubli : la garde par chemin
// existe parce qu'un fichier peut se referencer lui-meme, directement ou par un
// tour de plusieurs documents. Un document embarque, lui, est CONTENU dans son
// parent : il ne peut pas le contenir en retour, et sa profondeur est donc celle
// du texte, finie par construction. La borne kMaxSubgraphDepth continue de
// s'appliquer aux chemins qu'il pourrait contenir, la pile etant partagee.
LoadStatus LoadSubgraphFromText (const std::string &document, SubgraphInstance &instance,
                                 std::string &detail);

const char *ToString (LoadStatus status);

// Resultat d'une passe. Le statut de l'evaluateur INTERNE y est rendu tel quel :
// Node::Compute ne sait dire que « vrai » ou « faux », et la cause exacte serait
// perdue a la frontiere sans ce canal.
struct PassResult
{
	cggraph::EvalStatus status = cggraph::EvalStatus::Ok;
	cggraph::NodeId node = cggraph::kInvalidNodeId;
	std::string detail;

	bool IsOk () const { return status == cggraph::EvalStatus::Ok; }
};

// Lie `in` aux frontieres d'entree sous la cle `binding`, tire les frontieres de
// sortie, puis les puits si et seulement si `runSinks`. `out` est dimensionne a
// la frontiere de sortie.
// Chemins que les puits du document ont ecrits depuis leur construction, dans
// l'ordre des noeuds. Un pilote en tete haute nomme les fichiers produits sans
// connaitre le type des puits (node_support.h) ; sans cette collecte, la
// frontiere du sous-graphe les lui cacherait, et « deux executions nomment les
// memes fichiers » ne serait verifiable que depuis le systeme de fichiers.
std::vector<std::string> CollectWritten (const SubgraphInstance &instance);

PassResult RunPass (SubgraphInstance &instance, cggraph::Evaluator &evaluator,
                    cggraph::EvalContext &ctx, const std::string &binding,
                    const std::vector<cggraph::Value> &in, std::vector<cggraph::Value> &out,
                    bool runSinks);

} // namespace flow
} // namespace cggraph_nodes
