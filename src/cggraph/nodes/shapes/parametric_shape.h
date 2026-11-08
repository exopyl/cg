#pragma once
//
//  L'adaptateur GENERIQUE -- IParameterized -> Node, une fois pour toutes.
//
// Un objet parametre de cgmesh expose une liste de Parameter, un Regenerate()
// et un TakeMesh(). C'est exactement ce qu'un noeud sans entree demande, et un
// seul adaptateur suffit donc pour toutes les formes qui tiennent dans ce moule.
//
// ⚠ CE QU'IL N'EMBALLE PAS, et c'est la moitie importante de sa definition :
//
//  1. les formes a RESSOURCE DE CONSTRUCTEUR -- extrusion SVG, quantification
//     d'image, blocs pixelises, texte 3D, surface implicite depuis un nuage. La
//     ressource n'apparait dans AUCUN GetParameters(), si bien que l'adaptateur
//     en ferait un noeud SANS PORT D'ENTREE : non connectable, et surtout
//     faussement mis en cache, sa ressource determinante n'entrant dans aucune
//     signature. Elles exigent un descripteur ecrit a la main, ou la ressource
//     est un PORT. Le catalogue de cgmesh (parametric_catalog.h) ne les contient
//     pas, et c'est la que passe la frontiere -- pas dans un filtre de ce
//     fichier ;
//  2. les formes a SOUS-OBJETS GEOMETRIQUES -- la fenetre gothique. L'adaptateur
//     APLATIT ce qu'il recoit : un profil de moulure y resterait une enumeration
//     a cinq positions, alors que c'est une courbe. Elle a son propre
//     descripteur, qui promeut ses profils en ports (voir archi/gothic_window.h).
//
// CE QUE L'ADAPTATEUR PORTE, parce que le corps emballe ne sait pas le tenir :
//
//  - les BORNES. Parameter porte un min et un max ; ParamSet n'en porte pas.
//    Sans reprise ici, un « Level = 40 » sur l'eponge de Menger partirait
//    directement dans une explosion combinatoire que rien n'arreterait ;
//  - l'ORDRE. Les bornes d'un parametre peuvent dependre d'un autre : celles de
//    « Iterations » du L-systeme sont celles du systeme COURANT. Les valeurs
//    sont donc posees dans l'ordre de declaration, et les bornes relues avant
//    chacune ;
//  - l'IDENTIFIANT. La cle d'un parametre dans le ParamSet, donc dans le
//    document, est Parameter::Id (), et non son libelle GetName (). Un libelle
//    renomme qui garde son identifiant se relit depuis les documents deja
//    ecrits. Il n'y a pas de repli sur le libelle a la lecture : Id () vaut le
//    libelle tant qu'aucun identifiant n'est pose, et un identifiant pose sur
//    un parametre publie prend la valeur de son libelle courant (parameterized.h).
//
// ANNULATION : Regenerate() est un appel MONOLITHIQUE, sans boucle externe. Le
// jeton n'a aucun endroit ou entrer, et l'adaptateur n'en fabrique pas un
// faux -- c'est le meme perimetre que les trois noeuds d'analyse monolithiques.
//
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "../../core/node.h"

class IParameterized;
struct ParametricShapeEntry;

namespace cggraph_nodes
{

// Pose dans `params` la valeur courante de chaque parametre de `shape`, sous
// Parameter::Id ().
void DeclareShapeParams (IParameterized &shape, cggraph::ParamSet &params);

// Ecrit dans `shape` les valeurs de `params` lues sous Parameter::Id (), dans
// l'ordre de declaration, bornes comprises. Un identifiant absent de `params`,
// ou d'un autre type, laisse la valeur de la forme.
void ApplyShapeParams (IParameterized &shape, const cggraph::ParamSet &params);

// Liaison entre un type de noeud et une forme de la reunion CatalogShapes ()
// (shape_catalog.h).
struct ShapeBinding
{
	// La forme emballee, dans CatalogShapes (). Jamais nulle.
	const ParametricShapeEntry *shape = nullptr;

	// Identifiant SERIALISE du type de noeud : shape->id. Champ garde pour ses
	// lecteurs existants (catalog.cpp, tests) ; il n'est ecrit que par
	// ShapeBindings (), depuis le catalogue.
	const char *typeName = nullptr;

	const char *label = nullptr;
	const char *caveat = nullptr;
};

// Les liaisons publiees par la couche, dans l'ordre de leurs annotations.
// Chaque annotation designe sa forme par son identifiant au catalogue ; une
// annotation dont l'identifiant ne s'y trouve pas ne produit pas de liaison, ce
// que ShapeBindings ().size () != ShapeNoteCount () rend visible. Adresses
// stables pour la duree du programme.
const std::vector<ShapeBinding> &ShapeBindings ();

// Nombre d'annotations, resolues ou non.
std::size_t ShapeNoteCount ();

// Nulle si le type est inconnu.
const ShapeBinding *FindShapeBinding (const std::string &typeName);

class ParametricShapeNode : public cggraph::Node
{
public:
	// La liaison doit survivre au noeud ; celles de ShapeBindings() vivent
	// autant que le programme. Le constructeur pose dans le ParamSet les valeurs
	// PAR DEFAUT de la forme, ce qui exige d'en construire une -- donc de la
	// regenerer une fois, son constructeur le faisant.
	explicit ParametricShapeNode (const ShapeBinding &binding);

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

private:
	const ShapeBinding *m_binding;
	const cggraph::NodeDesc *m_desc;
};

} // namespace cggraph_nodes
