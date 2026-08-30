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
//     descripteur, qui promeut ses profils en ports (voir gothic_window.h).
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
//  - l'IDENTIFIANT. Parameter n'a pas d'identifiant stable, seulement un nom
//    d'affichage : c'est lui qui sert de cle dans le ParamSet, donc dans le
//    document. Renommer un libelle dans cgmesh casse les documents deja ecrits.
//    C'est le manque que P9 nomme depuis longtemps, et l'adaptateur ne peut
//    que le subir.
//
// ANNULATION : Regenerate() est un appel MONOLITHIQUE, sans boucle externe. Le
// jeton n'a aucun endroit ou entrer, et l'adaptateur n'en fabrique pas un
// faux -- c'est le meme perimetre que les trois noeuds d'analyse monolithiques.
//
#include <memory>
#include <string>
#include <vector>

#include "../../core/node.h"

namespace cggraph_nodes
{

// Liaison entre un type de noeud et une forme du catalogue de cgmesh.
struct ShapeBinding
{
	// Identifiant SERIALISE du type de noeud. Il est ecrit ici, et non derive du
	// nom d'affichage : un libelle se retouche, un identifiant de document non.
	const char *typeName = nullptr;

	// Cle dans ParametricShapes() -- c'est-a-dire GetName() de la forme.
	const char *shapeName = nullptr;

	const char *label = nullptr;
	const char *caveat = nullptr;
};

// Les liaisons publiees par la couche. Adresses stables pour la duree du
// programme.
const std::vector<ShapeBinding> &ShapeBindings ();

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
