#pragma once
//
//  Frontiere d'un sous-graphe -- les deux noeuds par lesquels un document
//  communique avec le noeud qui le delegue.
//
// Un document de sous-graphe est un document comme un autre : meme format, meme
// fabrique, meme evaluateur. Ce qui en fait un sous-graphe est la presence de
// ces deux types de noeuds, qui n'ont de sens que la : `flow.in` rend une valeur
// que PERSONNE dans le document ne produit, et `flow.out` publie une valeur que
// personne dans le document ne consomme.
//
// PORTS DU NOEUD HOTE. Le parametre `slot` ordonne les frontieres et le
// parametre `nom` nomme le port correspondant du noeud hote. C'est ainsi qu'un
// sous-graphe a « ses propres ports » : le document les declare, l'hote les
// publie. Le port de la frontiere elle-meme, lui, est fixe -- le renommer ne
// dirait rien de plus et casserait les liens internes du document.
//
// ⚠ LA CLE DE LIAISON EST UN PARAMETRE SEMANTIQUE, et c'est le point de
// conception de tout l'edifice. Le cache du sous-graphe est indexe sur la
// signature, comme celui du graphe principal. Une iteration de ForEach ne change
// NI la topologie NI les parametres du document : sans cette cle, les N
// iterations auraient la meme signature et le cache servirait le resultat du
// PREMIER element a tous les autres -- faux, et sans le moindre plantage. La cle
// entre donc dans la signature par la seule voie que le moteur offre a une
// couche de domaine : un parametre declare Semantic.
//
#include <string>

#include "../../core/node.h"
#include "../../core/value.h"

namespace cggraph_nodes
{
namespace flow
{

// Noms serialises. Ils sont ici et nulle part ailleurs : le reperage des
// frontieres dans un document chargé les compare, et une chaine recopiee aurait
// diverge.
extern const char *const kInputTypeName;
extern const char *const kOutputTypeName;

// Nom par defaut du port publie par l'hote pour une frontiere qui n'en nomme
// aucun.
extern const char *const kDefaultPortName;

class GraphInputNode : public cggraph::Node
{
public:
	GraphInputNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

	// Pose la valeur de l'iteration ET la cle qui la distingue. Les deux
	// ensemble, jamais l'une sans l'autre : une valeur posee sans cle serait
	// invisible du cache, une cle posee sans valeur designerait un calcul qui
	// n'a pas eu lieu.
	//
	// Appelee par le noeud hote sur SON exemplaire du document, dont il est seul
	// proprietaire pour la duree d'un calcul. Ce n'est donc pas une ecriture
	// concurrente d'une lecture, contrairement a RefreshExternalState.
	void Bind (const cggraph::Value &value, const std::string &key);

private:
	cggraph::Value m_bound;
};

class GraphOutputNode : public cggraph::Node
{
public:
	GraphOutputNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace flow
} // namespace cggraph_nodes
