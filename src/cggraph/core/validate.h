#pragma once
//
//  Validation AVANT calcul -- quels ports doivent etre cables.
//
// C'est une ENUMERATION, et c'est ce qui la distingue de l'evaluation.
// Evaluate rend le PREMIER port manquant qu'il rencontre en descendant la
// branche, au moment ou il le rencontre, et s'arrete la. Un editeur a besoin de
// l'autre requete : TOUS les ports a la fois, sur un graphe deliberement
// incomplet, pour griser ou signaler chacun d'eux avant que quiconque demande
// un calcul. Les deux lisent la meme source -- PortDesc::optional et le lien
// d'entree --, elles ne rendent pas la meme chose, et l'une ne se deduit pas
// de l'autre.
//
// Rien ici n'evalue et rien n'observe l'exterieur : ni Compute, ni
// RefreshExternalState. Le graphe est pris const, ce qui l'interdit par le type
// et non par la discipline.
//
#include <cstddef>
#include <string>
#include <vector>

#include "graph.h"

namespace cggraph
{

// Etat nomme, et non un booleen : « non cable » ne dit pas s'il faut le cabler.
// Une entree optionnelle libre est un etat NORMAL, une entree obligatoire libre
// est ce qui empeche le calcul ; les confondre ferait griser les deux ou aucune.
enum class PortState
{
	Connected,
	MissingRequired,
	MissingOptional
};

struct InputStatus
{
	PortIdx port = 0;
	std::string name;
	const TypeDesc *type = nullptr;
	PortState state = PortState::Connected;
};

enum class NodeReadiness
{
	Ready,
	MissingRequiredInput,
	UnknownNode
};

struct NodeValidation
{
	NodeId node = kInvalidNodeId;
	NodeReadiness readiness = NodeReadiness::UnknownNode;

	// Une entree par port declare, dans l'ordre de declaration, y compris
	// celles qui sont cablees : un editeur dessine tous les ports, pas
	// seulement ceux qui manquent.
	std::vector<InputStatus> inputs;

	std::size_t GetMissingRequiredCount () const;
};

// Le noeud SEUL. Ne descend pas la branche : un noeud pret dont l'amont ne
// l'est pas rend Ready ici, et c'est voulu -- c'est ce que l'editeur dessine
// autour de cette boite-la.
NodeValidation ValidateNode (const Graph &graph, NodeId id);

struct BranchValidation
{
	// Ready si tous les noeuds de la branche le sont. C'est la question a
	// laquelle repond le bouton « calculer ».
	NodeReadiness readiness = NodeReadiness::UnknownNode;

	// Amont d'abord, la racine en dernier ; un noeud atteint par deux chemins
	// n'y figure qu'une fois. L'ordre est celui du parcours, donc reproductible.
	std::vector<NodeValidation> nodes;

	std::size_t GetMissingRequiredCount () const;
};

// Le noeud ET toute la branche qui l'alimente -- le meme perimetre que
// l'evaluation tiree, sans l'evaluer.
BranchValidation ValidateBranch (const Graph &graph, NodeId id);

} // namespace cggraph
