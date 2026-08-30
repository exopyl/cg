#pragma once
//
//  Rejeu d'un document en tete haute.
//
// Ce qui pilote un graphe sans interface, et le seul point d'entree du rejeu :
// la logique vit dans une bibliotheque pour qu'une suite de tests puisse
// l'exercer, un executable ne s'appelant pas depuis un test.
//
// ⚠ UNE DEMANDE EXPLICITE EST EXIGEE, et ce n'est pas une commodite d'API. Un
// noeud a effet de bord n'est jamais mis en cache, et « execute seulement sur
// demande explicite » etait jusqu'ici GRATUIT : un puits n'a pas d'aval, donc
// l'evaluation tiree ne l'atteignait que sur demande. Cette propriete tenait au
// fait que la demande venait toujours d'un humain designant un noeud. Un pilote
// qui choisit lui-meme quoi tirer la perd : charger un document et evaluer
// « tout ce qui n'a pas d'aval » ecrirait des fichiers que personne n'a
// demandes. La cible est donc TOUJOURS nommee -- des identifiants, ou le mot
// « tous les puits » qui est lui aussi une designation.
//
#include <string>
#include <vector>

#include "../core/evaluator.h"
#include "../core/graph.h"
#include "../core/serialize.h"

namespace cggraph_nodes
{

enum class RunStatus
{
	Ok,
	LoadFailed,
	NoTarget,
	UnknownTarget,
	IncompatibleDocument,
	EvaluationFailed
};

struct RunRequest
{
	std::string document;

	// Noeuds explicitement designes. Vide et `allSinks` faux : rien n'est
	// evalue, et c'est un refus nomme.
	std::vector<cggraph::NodeId> targets;

	// « Tous les puits » -- les noeuds sans aval. C'est une demande explicite,
	// pas un defaut.
	bool allSinks = false;
};

struct RunReport
{
	RunStatus status = RunStatus::Ok;
	std::string detail;

	std::size_t nodeCount = 0;
	std::size_t linkCount = 0;

	// Noeuds relus sous une autre version de leur type. Le document se LIT --
	// ils sont dans le graphe --, il ne se LANCE pas.
	std::vector<cggraph::NodeId> incompatible;

	// Noeuds sans aval : les cibles qu'une demande pourrait nommer. Remplis meme
	// quand rien n'est evalue, c'est ce qui permet a un refus NoTarget de dire
	// ce qu'il attendait.
	std::vector<cggraph::NodeId> sinks;

	std::vector<cggraph::NodeId> evaluated;

	// Fichiers ecrits par les puits, dans l'ordre des noeuds puis des ecritures.
	std::vector<std::string> written;

	// Statut du premier refus d'evaluation, quand `status` vaut
	// EvaluationFailed. Le detail y nomme le port ou le parametre en cause.
	cggraph::EvalStatus evalStatus = cggraph::EvalStatus::Ok;

	bool IsOk () const { return status == RunStatus::Ok; }
};

// Noeuds sans aval, dans l'ordre des identifiants du graphe.
std::vector<cggraph::NodeId> FindSinks (const cggraph::Graph &graph);

// Charge, verifie, ne calcule rien. C'est ce qu'un editeur demande avant
// d'ouvrir un document.
RunReport Check (const std::string &document);

RunReport Run (const RunRequest &request);

const char *ToString (RunStatus status);

} // namespace cggraph_nodes
