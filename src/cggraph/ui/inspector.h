#pragma once
//
//  Inspecteur -- le panneau d'un noeud, derive de son descripteur et de ses
//  parametres.
//
// IL N'Y A PAS DE PANNEAU PAR TYPE DE NOEUD, et il ne doit jamais y en avoir.
// Ce fichier ne nomme aucun type de noeud, aucun parametre, aucun port : il lit
// NodeDesc pour les ports et ParamSet pour les parametres, et un noeud ajoute
// au catalogue est inspectable sans qu'une ligne soit ecrite ici. Un panneau
// ecrit a la main promettrait, au premier nouveau noeud, un travail par noeud
// que le registre existe precisement pour supprimer.
//
// ⚠ CE FICHIER EST LE CONSOMMATEUR DE P4. Un champ pointe DIRECTEMENT dans le
// ParamSet du noeud, en lecture et en ecriture : le widget ecrit dans la valeur
// sans passer par le jeu. C'est ce qui rend l'edition immediate, et c'est aussi
// ce qui rendrait une reallocation du stockage mortelle -- pas perimee, mortelle,
// puisqu'un widget lie ECRIRAIT dans de la memoire liberee. Le stockage est un
// deque (D26) et ne deplace pas ses elements ; la propriete est structurelle et
// ne depend d'aucune assertion a maintenir.
//
// Une seule chose invalide les champs : Clear() sur le jeu, donc la relecture
// d'un document. Reconstruire l'inspecteur apres un chargement n'est pas une
// precaution, c'est une obligation.
//
#include <string>
#include <vector>

#include "../core/graph.h"
#include "../core/param_set.h"
#include "../core/validate.h"
#include "palette.h"

namespace cggraph_ui
{

struct ParamField
{
	std::string name;

	// Literal : `type` designe le champ de `value` que le widget edite.
	// Driven : il n'y a rien a editer qu'une expression, et personne ne sait
	// l'evaluer -- un editeur l'affiche et le dit, il ne la calcule pas.
	cggraph::ParamKind kind = cggraph::ParamKind::Literal;
	cggraph::ParamType type = cggraph::ParamType::Int;

	// Non semantique : le modifier ne recalcule rien. L'ecrire ici permet a un
	// editeur de le signaler au lieu de laisser croire a une invalidation.
	cggraph::ParamRole role = cggraph::ParamRole::Semantic;

	// Adresse dans le ParamSet du noeud. Jamais nulle dans un champ publie.
	cggraph::ParamValue *value = nullptr;
};

struct PortField
{
	cggraph::PortIdx port = 0;
	std::string name;
	const cggraph::TypeDesc *type = nullptr;

	// Toujours Connected pour une sortie : une sortie ne se cable pas, elle est
	// lue ou elle ne l'est pas.
	cggraph::PortState state = cggraph::PortState::Connected;
};

// Le panneau ne publie que les parametres Public : ce qui suit est ce qu'un
// utilisateur regle, jamais la tenue de livre qu'un noeud tient sur lui-meme.
class Inspector
{
public:
	// Reconstruit le panneau pour `id`. Le graphe est pris non const parce que
	// les champs pointent dans les parametres du noeud : un panneau en lecture
	// seule ne serait pas un inspecteur. Vide le panneau si le noeud est inconnu.
	void Build (const Palette &palette, cggraph::Graph &graph, cggraph::NodeId id);

	void Clear ();

	cggraph::NodeId GetNode () const { return m_node; }
	const std::string &GetTypeName () const { return m_typeName; }

	// Libelle du catalogue, ou le nom de type quand la palette ne le connait
	// pas -- un noeud reconstruit hors catalogue reste affichable.
	const std::string &GetLabel () const { return m_label; }

	// Nul si le catalogue n'a rien a signaler sur ce type.
	const char *GetCaveat () const { return m_caveat; }

	cggraph::NodeReadiness GetReadiness () const { return m_readiness; }

	const std::vector<PortField> &GetInputs () const { return m_inputs; }
	const std::vector<PortField> &GetOutputs () const { return m_outputs; }
	const std::vector<ParamField> &GetParams () const { return m_params; }

private:
	cggraph::NodeId m_node = cggraph::kInvalidNodeId;
	std::string m_typeName;
	std::string m_label;
	const char *m_caveat = nullptr;
	cggraph::NodeReadiness m_readiness = cggraph::NodeReadiness::UnknownNode;
	std::vector<PortField> m_inputs;
	std::vector<PortField> m_outputs;
	std::vector<ParamField> m_params;
};

} // namespace cggraph_ui
