#pragma once
//
//  SvgExtrudeColored -- un fichier SVG devient un solide PEINT, en un noeud.
//
// MONOLITHIQUE : contours, marqueterie, extrusion et palette en un seul calcul.
// Une liste plate d'`ExtrudeContour` ne transporte ni la couleur d'une forme, ni
// son rang, ni la frontiere entre deux formes voisines, si bien que le decoupage
// habituel du catalogue -- producteur de contours puis extrudeur partage -- ne
// peut pas porter cette chaine.
//
// `svg.contours` reste en place : ce noeud est une seconde voie, pas une
// migration.
//
// ⚠ `useSvgColors` gouverne DEUX options. Le SVG est un ordre de peintre :
// peindre sans retirer la matiere cachee laisserait deux capots coplanaires et
// superposes -- rendu clignotant, volume compte deux fois. La bascule met donc a
// la fois `perShapeMaterials` et `overlapPolicy` (cf. import_svg.h). L'API C++
// garde les deux separes, la marqueterie ayant un sens sans palette.
//
// Les couleurs sortent par les exports qui portent des materiaux -- OBJ+MTL et
// GLB. Le STL n'en porte pas : le format n'a pas de couleur.
//
#include <atomic>

#include "../../core/node.h"

namespace cggraph_nodes
{

class SvgExtrudeColoredNode : public cggraph::Node
{
public:
	SvgExtrudeColoredNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

	void PublishStats (std::vector<cggraph::NodeStat> &out) const override;

private:
	// Ce que le document PERD ou que le calcul a change.
	std::atomic<unsigned int> m_gradientShapes { 0 };
	std::atomic<unsigned int> m_hiddenShapes { 0 };
	std::atomic<unsigned int> m_subtractedShapes { 0 };
	std::atomic<unsigned int> m_materials { 0 };
};

} // namespace cggraph_nodes
