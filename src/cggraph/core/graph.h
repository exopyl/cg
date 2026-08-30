#pragma once
//
//  Graphe : noeuds, liens, et validation a la connexion.
//
// Le graphe ne calcule rien ; il tient la topologie et refuse les topologies
// invalides AU MOMENT de la connexion, jamais plus tard. Trois refus font la
// validite d'un lien : le cycle, l'incompatibilite de types, et la seconde
// source sur une entree deja alimentee.
//
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "node.h"

namespace cggraph
{

using NodeId = std::uint32_t;
using PortIdx = std::uint16_t;

// Les identifiants commencent a 1 : 0 designe l'absence de noeud.
static const NodeId kInvalidNodeId = 0;

struct Link
{
	NodeId from = kInvalidNodeId;
	PortIdx fromPort = 0;
	NodeId to = kInvalidNodeId;
	PortIdx toPort = 0;
};

enum class ConnectStatus
{
	Ok,
	UnknownNode,
	UnknownPort,
	TypeMismatch,
	InputAlreadyConnected,
	Cycle
};

// Statut nomme plutot qu'un booleen : « faux » ne distinguerait pas un noeud
// absent d'une suppression refusee pour une autre raison, et le jour ou une
// seconde raison existe, l'appelant serait deja ecrit.
enum class RemoveStatus
{
	Ok,
	UnknownNode
};

class Graph
{
public:
	// Rend kInvalidNodeId si le noeud est nul. Le graphe en prend la propriete.
	NodeId AddNode (std::unique_ptr<Node> node);

	// Reprend l'identifiant que le document porte, au lieu d'en attribuer un
	// neuf. Un identifiant designe un noeud AILLEURS que dans le graphe --
	// selection d'un editeur, argument d'un pilote en tete haute, journal --, et
	// le renumeroter a chaque relecture rendrait toutes ces references fausses.
	// Rend kInvalidNodeId si le noeud est nul, si l'identifiant est
	// kInvalidNodeId, ou s'il est deja pris.
	NodeId AddNodeWithId (NodeId id, std::unique_ptr<Node> node);

	// Retire le noeud et TOUS les liens qui y touchent, amont comme aval : un
	// lien vers un noeud absent ne serait plus refusable par Connect, donc plus
	// jamais detectable.
	//
	// La suite des identifiants garde un TROU -- 1, 2, _, 4, 5 -- et le compteur
	// ne recule pas. Un identifiant designe un noeud AILLEURS que dans le graphe :
	// selection de l'editeur, document deja ecrit, journal. Renumeroter rendrait
	// ces references fausses en silence, exactement ce que AddNodeWithId existe
	// pour empecher a la relecture.
	RemoveStatus RemoveNode (NodeId id);

	const Node *FindNode (NodeId id) const;
	Node *FindNode (NodeId id);

	std::size_t GetNodeCount () const { return m_nodes.size (); }
	std::vector<NodeId> GetNodeIds () const;

	ConnectStatus Connect (NodeId from, PortIdx fromPort, NodeId to, PortIdx toPort);

	// Une entree porte au plus un lien : elle se libere par son extremite aval.
	bool Disconnect (NodeId to, PortIdx toPort);

	const std::vector<Link> &GetLinks () const { return m_links; }

	// Position d'ecran. Le graphe la porte pour l'editeur et pour la sauvegarde ;
	// elle n'entre dans aucun calcul, et surtout pas dans la signature : deplacer
	// une boite ne doit rien recalculer. Faux si le noeud est inconnu.
	bool SetNodePosition (NodeId id, float x, float y);
	bool GetNodePosition (NodeId id, float &x, float &y) const;

	// Reference de sous-graphe : le document du graphe que ce noeud delegue.
	// AUCUN type de noeud ne l'interprete encore -- le noeud de sous-graphe
	// appartient a une etape ulterieure --, et le graphe la porte quand meme,
	// parce que l'ajouter apres coup toucherait le format de document, donc tous
	// les documents deja ecrits. Elle entre dans la signature : elle designe le
	// calcul, contrairement a la position d'ecran. Chaine vide = ne delegue rien.
	// Faux si le noeud est inconnu.
	bool SetNodeSubgraph (NodeId id, const std::string &reference);
	const std::string &GetNodeSubgraph (NodeId id) const;

	// Lien qui alimente une entree, nullptr si elle est libre.
	const Link *FindInputLink (NodeId to, PortIdx toPort) const;

	// Section critique de la PRE-PASSE d'evaluation -- relevee de l'etat exterieur
	// puis figeage des signatures de la branche. Elle vit ici parce que le graphe
	// est le seul objet que deux evaluateurs concurrents partagent : un verrou
	// porte par l'evaluateur ne les separerait pas.
	//
	// Ce qu'elle protege, et rien d'autre : RefreshExternalState ECRIT dans les
	// parametres d'un noeud source, et la signature les LIT. Sans exclusion, deux
	// evaluations calculent chacune sa signature sur l'identite relevee par
	// l'autre -- le cache annonce alors un resultat qui n'est pas le sien, et il
	// ne plante jamais.
	//
	// Elle ne couvre PAS Compute : c'est la partie chere, et la serialiser
	// annulerait l'interet du fil separe. La partie bon marche est serialisee,
	// le calcul reste concurrent.
	std::unique_lock<std::mutex> LockPrePass () const
	{
		return std::unique_lock<std::mutex> (m_prePass);
	}

	// Sources directes d'un noeud, dans l'ordre de ses ports d'entree ;
	// kInvalidNodeId pour une entree non alimentee.
	std::vector<NodeId> GetUpstream (NodeId id) const;

private:
	// Vrai si `target` est atteignable depuis `origin` en descendant les liens.
	bool IsDownstream (NodeId origin, NodeId target) const;

	struct Slot
	{
		NodeId id = kInvalidNodeId;
		std::unique_ptr<Node> node;
		float x = 0.0f;
		float y = 0.0f;
		std::string subgraph;
	};

	const Slot *FindSlot (NodeId id) const;

	std::vector<Slot> m_nodes;
	std::vector<Link> m_links;
	NodeId m_nextId = 1;

	mutable std::mutex m_prePass;
};

} // namespace cggraph
