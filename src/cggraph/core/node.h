#pragma once
//
//  Noeud et description de ses ports.
//
#include <string>
#include <vector>

#include "param_set.h"
#include "value.h"

namespace cggraph
{

using ValueList = std::vector<Value>;

// Defini dans eval_context.h ; un noeud n'en a besoin que pour le transmettre.
class EvalContext;

struct PortDesc
{
	std::string name;
	const TypeDesc *type = nullptr;

	// Une entree optionnelle non alimentee est rendue vide au calcul ; une
	// entree obligatoire non alimentee refuse l'evaluation. Obligatoire est le
	// defaut : un port qui ne dit rien exige d'etre alimente.
	bool optional = false;
};

struct NodeDesc
{
	// Identifiant du type de noeud, serialise ("domaine.Operation").
	std::string typeName;

	// Version du TYPE de noeud, serialisee avec chaque instance. Elle est
	// incrementee des que le sens d'un port ou d'un parametre change, si bien
	// qu'un document ecrit sous une version anterieure se relit -- il reste
	// visible et modifiable -- mais ne se calcule plus. Un document qui se
	// calculerait sous une autre version rendrait faux sans planter.
	int version = 1;

	std::vector<PortDesc> inputs;
	std::vector<PortDesc> outputs;

	// Un noeud a effet de bord agit AILLEURS que dans sa sortie -- il ecrit un
	// fichier, il pilote un peripherique. Il n'est jamais mis en cache : une
	// entree de cache dirait « deja calcule » d'une chose dont le resultat n'est
	// pas la valeur rendue, et l'effet ne se reproduirait pas. Ses entrees, elles,
	// restent mises en cache normalement.
	//
	// « Execute seulement sur demande explicite » est une propriete de
	// l'evaluation TIREE, pas de ce drapeau : un puits n'a pas d'aval, donc rien
	// ne l'evalue sinon la demande qui le designe.
	bool sideEffect = false;
};

// Une mesure publiee par un noeud apres calcul (cf. Node::PublishStats).
struct NodeStat
{
	std::string name;
	double      value = 0.0;
};

class Node
{
public:
	virtual ~Node () = default;

	virtual const NodeDesc &GetDesc () const = 0;

	// Contrat : n'ecrit QUE dans out, ne modifie jamais in. `out` est
	// dimensionne par l'appelant a la taille de desc().outputs.
	virtual bool Compute (EvalContext &ctx, const ValueList &in, ValueList &out) = 0;

	// Reference de sous-graphe que le GRAPHE porte pour ce noeud
	// (Graph::SetNodeSubgraph). Le graphe la transmet ; seul le noeud sait
	// l'interpreter, et la quasi-totalite du catalogue n'en fait rien. Sans cette
	// transmission la reference resterait un champ de document que personne ne
	// lit -- decoratif, donc.
	//
	// Elle arrive AVANT toute connexion et toute evaluation : le chargeur la pose
	// juste apres avoir ajoute le noeud, et avant de poser le moindre lien. Un
	// noeud qui DERIVE ses ports de ce document les publie donc a temps pour que
	// Connect les valide.
	virtual void SetSubgraphReference (const std::string &reference) { (void)reference; }

	// Le pendant embarque : le TEXTE du document delegue, transmis par le
	// graphe au noeud qui sait quoi en faire. Meme raison d'etre que
	// SetSubgraphReference -- le graphe ne connait aucun type de noeud.
	virtual void SetSubgraphDocument (const std::string &document) { (void)document; }

	// Appele sur toute la branche amont AVANT que la signature soit calculee.
	// Un noeud source depend d'un etat exterieur au graphe -- un fichier -- que
	// rien d'autre ne vient relever : sans ce moment, sa signature resterait
	// celle du dernier calcul et le cache resservirait un resultat perime sans
	// jamais planter. Ne fait rien pour un noeud qui ne depend que de ses
	// entrees, c'est-a-dire pour la quasi-totalite du catalogue.
	virtual void RefreshExternalState () {}

	// Ce que le dernier calcul a REELLEMENT fait, pour qui le montre.
	//
	// Plusieurs noeuds comptaient deja leur travail -- glyphes places, morceaux
	// d'une silhouette, formes qu'un profil a mangees -- chacun derriere un
	// accesseur qui lui etait propre. Aucun n'atteignait un ecran : il n'y avait
	// pas de chemin generique entre un compteur de noeud et une interface, si
	// bien qu'un garde-fou pouvait exister dans le moteur et rester invisible la
	// ou il servait. C'est ce chemin.
	//
	// Les noms sont des IDENTIFIANTS STABLES, lus par un gabarit ou un pilote :
	// les changer casse ce qui les affiche, exactement comme un nom de parametre.
	// Ne publie rien pour la quasi-totalite du catalogue.
	virtual void PublishStats (std::vector<NodeStat> &out) const { (void)out; }

	// Version du document dont cette instance est issue. Negative tant que rien
	// ne l'a posee : un noeud construit en memoire est, par construction, a la
	// version de son descripteur. La relecture pose la version LUE et non celle
	// du descripteur, pour deux raisons -- l'evaluateur doit pouvoir refuser, et
	// une re-sauvegarde ne doit pas promouvoir en silence un document que
	// personne n'a migre.
	void SetDocumentVersion (int version) { m_documentVersion = version; }
	int GetDocumentVersion () const
	{
		return m_documentVersion < 0 ? GetDesc ().version : m_documentVersion;
	}

	bool IsVersionCompatible () const { return GetDocumentVersion () == GetDesc ().version; }

	// Les parametres sont portes par le noeud et non par sa description : la
	// description decrit un type de noeud, les parametres une instance.
	ParamSet &GetParams () { return m_params; }
	const ParamSet &GetParams () const { return m_params; }

private:
	ParamSet m_params;
	int m_documentVersion = -1;
};

} // namespace cggraph
