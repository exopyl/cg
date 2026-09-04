#pragma once
//
//  flow.select -- CHOISIR une entree parmi plusieurs, par un reglage.
//
// LE NOEUD QU'UNE PAGE A GRAPHE FIXE RECLAMAIT. Un gabarit ne debranche rien :
// il ne sait que regler des parametres. Or « avec ou sans socle », « plaque
// rectangulaire ou silhouette » sont des VARIANTES, pas des reglages -- et sans
// ce noeud, la seule facon de les offrir etait de laisser le JS reecrire le
// graphe, donc de perdre le document comme source unique. C'est precisement ce
// que l'architecture de gabarits avait coute d'efforts a supprimer.
//
// ⚠ TOUTES LES BRANCHES SONT EVALUEES, y compris celle qu'on ne garde pas.
// L'evaluateur tire ses entrees avant d'appeler Compute ; ce noeud choisit une
// valeur DEJA calculee. Deux consequences a connaitre avant de s'en servir :
//
//   - le cout. Une branche inutilisee est calculee quand meme. Acceptable pour
//     une extrusion de contours deja en cache, pas pour une reconstruction
//     lourde -- pour celle-la, c'est `flow.subgraph` qu'il faut ;
//   - la ROBUSTESSE. Une branche non selectionnee qui ECHOUE fait echouer
//     l'evaluation entiere. Le selecteur ne protege de rien : il ne rattrape pas
//     une panne, il choisit un resultat. Un gabarit doit donc borner ses
//     branches de facon qu'aucune ne puisse echouer, et non compter sur le
//     selecteur pour masquer celle qui le ferait.
//
// PAS DE PORT GENERIQUEMENT TYPE, et ce n'est pas un oubli. Les liens d'un
// document sont valides A LA RELECTURE, avant toute evaluation ; des ports dont
// le type dependrait d'un parametre seraient donc encore a leur type par defaut
// au moment ou le graphe verifie les liens, et un document parfaitement valide
// serait refuse. Le seul hameau ou un noeud peut republier ses ports est
// `RefreshExternalState`, qui n'est appele qu'a l'EVALUATION -- trop tard.
// D'ou une FAMILLE : une variante par type transporte, chacune trois lignes.
//
// TROIS ENTREES, la premiere obligatoire. Trois parce que c'est ce que les
// variantes du texte 3D demandent (aucun socle / plaque rectangulaire / plaque
// silhouette) ; au-dela, deux selecteurs en cascade disent la meme chose sans
// qu'un port de plus soit necessaire. L'entree choisie doit etre alimentee : un
// index qui designe un port vide est une erreur de graphe, pas un defaut a
// combler en silence.
//
#include "../../core/node.h"

namespace cggraph_nodes
{
namespace flow
{

// Le corps commun. Le TYPE transporte est fixe a la construction : c'est ce qui
// permet aux ports d'etre types des la relecture (cf. l'en-tete).
class SelectNode : public cggraph::Node
{
public:
	explicit SelectNode (const cggraph::TypeDesc *type, const char *typeName);

	const cggraph::NodeDesc &GetDesc () const override { return m_desc; }
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

private:
	cggraph::NodeDesc          m_desc;
	const cggraph::TypeDesc   *m_type = nullptr;
};

// Les deux variantes du catalogue. Une de plus est un constructeur de plus.
class SelectMeshNode : public SelectNode
{
public:
	SelectMeshNode ();
};

class SelectContoursNode : public SelectNode
{
public:
	SelectContoursNode ();
};

class SelectProfileNode : public SelectNode
{
public:
	SelectProfileNode ();
};

} // namespace flow
} // namespace cggraph_nodes
