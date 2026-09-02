#pragma once
//
//  Extrude -- un contour 2D devient un solide.
//
// L'EXTRUDEUR PARTAGE, et c'est tout ce qu'il fait. Il ne sait pas d'ou viennent
// ses contours : d'un texte, d'un SVG, ou d'autre chose plus tard. C'est le
// point de la decomposition -- extruder un texte et extruder un dessin etaient
// deux fonctions monolithiques qui refaisaient la meme derniere etape.
//
// Le type d'entree, `cgmesh.ExtrudeContours`, existait DEJA dans le catalogue de
// types, avec ce commentaire : « Aucun noeud du catalogue ne les fait transiter
// -- l'extrusion de texte est monolithique ». Ce noeud est celui qu'il attendait.
//
// Ce qu'il ne fait PAS, et qu'il faut savoir :
//
//  - il n'ajoute pas de plaque de support. Le support est un CONTOUR de plus,
//    fondu aux autres par une union 2D, et il se decide donc la ou les contours
//    sont produits -- text.contours le porte. Le generaliser au SVG demanderait
//    de deplacer cette union ici, ce qui est un autre chantier ;
//  - il extrude d'un seul tenant. Les contours arrivent deja resolus en une
//    region unique (les deux producteurs passent par Clipper2), donc NonZero
//    suffit et il n'y a pas d'orientation a renormaliser.
//
#include "../../core/node.h"

namespace cggraph_nodes
{

class ExtrudeNode : public cggraph::Node
{
public:
	ExtrudeNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
